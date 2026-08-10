#include "Horde/GSHordeSubsystem.h"

#include "Horde/GSHordeGoblin.h"
#include "AI/GSAIDebug.h"
#include "Characters/GSCharacterBase.h"
#include "Combat/GSEngagementComponent.h"
#include "Combat/GSGameplayTags.h"
#include "Raid/GSRaidMarker.h"
#include "Raid/GSRaidLibrary.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSHorde, Log, All);

bool UGSHordeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->IsGameWorld();
	}
	return false;
}

void UGSHordeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	ResetPoolForNewRaid();

	// One timer for the whole horde, not one per goblin. See ScanForThreats.
	if (ThreatScanIntervalSeconds > 0.f)
	{
		InWorld.GetTimerManager().SetTimer(ThreatScanTimer, this, &UGSHordeSubsystem::ScanForThreats,
			ThreatScanIntervalSeconds, true);

		UE_LOG(LogGSHorde, Log, TEXT("Threat scan armed: every %.2fs, radius %.0f."),
			ThreatScanIntervalSeconds, AutoThreatRadius);
	}
	else
	{
		UE_LOG(LogGSHorde, Warning,
			TEXT("Threat scan DISABLED (ThreatScanIntervalSeconds = %.2f). The horde will only ever "
			     "retaliate - it will never start a fight."), ThreatScanIntervalSeconds);
	}
}

void UGSHordeSubsystem::Deinitialize()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThreatScanTimer);
	}
	ActiveGoblins.Empty();
	Threats.Empty();
	Super::Deinitialize();
}

void UGSHordeSubsystem::ScanForThreats()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Nothing summoned means nothing to swarm with, and this is the state the timer spends almost
	// all of a raid in. Early-out first so the idle cost of the feature is one map lookup.
	TArray<FVector> Anchors;
	for (const TPair<TWeakObjectPtr<AController>, TArray<TWeakObjectPtr<AGSHordeGoblin>>>& Pair : ActiveGoblins)
	{
		for (const TWeakObjectPtr<AGSHordeGoblin>& Weak : Pair.Value)
		{
			if (const AGSHordeGoblin* Goblin = Weak.Get())
			{
				if (Goblin->IsAlive())
				{
					Anchors.Add(Goblin->GetActorLocation());
				}
			}
		}

		// The summoner is an anchor too, so a defender closing on the player is swarmed by goblins
		// who have not personally got near him yet. This is the "swarmed automatically" half of the
		// Frenzy rule arriving before the first blow rather than after it.
		if (const AController* Summoner = Pair.Key.Get())
		{
			if (const APawn* Pawn = Summoner->GetPawn())
			{
				Anchors.Add(Pawn->GetActorLocation());
			}
		}
	}

	if (Anchors.Num() == 0)
	{
		return;
	}

	const float RadiusSq = AutoThreatRadius * AutoThreatRadius;
	int32 Considered = 0;
	int32 Registered = 0;
	int32 RejectedRace = 0;
	int32 RejectedRange = 0;

	for (TActorIterator<AGSCharacterBase> It(World); It; ++It)
	{
		AGSCharacterBase* Candidate = *It;
		if (!IsValid(Candidate) || !Candidate->IsAlive())
		{
			continue;
		}

		// Goblins and anything with no declared race are not swarm targets. The RaceTag test is not
		// merely "not a goblin": AGSCharacterBase::IsHostileTo treats an unset tag as hostile, which
		// is right for damage and would make BP_GS_TargetDummy a permanent threat here.
		++Considered;

		const FGameplayTag Race = Candidate->GetRaceTag();
		if (!Race.IsValid() || Race == GSTags::Race_Goblin)
		{
			++RejectedRace;
			continue;
		}

		const FVector CandidateLoc = Candidate->GetActorLocation();
		bool bInRange = false;
		for (const FVector& Anchor : Anchors)
		{
			if (FVector::DistSquared(Anchor, CandidateLoc) <= RadiusSq)
			{
				// Idempotent - refreshes an existing entry's timestamp rather than stacking, so
				// re-registering the same guard every 0.5s is exactly how his threat stays alive
				// while he is nearby and expires ThreatMemorySeconds after he leaves.
				RegisterThreat(Candidate, nullptr);
				++Registered;
				bInRange = true;
				break;
			}
		}
		if (!bInRange)
		{
			++RejectedRange;
		}
	}

	// Gated on GS.Combat.LogAI. "The allies do not attack" has four indistinguishable causes from
	// outside - the scan never ran, there were no anchors, every candidate failed the race test, or
	// they were all out of range - and this line separates them in one glance.
	if (GSAIDebug::IsLogging())
	{
		GSAIDebug::Log(nullptr, FString::Printf(
			TEXT("threat scan: %d anchor(s), %d candidate(s) -> %d registered, %d wrong race, "
			     "%d out of range. Threats now %d."),
			Anchors.Num(), Considered, Registered, RejectedRace, RejectedRange, Threats.Num()));
	}
}

