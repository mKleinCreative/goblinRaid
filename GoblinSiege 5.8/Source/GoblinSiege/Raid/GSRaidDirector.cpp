#include "Raid/GSRaidDirector.h"
#include "Destruction/GSBurnObjectiveBase.h"
#include "Destruction/GSTopplableComponent.h"
#include "Combat/GSGameplayTags.h"
#include "Missions/GSMissionObjective.h"
#include "Core/GSGameState.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSRaid, Log, All);

// ====================================================================== lifecycle

UGSRaidDirector::UGSRaidDirector()
{
	// SEEDED HERE, NOT IN DefaultGame.ini, and that is a correction rather than a preference.
	// The ini form was written first and silently parsed to nothing - a TMap keyed by FGameplayTag
	// and a TSet of them are both finicky to express in config, and the failure is invisible: the
	// section loads, the properties stay empty, and the raid quietly keeps the pre-#305 rules. It was
	// caught by reading the CDO back rather than by trusting the file.
	//
	// The properties remain Config, so an ini override still wins if anyone writes a working one.
	// These are the defaults, in native tags that cannot be mistyped.

	// Michael, 2026-08-25: "burn down a percentage of houses". 0.4 of 67 houses is 27, calibrated
	// against #304's fire spread - one well-placed torch completes 23, so 40% needs a second fire or
	// the outliers rather than rewarding a single lucky light.
	TypeCompletionFraction.Add(GSTags::Objective_Burn_House, 0.4f);

	// THE MILL IS REQUIRED AGAIN, and deliberately with no fraction. Michael, 2026-08-25: "Make both
	// windmill 1 and 2 on the inside burnable as objectives, but only make one count as a primary,
	// the other is secondary for points."
	//
	// That is exactly what a type with NO fraction already does. RequiredCount stays 1, so the first
	// windmill to burn satisfies the type and the demotion pass drops its sibling to Optional - still
	// burnable, still scored, no longer on the critical path. Ruling Q-32 survives here precisely
	// because this is the case it was written for: one of a kind, where the player picks which.
	//
	// It was in OptionalTypes for a few hours under ruling 63, when Michael's required set named
	// houses, stalls, statue and field and nothing else. It is out again by his later instruction,
	// and the ledger records both.
}

bool UGSRaidDirector::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	const UWorld* World = Cast<UWorld>(Outer);
	if (!World)
	{
		return false;
	}

	// Deliberately NOT excluding dedicated servers, unlike UGSBurnMaskSubsystem: that subsystem is
	// cosmetic and a server has no renderer, but this one IS the game rules. A server without a
	// raid director has no win condition.
	return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
}

UGSRaidDirector* UGSRaidDirector::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	return World ? World->GetSubsystem<UGSRaidDirector>() : nullptr;
}

bool UGSRaidDirector::HasAuthority() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

