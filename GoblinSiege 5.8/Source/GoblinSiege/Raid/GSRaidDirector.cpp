#include "Raid/GSRaidDirector.h"
#include "Destruction/GSBurnObjectiveBase.h"
#include "Missions/GSMissionObjective.h"
#include "Core/GSGameState.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSRaid, Log, All);

// ====================================================================== lifecycle

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
	RequiredTypes.Add(TypeTag);

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

	const bool bFirstOfType = !Bucket->bTypeComplete;
	Bucket->bTypeComplete = true;

	if (bFirstOfType)
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

	OnRaidObjectivesComplete.Broadcast();
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