UGSHordeSubsystem* UGSHordeSubsystem::Get(const UObject* WorldContextObject)
{
	if (const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr)
	{
		return World->GetSubsystem<UGSHordeSubsystem>();
	}
	return nullptr;
}

void UGSHordeSubsystem::ResetPoolForNewRaid()
{
	ReserveRemaining = GetRaidPoolSize();
	bDryAnnounced = false;
	ActiveGoblins.Empty();
	Threats.Empty();

	UE_LOG(LogGSHorde, Log, TEXT("Pool reset: %d in reserve, cap %d active."),
		ReserveRemaining, ActiveCap);
	BroadcastPoolChanged();
}

int32 UGSHordeSubsystem::GetActiveCount() const
{
	int32 Count = 0;
	for (const TPair<TWeakObjectPtr<AController>, TArray<TWeakObjectPtr<AGSHordeGoblin>>>& Pair : ActiveGoblins)
	{
		for (const TWeakObjectPtr<AGSHordeGoblin>& Goblin : Pair.Value)
		{
			if (Goblin.IsValid())
			{
				++Count;
			}
		}
	}
	return Count;
}

UClass* UGSHordeSubsystem::ResolveGoblinClass() const
{
	if (!HordeGoblinClassPath.IsValid())
	{
		// Deliberately loud. An unset class here is indistinguishable in play from "the horn does
		// nothing", and this project has shipped an unset TObjectPtr twice already (#060, and
		// InteractAction). Say which knob is empty and where it lives.
		UE_LOG(LogGSHorde, Error,
			TEXT("HordeGoblinClassPath is unset - the horn cannot spawn anything. Set it in "
			     "DefaultGame.ini under [/Script/GoblinSiege.GSHordeSubsystem] to BP_HordeGoblin."));
		return nullptr;
	}

	UClass* Resolved = HordeGoblinClassPath.TryLoadClass<AGSHordeGoblin>();
	if (!Resolved)
	{
		UE_LOG(LogGSHorde, Error, TEXT("HordeGoblinClassPath '%s' did not resolve to an AGSHordeGoblin."),
			*HordeGoblinClassPath.ToString());
	}
	return Resolved;
}