void UGSRaidDirector::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// ---- 1. Carriers. Runs on clients too: the roster is what the HUD lists, and each carrier's
	// own list-state is replicated, so a client builds the same list without any of the authority
	// work below.
	for (TActorIterator<AGSBurnObjectiveBase> It(&InWorld); It; ++It)
	{
		RegisterCarrier(*It);
	}

	// ---- 1b. Monuments. A statue is not burned, it is pulled over - so it is swept separately and
	// satisfies its type through OnToppled rather than through the burn delegate.
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		UGSTopplableComponent* Topplable = It->FindComponentByClass<UGSTopplableComponent>();
		if (!Topplable)
		{
			continue;
		}

		const FGameplayTag MonumentType = Topplable->GetObjectiveTypeTag();
		if (!MonumentType.IsValid())
		{
			continue;   // scenery: a monument that counts for nothing is a legitimate thing to place
		}

		MonumentsByType.FindOrAdd(MonumentType).Add(Topplable);

		if (!OptionalTypes.Contains(MonumentType))
		{
			RequiredTypes.Add(MonumentType);
		}

		FGSObjectiveTypeBucket& Bucket = BucketsByType.FindOrAdd(MonumentType);
		const float* MonumentFraction = TypeCompletionFraction.Find(MonumentType);
		const int32 MonumentCount = MonumentsByType[MonumentType].Num();
		Bucket.RequiredCount = MonumentFraction
			? FMath::Max(1, FMath::CeilToInt(*MonumentFraction * MonumentCount))
			: 1;

		Topplable->OnToppled.AddDynamic(this, &UGSRaidDirector::HandleMonumentToppled);

		UE_LOG(LogGSRaid, Log, TEXT("[GoblinSiege] Monument '%s' counts toward %s (%d needed)."),
			*It->GetName(), *MonumentType.ToString(), Bucket.RequiredCount);
	}

	if (RequiredTypes.Num() == 0)
	{
		// Not a warning. A level with no tagged burn carrier cannot be won by any sequence of
		// player actions, and the failure is silent everywhere else - the player burns everything
		// and simply nothing happens. This log line is the whole diagnosis.
		UE_LOG(LogGSRaid, Error,
			TEXT("[GoblinSiege] No tagged burn objectives found in '%s'. THIS RAID CANNOT BE WON. ")
			TEXT("Set ObjectiveTypeTag (Objective.Burn.Field/.Mill/.Market) on each placed burn objective."),
			*InWorld.GetMapName());
	}
	else
	{
		UE_LOG(LogGSRaid, Log, TEXT("[GoblinSiege] Raid starting in '%s': %d carriers across %d types."),
			*InWorld.GetMapName(), TrackedCarriers.Num(), RequiredTypes.Num());
	}

	if (!HasAuthority())
	{
		return;
	}

	// ---- 2. Mission objectives. BeginObjective() has had no caller in C++ since it was written;
	// its own header says "the GameMode (or level Blueprint) begins them at raid start". The
	// director is the third answer, and the only one that needs no per-map wiring.
	if (bAutoBeginMissionObjectives)
	{
		int32 BegunCount = 0;
		for (TActorIterator<AGSMissionObjective> It(&InWorld); It; ++It)
		{
			It->BeginObjective();
			++BegunCount;
		}

		if (BegunCount > 0)
		{
			UE_LOG(LogGSRaid, Log, TEXT("[GoblinSiege] Began %d mission objective(s)."), BegunCount);
		}
	}

	// ---- 3. The clock. AGSGameState::Tick already drives TickRaidClock every 0.1s; it was only
	// ever waiting for someone to start it.
	AGSGameState* GS = InWorld.GetGameState<AGSGameState>();
	if (!GS)
	{
		UE_LOG(LogGSRaid, Error,
			TEXT("[GoblinSiege] No AGSGameState - the raid clock cannot start. Check that this map's ")
			TEXT("GameMode derives from AGSGameMode (it forces GameStateClass)."));
		return;
	}

	GS->OnRaidClockPhaseChanged.AddDynamic(this, &UGSRaidDirector::HandleRaidClockPhaseChanged);

	if (bAutoStartRaidClock)
	{
		GS->StartRaidClock();
	}
}

void UGSRaidDirector::Deinitialize()
{
	// Unbind explicitly rather than trusting teardown order. A carrier can outlive this subsystem
	// during seamless travel, and a dynamic delegate holding a stale UObject* is the kind of bug
	// that only shows up on the second map.
	for (const TWeakObjectPtr<AGSBurnObjectiveBase>& Weak : TrackedCarriers)
	{
		if (AGSBurnObjectiveBase* Carrier = Weak.Get())
		{
			Carrier->OnBurnObjectiveCompleted.RemoveDynamic(this, &UGSRaidDirector::HandleCarrierCompleted);
		}
	}

	if (const UWorld* World = GetWorld())
	{
		if (AGSGameState* GS = World->GetGameState<AGSGameState>())
		{
			GS->OnRaidClockPhaseChanged.RemoveDynamic(this, &UGSRaidDirector::HandleRaidClockPhaseChanged);
		}
	}

	TrackedCarriers.Reset();
	BucketsByType.Reset();
	RequiredTypes.Reset();

	Super::Deinitialize();
}

// ====================================================================== carrier roster

void UGSRaidDirector::RegisterCarrier(AGSBurnObjectiveBase* Carrier)
{
	if (!Carrier)
	{
		return;
	}

	// Idempotent - see the header. A linear scan over a handful of objectives per level beats
	// carrying a second container to make it O(1).
	for (const TWeakObjectPtr<AGSBurnObjectiveBase>& Existing : TrackedCarriers)
	{
		if (Existing.Get() == Carrier)
		{
			return;
		}
	}

	TrackedCarriers.Add(Carrier);

	const FGameplayTag TypeTag = Carrier->GetObjectiveTypeTag();
	if (!TypeTag.IsValid())
	{
		// An untagged carrier is invisible to the demotion pass BY DESIGN (see ObjectiveTypeTag's
		// comment: the safe failure is that it stays Required rather than silently satisfying the
		// win). Safe, but still a broken level - nothing in C++ ever sets this tag, so a designer
		// who forgot gets a map where burning the objective achieves nothing. Say so loudly.
		UE_LOG(LogGSRaid, Error,
			TEXT("[GoblinSiege] Burn objective '%s' has no ObjectiveTypeTag - it can never satisfy ")
			TEXT("the win condition and will stay Required forever. Set it on the placed instance."),
			*Carrier->GetName());
		return;
	}

	FGSObjectiveTypeBucket& Bucket = BucketsByType.FindOrAdd(TypeTag);
	Bucket.Carriers.Add(Carrier);

	// OPTIONAL TYPES REGISTER BUT DO NOT GATE. They still list, still burn and still score - they
	// simply never appear in RequiredTypes, so the portal does not wait on them.
	if (!OptionalTypes.Contains(TypeTag))
	{
		RequiredTypes.Add(TypeTag);
	}

	// How many of this type are needed. Recomputed on every registration because carriers arrive one
	// at a time and the denominator is only correct once they all have - a fraction resolved on the
	// first carrier would demand ceil(0.4 * 1) = 1 house.
	const float* Fraction = TypeCompletionFraction.Find(TypeTag);
	Bucket.RequiredCount = Fraction
		? FMath::Max(1, FMath::CeilToInt(*Fraction * Bucket.Carriers.Num()))
		: 1;

	Carrier->OnBurnObjectiveCompleted.AddDynamic(this, &UGSRaidDirector::HandleCarrierCompleted);

	// A carrier that joins AFTER its type was already satisfied is born Optional. This is the case
	// the Q-37 hook comment says a completion-path implementation could never handle, and the
	// reason registration exists at all rather than a pure sweep.
	if (Bucket.bTypeComplete && !Carrier->IsComplete())
	{
		Carrier->SetListState(EGSObjectiveListState::Optional);
	}

	OnObjectiveRosterChanged.Broadcast();
}

void UGSRaidDirector::GetTrackedCarriers(TArray<AGSBurnObjectiveBase*>& OutCarriers) const
{
	OutCarriers.Reset();
	for (const TWeakObjectPtr<AGSBurnObjectiveBase>& Weak : TrackedCarriers)
	{
		if (AGSBurnObjectiveBase* Carrier = Weak.Get())
		{
			OutCarriers.Add(Carrier);
		}
	}
}