bool UGSHordeSubsystem::FindArrivalTransform(AController* Summoner, FTransform& OutTransform) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	TArray<AGSRaidMarker*> Markers;
	AGSRaidMarker::GatherByType(this, GSTags::Marker_HordeArrival, Markers);

	// Reusing AGSRaidMarker rather than adding an AGSHordeSpawnMarker UCLASS is deliberate:
	// GSRaidMarker.h:20-26 argues against a new marker class for exactly this case, and on this
	// machine every new UCLASS is a six-minute editor-closed build. AGENT_STATE.md:160 still lists
	// AGSHordeSpawnMarker as missing work; it is stale architecture, not a work item.
	if (Markers.Num() == 0)
	{
		UE_LOG(LogGSHorde, Warning,
			TEXT("No Marker.HordeArrival markers in this level - the horde has nowhere to arrive from. "
			     "Place an AGSRaidMarker and tag it via GSRaidLibrary::ConfigureMarker."));
		return false;
	}

	// Prefer a marker the summoner is not staring at, so goblins do not blink into existence in the
	// middle of frame. GDD §2.5: "never popping into existence - watching them arrive is the joke".
	AGSRaidMarker* Chosen = Markers[0];
	if (const APawn* SummonerPawn = Summoner ? Summoner->GetPawn() : nullptr)
	{
		const FVector ViewForward = SummonerPawn->GetActorForwardVector();
		const FVector ViewOrigin = SummonerPawn->GetActorLocation();

		float BestScore = TNumericLimits<float>::Max();
		for (AGSRaidMarker* Marker : Markers)
		{
			if (!IsValid(Marker))
			{
				continue;
			}
			const FVector ToMarker = (Marker->GetActorLocation() - ViewOrigin);
			const float Facing = FVector::DotProduct(ViewForward, ToMarker.GetSafeNormal());
			// Lower is better: behind the player (negative facing) and close by.
			const float Score = Facing * 10000.f + ToMarker.Size();
			if (Score < BestScore)
			{
				BestScore = Score;
				Chosen = Marker;
			}
		}
	}

	if (!IsValid(Chosen))
	{
		return false;
	}

	FVector Spot = Chosen->GetActorLocation();

	// Two checks, not one. FindStandableSpotNear is explicitly NOT navmesh projection (see its
	// header - GEN_NavBounds_Village is only 4000x4000 uu), so on its own it will happily return a
	// spot the goblin can stand on and cannot path away from. That failure is silent and reads as
	// "the horde AI is broken" rather than "the navmesh is 80 metres wide".
	FVector Standable;
	if (UGSRaidLibrary::FindStandableSpotNear(this, Spot, Standable, nullptr))
	{
		Spot = Standable;
	}

	OutTransform = FTransform(Chosen->GetActorRotation(), Spot);
	return true;
}

int32 UGSHordeSubsystem::SummonWave(AController* Summoner)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	if (IsPoolDry())
	{
		// The designed end state, not an error: "when the pool is dry, the treeline is silent."
		if (!bDryAnnounced)
		{
			bDryAnnounced = true;
			OnPoolDry.Broadcast();
		}
		UE_LOG(LogGSHorde, Log, TEXT("Horn blown with a dry pool - nothing answers."));
		return 0;
	}

	const int32 Headroom = FMath::Max(0, ActiveCap - GetActiveCount());
	const int32 Wanted = FMath::Min3(SummonsPerBlast, ReserveRemaining, Headroom);
	if (Wanted <= 0)
	{
		UE_LOG(LogGSHorde, Log, TEXT("Horn blown at the active cap (%d/%d) - nothing spawned."),
			GetActiveCount(), ActiveCap);
		return 0;
	}

	UClass* GoblinClass = ResolveGoblinClass();
	if (!GoblinClass)
	{
		return 0;
	}

	FTransform ArrivalTransform;
	if (!FindArrivalTransform(Summoner, ArrivalTransform))
	{
		return 0;
	}

	TArray<TWeakObjectPtr<AGSHordeGoblin>>& Roster = ActiveGoblins.FindOrAdd(Summoner);

	int32 Spawned = 0;
	for (int32 Index = 0; Index < Wanted; ++Index)
	{
		// Fan the arrivals sideways so a blast reads as a line coming in rather than a stack of
		// goblins in one spot fighting their own capsules apart.
		const float Lateral = (static_cast<float>(Index) - (Wanted - 1) * 0.5f) * 140.f;
		FTransform Spawn = ArrivalTransform;
		Spawn.AddToTranslation(ArrivalTransform.GetRotation().GetRightVector() * Lateral);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		Params.Owner = Summoner;

		AGSHordeGoblin* Goblin = World->SpawnActor<AGSHordeGoblin>(GoblinClass, Spawn, Params);
		if (!Goblin)
		{
			continue;
		}

		Roster.Add(Goblin);
		++Spawned;
	}

	if (Spawned > 0)
	{
		// THE ONLY DEBIT IN THIS CLASS (decision 40). Death does not debit again - the goblin was
		// already paid for here - and a delivery credits back against this same counter.
		ReserveRemaining = FMath::Max(0, ReserveRemaining - Spawned);
		UE_LOG(LogGSHorde, Log, TEXT("Horn: %d answered. Reserve %d, active %d/%d."),
			Spawned, ReserveRemaining, GetActiveCount(), ActiveCap);

		if (IsPoolDry() && !bDryAnnounced)
		{
			bDryAnnounced = true;
			OnPoolDry.Broadcast();
		}
		BroadcastPoolChanged();
	}

	return Spawned;
}