void UGSRaidDirector::GetObjectiveRows(TArray<FGSObjectiveRow>& OutRows) const
{
	OutRows.Reset();
	for (const TWeakObjectPtr<AGSBurnObjectiveBase>& Weak : TrackedCarriers)
	{
		const AGSBurnObjectiveBase* Carrier = Weak.Get();
		if (!Carrier)
		{
			continue;
		}

		FGSObjectiveRow& Row = OutRows.AddDefaulted_GetRef();
		Row.DisplayName = Carrier->GetObjectiveDisplayName();
		Row.ListState = static_cast<uint8>(Carrier->GetListState());
		Row.Completion01 = Carrier->GetCompletion01();
		Row.TypeTag = Carrier->GetObjectiveTypeTag();
	}

	// MONUMENTS TOO. Michael, having played #305: "the statue wasn't listed up there with the
	// objectives on the HUD." It counted toward the win and appeared nowhere, which is the worst
	// combination - a requirement the player cannot see is one they cannot plan around. The list is
	// built from TrackedCarriers, and a monument is not an AGSBurnObjectiveBase, so it was invisible
	// by construction rather than by oversight.
	for (const TPair<FGameplayTag, TArray<TWeakObjectPtr<UGSTopplableComponent>>>& Pair : MonumentsByType)
	{
		for (const TWeakObjectPtr<UGSTopplableComponent>& Weak : Pair.Value)
		{
			const UGSTopplableComponent* Monument = Weak.Get();
			if (!Monument)
			{
				continue;
			}

			FGSObjectiveRow& Row = OutRows.AddDefaulted_GetRef();
			Row.DisplayName = Monument->GetObjectiveDisplayName();

			// A monument is Required until it falls, then Complete. It never becomes Optional: the
			// demotion pass exists so that burning ONE of eleven houses stops nagging you about the
			// other ten, and a statue has no siblings to demote.
			const bool bDown = Monument->IsToppled();
			Row.ListState = static_cast<uint8>(bDown ? EGSObjectiveListState::Complete
														: (OptionalTypes.Contains(Pair.Key)
															? EGSObjectiveListState::Optional
															: EGSObjectiveListState::Required));
			Row.Completion01 = bDown ? 1.f : 0.f;
			Row.TypeTag = Pair.Key;
		}
	}
}

// ====================================================================== the Q-37 pass

void UGSRaidDirector::HandleCarrierCompleted(AGSBurnObjectiveBase* Objective)
{
	// Authority only. Clients learn the same facts through replication: ListState is a replicated
	// property with its own OnRep, and the portal's open state replicates from the runic site. A
	// client running the pass locally would race with those and change nothing.
	if (!Objective || !HasAuthority())
	{
		return;
	}

	const FGameplayTag TypeTag = Objective->GetObjectiveTypeTag();
	if (!TypeTag.IsValid())
	{
		return; // Untagged: invisible to the pass, as specified. Already logged at registration.
	}

	FGSObjectiveTypeBucket* Bucket = BucketsByType.Find(TypeTag);
	if (!Bucket)
	{
		return;
	}

	// COUNT FIRST, THEN DECIDE. Under Q-32 the first carrier flipped the type outright; a fraction
	// needs a tally, and the tally has to survive a carrier being destroyed mid-raid.
	++Bucket->CompletedCount;

	const bool bJustSatisfied = !Bucket->bTypeComplete
		&& Bucket->CompletedCount >= Bucket->RequiredCount;

	UE_LOG(LogGSRaid, Log, TEXT("[GoblinSiege] '%s' burned: %s now %d/%d.%s"),
		*Objective->GetName(), *TypeTag.ToString(),
		Bucket->CompletedCount, Bucket->RequiredCount,
		bJustSatisfied ? TEXT(" TYPE SATISFIED.") : TEXT(""));

	AnnounceObjective(TypeTag, Objective->GetObjectiveDisplayName(), *Bucket, bJustSatisfied);

	if (bJustSatisfied)
	{
		Bucket->bTypeComplete = true;
	}

	if (bJustSatisfied)
	{
		// THE DEMOTION PASS (Q-32, ruled 2026-07-31). The first carrier of a type to burn drops
		// every sibling of that type from Required to Optional - they are still burnable and still
		// worth points, just no longer on the critical path.
		//
		// The market needs no special case, and that is the point of writing it this way: it has
		// exactly one carrier, so this loop simply finds no siblings to demote.
		int32 DemotedCount = 0;
		for (const TWeakObjectPtr<AGSBurnObjectiveBase>& Weak : Bucket->Carriers)
		{
			AGSBurnObjectiveBase* Sibling = Weak.Get();
			if (Sibling && Sibling != Objective)
			{
				// SetListState is authority-only, replicated, and refuses to demote a carrier that
				// is already Complete - so a sibling that burned first keeps its terminal state.
				Sibling->SetListState(EGSObjectiveListState::Optional);
				++DemotedCount;
			}
		}

		if (DemotedCount > 0)
		{
			UE_LOG(LogGSRaid, Log, TEXT("[GoblinSiege] '%s' completed type %s - demoted %d sibling(s) to Optional."),
				*Objective->GetName(), *TypeTag.ToString(), DemotedCount);
		}
	}

	EvaluateWinCondition();
}

void UGSRaidDirector::AnnounceObjective(const FGameplayTag& TypeTag, const FText& DisplayName,
	const FGSObjectiveTypeBucket& Bucket, bool bTypeJustSatisfied)
{
	// The leaf, pluralised the same way the HUD list does it - a fifth type should cost a tag and
	// nothing else.
	FString Leaf = TypeTag.IsValid() ? TypeTag.ToString() : TEXT("Objective");
	int32 Dot = INDEX_NONE;
	if (Leaf.FindLastChar(TEXT('.'), Dot))
	{
		Leaf = Leaf.RightChop(Dot + 1);
	}

	FText Message;
	if (bTypeJustSatisfied)
	{
		Message = FText::Format(
			NSLOCTEXT("GoblinSiege", "ObjectiveTypeDone", "{0} - done"), FText::FromString(Leaf));
	}
	else if (Bucket.RequiredCount > 1)
	{
		// A COUNT ONLY WHEN THERE IS ONE WORTH SHOWING. With RequiredCount 1 the tally is always
		// "1/1" and says nothing; with 27 houses it is the whole information.
		Message = FText::Format(
			NSLOCTEXT("GoblinSiege", "ObjectiveTally", "{0}  {1}/{2}"),
			FText::FromString(Leaf), FText::AsNumber(Bucket.CompletedCount),
			FText::AsNumber(Bucket.RequiredCount));
	}
	else
	{
		Message = FText::Format(
			NSLOCTEXT("GoblinSiege", "ObjectiveDone", "{0} destroyed"),
			DisplayName.IsEmpty() ? FText::FromString(Leaf) : DisplayName);
	}

	OnObjectiveAnnounced.Broadcast(Message);
}

void UGSRaidDirector::EvaluateWinCondition()
{
	if (bObjectivesComplete || !HasAuthority())
	{
		return;
	}

	// An empty roster must never read as "all types complete" - a level with nothing placed would
	// otherwise win itself on the first frame.
	if (RequiredTypes.Num() == 0)
	{
		return;
	}

	for (const FGameplayTag& Tag : RequiredTypes)
	{
		const FGSObjectiveTypeBucket* Bucket = BucketsByType.Find(Tag);
		if (!Bucket || !Bucket->bTypeComplete)
		{
			return;
		}
	}

	bObjectivesComplete = true;

	UE_LOG(LogGSRaid, Log,
		TEXT("[GoblinSiege] All %d objective type(s) burned - the portal opens."), RequiredTypes.Num());

	// Michael's own phrasing for the win condition was about this moment: "the prompt to leave
	// doesn't come back unless you've completed those missions". This is that prompt.
	OnObjectiveAnnounced.Broadcast(
		NSLOCTEXT("GoblinSiege", "WayOutOpen", "The way out is open"));

	OnRaidObjectivesComplete.Broadcast();
}