AController* UGSHordeSubsystem::RemoveFromActive(AGSHordeGoblin* Goblin)
{
	if (!Goblin)
	{
		return nullptr;
	}

	for (TPair<TWeakObjectPtr<AController>, TArray<TWeakObjectPtr<AGSHordeGoblin>>>& Pair : ActiveGoblins)
	{
		const int32 Removed = Pair.Value.RemoveAll(
			[Goblin](const TWeakObjectPtr<AGSHordeGoblin>& Entry)
			{
				return !Entry.IsValid() || Entry.Get() == Goblin;
			});

		if (Removed > 0)
		{
			return Pair.Key.Get();
		}
	}
	return nullptr;
}

void UGSHordeSubsystem::NotifyGoblinDied(AGSHordeGoblin* Goblin)
{
	RemoveFromActive(Goblin);

	// No credit back, and no second debit. Death is permanent precisely because the spawn already
	// spent this goblin; charging again here would drain a pool twice for one casualty.
	UE_LOG(LogGSHorde, Log, TEXT("Horde goblin died. Reserve %d (unchanged), active %d/%d."),
		ReserveRemaining, GetActiveCount(), ActiveCap);
	BroadcastPoolChanged();
}

void UGSHordeSubsystem::NotifyCourierDelivered(AGSHordeGoblin* Goblin)
{
	RemoveFromActive(Goblin);

	// Per goblin, never per cargo: a rope chain of three prisoners is one goblin coming home.
	// Clamped so a double-delivery bug cannot mint goblins out of nothing.
	ReserveRemaining = FMath::Min(GetRaidPoolSize(), ReserveRemaining + 1);
	bDryAnnounced = false;

	if (IsValid(Goblin))
	{
		Goblin->Destroy();
	}

	UE_LOG(LogGSHorde, Log, TEXT("Courier delivered and rejoined the reserve: %d available."),
		ReserveRemaining);
	BroadcastPoolChanged();
}

void UGSHordeSubsystem::NotifyGoblinSpentOnWarren(AGSHordeGoblin* Goblin)
{
	RemoveFromActive(Goblin);

	// Spent, not dead (§2.6). No death path - a Warren digger must not count as a casualty on the
	// score screen - and no credit back, because it never comes home.
	if (IsValid(Goblin))
	{
		Goblin->Destroy();
	}

	UE_LOG(LogGSHorde, Log, TEXT("A goblin stayed behind to hold the Warren open. Reserve %d, active %d/%d."),
		ReserveRemaining, GetActiveCount(), ActiveCap);
	BroadcastPoolChanged();
}

void UGSHordeSubsystem::BroadcastPoolChanged()
{
	OnHordePoolChanged.Broadcast(ReserveRemaining, GetActiveCount(), ActiveCap);
}

// ---- the stimulus bus ---------------------------------------------------------------------

void UGSHordeSubsystem::RegisterThreat(AActor* Threat, AActor* Provoker)
{
	if (!IsValid(Threat))
	{
		return;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	for (FGSHordeThreat& Existing : Threats)
	{
		if (Existing.Threat.Get() == Threat)
		{
			Existing.LastRefreshedTime = Now;
			Existing.Provoker = Provoker;
			return;
		}
	}

	FGSHordeThreat Added;
	Added.Threat = Threat;
	Added.Provoker = Provoker;
	Added.LastRefreshedTime = Now;
	Threats.Add(Added);
}

void UGSHordeSubsystem::UnregisterThreat(AActor* Threat)
{
	Threats.RemoveAll([Threat](const FGSHordeThreat& Entry)
		{
			return !Entry.Threat.IsValid() || Entry.Threat.Get() == Threat;
		});
}

void UGSHordeSubsystem::PruneStaleThreats()
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	Threats.RemoveAll([this, Now](const FGSHordeThreat& Entry)
		{
			if (!Entry.Threat.IsValid())
			{
				return true;
			}
			// A dead threat stops being one immediately - otherwise the horde keeps swarming a
			// corpse, and MoveTo onto a ragdoll is the AlreadyAtGoal freeze recorded in AGENT_STATE.
			if (const AGSCharacterBase* AsCharacter = Cast<AGSCharacterBase>(Entry.Threat.Get()))
			{
				if (!AsCharacter->IsAlive())
				{
					return true;
				}
			}
			return (Now - Entry.LastRefreshedTime) > ThreatMemorySeconds;
		});
}

AActor* UGSHordeSubsystem::GetAssignedTargetFor(AGSHordeGoblin* Goblin) const
{
	if (!IsValid(Goblin))
	{
		return nullptr;
	}

	const_cast<UGSHordeSubsystem*>(this)->PruneStaleThreats();
	if (Threats.Num() == 0)
	{
		return nullptr;
	}

	// One resolve for the whole crowd, chosen by distance. This is the "central subsystem feeds
	// stimuli" trick from §3.4 in its cheapest honest form: N goblins x M threats of distance maths
	// per query, against N independent sight cones per frame.
	const FVector From = Goblin->GetActorLocation();
	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	for (const FGSHordeThreat& Entry : Threats)
	{
		AActor* Candidate = Entry.Threat.Get();
		if (!IsValid(Candidate))
		{
			continue;
		}

		// CAPACITY. Without this every goblin in the warband picks the same nearest guard and the
		// fight is a queue - the classic conga line. The victim's own component owns the number, so
		// a knight can be worth more attention than a levy without this function knowing why.
		// Halo 3's Objectives system does the same thing and lets the overflow "filter down" to the
		// next task; here the overflow simply falls through to the next-nearest threat below.
		if (const UGSEngagementComponent* Engagement =
				Candidate->FindComponentByClass<UGSEngagementComponent>())
		{
			if (!Engagement->HasEngagementRoom(Goblin))
			{
				continue;
			}
		}

		const float DistSq = FVector::DistSquared(From, Candidate->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	}

	// Book the place. Registering here rather than in the behaviour tree keeps the count honest for
	// the goblins that have not arrived yet - capacity has to mean "on their way" as well as
	// "swinging", or ten goblins all pass the check on the same frame and then converge.
	if (Best)
	{
		if (UGSEngagementComponent* Engagement = Best->FindComponentByClass<UGSEngagementComponent>())
		{
			Engagement->RegisterEngaged(Goblin);
		}
	}

	// Let go of everyone else. A goblin that switched targets must free the slot and the assignment
	// it was holding on its previous one, or the caps drift upward over a long fight and stop
	// meaning anything.
	for (const FGSHordeThreat& Entry : Threats)
	{
		AActor* Candidate = Entry.Threat.Get();
		if (!IsValid(Candidate) || Candidate == Best)
		{
			continue;
		}
		if (UGSEngagementComponent* Engagement = Candidate->FindComponentByClass<UGSEngagementComponent>())
		{
			Engagement->ReleaseAll(Goblin);
		}
	}

	return Best;
}

AActor* UGSHordeSubsystem::GetFollowTargetFor(AGSHordeGoblin* Goblin) const
{
	if (!IsValid(Goblin))
	{
		return nullptr;
	}

	// The summoner this goblin was spawned under, not "player 0" - the TODO the old tick-follow
	// controller left behind. Falls back to player 0 only when the roster has lost its key.
	for (const TPair<TWeakObjectPtr<AController>, TArray<TWeakObjectPtr<AGSHordeGoblin>>>& Pair : ActiveGoblins)
	{
		for (const TWeakObjectPtr<AGSHordeGoblin>& Entry : Pair.Value)
		{
			if (Entry.Get() == Goblin)
			{
				if (const AController* Summoner = Pair.Key.Get())
				{
					if (APawn* Pawn = Summoner->GetPawn())
					{
						return Pawn;
					}
				}
			}
		}
	}

	return UGameplayStatics::GetPlayerPawn(this, 0);
}

int32 UGSHordeSubsystem::GetFollowSlotFor(AGSHordeGoblin* Goblin) const
{
	for (const TPair<TWeakObjectPtr<AController>, TArray<TWeakObjectPtr<AGSHordeGoblin>>>& Pair : ActiveGoblins)
	{
		int32 Slot = 0;
		for (const TWeakObjectPtr<AGSHordeGoblin>& Entry : Pair.Value)
		{
			if (!Entry.IsValid())
			{
				continue;
			}
			if (Entry.Get() == Goblin)
			{
				return Slot;
			}
			++Slot;
		}
	}
	return 0;
}