void UGSRaidDirector::HandleMonumentToppled(AActor* /*Toppler*/)
{
	if (!HasAuthority())
	{
		return;
	}

	// RECOUNT rather than increment. FGSOnToppled carries the TOPPLER, not the monument, so this
	// broadcast cannot say which statue it came from - and a blind ++ would double-count if two
	// monuments of a type fell, or miscount after a GS.Topple.ResetAll.
	for (const TPair<FGameplayTag, TArray<TWeakObjectPtr<UGSTopplableComponent>>>& Pair : MonumentsByType)
	{
		FGSObjectiveTypeBucket* Bucket = BucketsByType.Find(Pair.Key);
		if (!Bucket)
		{
			continue;
		}

		int32 Down = 0;
		for (const TWeakObjectPtr<UGSTopplableComponent>& Weak : Pair.Value)
		{
			if (const UGSTopplableComponent* Monument = Weak.Get())
			{
				Down += Monument->IsToppled() ? 1 : 0;
			}
		}

		if (Down == Bucket->CompletedCount)
		{
			continue;
		}
		Bucket->CompletedCount = Down;

		if (!Bucket->bTypeComplete && Down >= Bucket->RequiredCount)
		{
			Bucket->bTypeComplete = true;
			UE_LOG(LogGSRaid, Log,
				TEXT("[GoblinSiege] %s satisfied - %d of %d monument(s) cast down."),
				*Pair.Key.ToString(), Down, Bucket->RequiredCount);
		}

		// Announce whichever monument of this type is down and has a name. With one statue this is
		// simply "the statue"; with several it names the type, which is the honest thing to say when
		// the broadcast cannot tell us which one fell.
		FText MonumentName;
		for (const TWeakObjectPtr<UGSTopplableComponent>& Weak : Pair.Value)
		{
			if (const UGSTopplableComponent* M = Weak.Get())
			{
				MonumentName = M->GetObjectiveDisplayName();
				break;
			}
		}
		AnnounceObjective(Pair.Key, MonumentName, *Bucket, Bucket->bTypeComplete);
	}

	EvaluateWinCondition();
}

int32 UGSRaidDirector::GetRequiredCountForType(FGameplayTag TypeTag) const
{
	const FGSObjectiveTypeBucket* Bucket = BucketsByType.Find(TypeTag);
	return Bucket ? Bucket->RequiredCount : 0;
}

int32 UGSRaidDirector::GetCompletedTypeCount() const
{
	int32 Count = 0;
	for (const FGameplayTag& Tag : RequiredTypes)
	{
		const FGSObjectiveTypeBucket* Bucket = BucketsByType.Find(Tag);
		if (Bucket && Bucket->bTypeComplete)
		{
			++Count;
		}
	}
	return Count;
}

// ====================================================================== raid end

void UGSRaidDirector::HandleRaidClockPhaseChanged(EGSRaidClockPhase NewPhase)
{
	// GSGameState.cpp's own comment says "GameMode listens for Expired". Nothing did. This is that
	// listener - Expired means the 90s collapse grace is gone, not merely that 0:00 was reached.
	if (NewPhase == EGSRaidClockPhase::Expired)
	{
		EndRaid(EGSRaidResult::LeftBehind);
	}
}

void UGSRaidDirector::EndRaid(EGSRaidResult Result)
{
	if (!HasAuthority() || Result == EGSRaidResult::NotEnded)
	{
		return;
	}

	// Terminal, first-call-wins. See the header: a goblin who spends their last life while
	// stepping through an open portal has extracted.
	if (RaidResult != EGSRaidResult::NotEnded)
	{
		return;
	}

	RaidResult = Result;

	const UEnum* ResultEnum = StaticEnum<EGSRaidResult>();
	UE_LOG(LogGSRaid, Log, TEXT("[GoblinSiege] RAID ENDED: %s (%d/%d objective types burned)."),
		ResultEnum ? *ResultEnum->GetNameStringByValue(static_cast<int64>(Result)) : TEXT("?"),
		GetCompletedTypeCount(), RequiredTypes.Num());

	// Time stops when the raid does. Before #049 it did not: a raid lost to OutOfLives kept counting
	// down and would eventually enter the collapse phase it had already lost the right to.
	if (const UWorld* World = GetWorld())
	{
		if (AGSGameState* GS = World->GetGameState<AGSGameState>())
		{
			GS->StopRaidClock();
		}
	}

	OnRaidEnded.Broadcast(Result);
}
