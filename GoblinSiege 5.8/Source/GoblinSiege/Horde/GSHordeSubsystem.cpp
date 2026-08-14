#include "Horde/GSHordeSubsystem.h"

#include "Horde/GSHordeGoblin.h"
#include "Horde/GSHordeOrderMarker.h"
#include "AI/GSAIDebug.h"
#include "Characters/GSCharacterBase.h"
#include "Combat/GSEngagementComponent.h"
#include "Combat/GSGameplayTags.h"
#include "Raid/GSRaidMarker.h"
#include "Raid/GSRaidLibrary.h"
#include "Raid/GSRunicSite.h"
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

	// The markers are level actors and the level is going away, so this does not need to destroy
	// them - it needs to stop holding weak handles to actors mid-teardown.
	ActiveOrders.Empty();
	Super::Deinitialize();
}

void UGSHordeSubsystem::ScanForThreats()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Piggy-backed on this timer rather than given its own (#141). A beacon left standing over a dead
	// guard is the visible half of the same staleness the threat registry already prunes here, and
	// GSHordeAIController.h's rule is about per-agent work on the frame - this is one pass over at
	// most one entry per player, twice a second.
	PruneStaleOrders();

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
				RegisterThreat(Candidate);
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

	// A restarted raid must not inherit last raid's beacons. Retire them properly rather than just
	// dropping the map - these are spawned actors, and an abandoned one would sit in the level
	// pointing at a fight that is over.
	for (TPair<TWeakObjectPtr<AController>, FGSHordeOrder>& Pair : ActiveOrders)
	{
		if (AGSHordeOrderMarker* Marker = Pair.Value.Marker.Get())
		{
			Marker->Retire();
		}
	}
	ActiveOrders.Empty();

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

void UGSHordeSubsystem::RegisterThreat(AActor* Threat)
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
			return;
		}
	}

	FGSHordeThreat Added;
	Added.Threat = Threat;
	Added.LastRefreshedTime = Now;
	Threats.Add(Added);
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

	// AN EXPLICIT ORDER OUTRANKS THE THREAT REGISTRY (#141). EGSHordeState has put Commanded above
	// Frenzy since #069; this clause is the line that finally gives that ordering a meaning.
	//
	// Funnelling the ordered victim through TargetActor rather than adding a parallel "ordered target"
	// path is what makes Attack cost almost nothing: AGSAIControllerBase::TickFacing reads TargetActor
	// by hardcoded literal, and BT_HordeGoblin's Block, MeleeAttack and chase branches all gate on it.
	// One assignment buys facing, separation, the attack tokens and the ring geometry unchanged.
	if (const FGSHordeOrder* Order = FindOrderFor(Goblin))
	{
		if (Order->Verb == EGSHordeOrder::Attack)
		{
			AActor* Victim = Order->Subject.Get();
			const AGSCharacterBase* AsCharacter = Cast<AGSCharacterBase>(Victim);
			if (IsValid(Victim) && (!AsCharacter || AsCharacter->IsAlive()))
			{
				// NOT capacity-gated, unlike the ambient path below. HasEngagementRoom exists to stop
				// the whole warband converging on one guard BY ACCIDENT; converging on one guard on
				// purpose is precisely what the player just asked for, and a gate here would quietly
				// send most of them somewhere else while the beacon said otherwise. The ring geometry
				// (MenaceOrbit + TickSeparation) still spaces them and BTDecorator_HasAttackToken still
				// caps how many swing at once, so this is "everyone piles in, four connect" rather
				// than a mob standing inside each other.
				if (UGSEngagementComponent* Engagement = Victim->FindComponentByClass<UGSEngagementComponent>())
				{
					Engagement->RegisterEngaged(Goblin);
				}
				return Victim;
			}
		}
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
	// (The double loop this used to inline is now FindSummonerFor, which the order board also needs.)
	if (const AController* Summoner = FindSummonerFor(Goblin))
	{
		if (APawn* Pawn = Summoner->GetPawn())
		{
			return Pawn;
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

// ---- the order board (#141) ------------------------------------------------------------------
//
// GDD §2.5's point command. The verbs are named on a wheel rather than inferred from the crosshair
// (Michael, 2026-08-12), but the context resolution the GDD describes still happens: Attack on a
// breakable smashes it, Attack on a guard swarms him, and both come from this one order.

AController* UGSHordeSubsystem::FindSummonerFor(const AGSHordeGoblin* Goblin) const
{
	if (!IsValid(Goblin))
	{
		return nullptr;
	}

	for (const TPair<TWeakObjectPtr<AController>, TArray<TWeakObjectPtr<AGSHordeGoblin>>>& Pair : ActiveGoblins)
	{
		for (const TWeakObjectPtr<AGSHordeGoblin>& Entry : Pair.Value)
		{
			if (Entry.Get() == Goblin)
			{
				return Pair.Key.Get();
			}
		}
	}
	return nullptr;
}

const FGSHordeOrder* UGSHordeSubsystem::FindOrderFor(const AGSHordeGoblin* Goblin) const
{
	if (ActiveOrders.Num() == 0)
	{
		// The state a raid spends almost all of its time in. Early-out before the roster walk so an
		// un-commanded horde pays one integer compare per goblin per refresh, not a double loop.
		return nullptr;
	}

	const AController* Summoner = FindSummonerFor(Goblin);
	if (!Summoner)
	{
		return nullptr;
	}

	const FGSHordeOrder* Order = ActiveOrders.Find(Summoner);
	return (Order && Order->Verb != EGSHordeOrder::None) ? Order : nullptr;
}

UClass* UGSHordeSubsystem::ResolveOrderMarkerClass() const
{
	if (!OrderMarkerClassPath.IsValid())
	{
		// Warning, not Error, and the difference is the point: unlike HordeGoblinClassPath, an unset
		// marker class does not stop anything working. The order is issued, the goblins obey, and the
		// player simply has no beacon to look at.
		UE_LOG(LogGSHorde, Warning,
			TEXT("OrderMarkerClassPath is unset - orders will be obeyed but invisible. Set it in "
			     "DefaultGame.ini under [/Script/GoblinSiege.GSHordeSubsystem] to BP_HordeOrderMarker."));
		return nullptr;
	}

	UClass* Resolved = OrderMarkerClassPath.TryLoadClass<AGSHordeOrderMarker>();
	if (!Resolved)
	{
		UE_LOG(LogGSHorde, Warning,
			TEXT("OrderMarkerClassPath '%s' did not resolve to an AGSHordeOrderMarker."),
			*OrderMarkerClassPath.ToString());
	}
	return Resolved;
}

FVector UGSHordeSubsystem::ResolveDeliveryLocation(AController* Summoner, const FVector& From) const
{
	UWorld* World = GetWorld();

	// 1. The stones. This is what §2.7 means by "banks permanently the instant it reaches the runic
	//    site", and NotifyCourierDelivered's own comment names it.
	if (World)
	{
		const AGSRunicSite* BestSite = nullptr;
		float BestDistSq = TNumericLimits<float>::Max();
		for (TActorIterator<AGSRunicSite> It(World); It; ++It)
		{
			const AGSRunicSite* Site = *It;
			if (!IsValid(Site))
			{
				continue;
			}
			const float DistSq = FVector::DistSquared(From, Site->GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				BestSite = Site;
			}
		}
		if (BestSite)
		{
			return BestSite->GetActorLocation();
		}
	}

	// 2. The treeline they came from. Not a design compromise so much as a testing one that happens
	//    to read correctly: L_CombatArena has three Marker.HordeArrival markers and NO runic site, so
	//    without this tier the courier verb could not be demonstrated on the only map where the horn
	//    summons anything at all. "Carries it back the way it came" is a perfectly legible fiction.
	TArray<AGSRaidMarker*> Markers;
	AGSRaidMarker::GatherByType(this, GSTags::Marker_HordeArrival, Markers);

	const AGSRaidMarker* BestMarker = nullptr;
	float BestMarkerDistSq = TNumericLimits<float>::Max();
	for (const AGSRaidMarker* Marker : Markers)
	{
		if (!IsValid(Marker))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(From, Marker->GetActorLocation());
		if (DistSq < BestMarkerDistSq)
		{
			BestMarkerDistSq = DistSq;
			BestMarker = Marker;
		}
	}
	if (BestMarker)
	{
		return BestMarker->GetActorLocation();
	}

	// 3. The summoner. A last resort that keeps the verb from silently doing nothing on a map with
	//    neither - the goblin brings you the sack, which is at least an answer.
	if (Summoner)
	{
		if (const APawn* Pawn = Summoner->GetPawn())
		{
			return Pawn->GetActorLocation();
		}
	}

	return From;
}

void UGSHordeSubsystem::IssueOrder(AController* Summoner, EGSHordeOrder Verb, AActor* Subject, const FVector& Location)
{
	UWorld* World = GetWorld();
	if (!World || !Summoner)
	{
		return;
	}

	// Follow IS the absence of an order, not a fifth kind of one. See the header.
	if (Verb == EGSHordeOrder::None || Verb == EGSHordeOrder::Follow)
	{
		ClearOrder(Summoner);
		return;
	}

	// ATTACK AND LOOT ARE VERBS ABOUT A THING. Refuse one that has no thing.
	//
	// Michael, watching the first build 2026-08-12: "there's no way to give it anything to attack -
	// it says attack the bare ground". Two bugs were stacked there. The trace was the first (fixed in
	// UGSHordeCommandComponent::TraceForOrder). This is the second: a subject-less Attack was accepted,
	// planted a beacon, and was then binned by PruneStaleOrders within half a second - because that
	// function cannot tell "never had a subject" from "its subject just died", and correctly concluded
	// the order was finished. Net effect on screen: the warband got a third of a second of instruction
	// and went back to heel, which reads as the whole feature not working.
	//
	// Refusing at the door is the honest answer rather than inventing a meaning for it. "Attack that
	// patch of grass" is not an order, and a wheel that silently commits one is worse than a wheel
	// that says no.
	if ((Verb == EGSHordeOrder::Attack || Verb == EGSHordeOrder::Loot) && !IsValid(Subject))
	{
		UE_LOG(LogGSHorde, Warning,
			TEXT("Ignored a %s order: it needs something to point at and the trace found bare ground. "
			     "Aim at a defender (Attack) or a carryable (Loot)."),
			*UEnum::GetDisplayValueAsText(Verb).ToString());
		return;
	}

	// Retire the previous beacon before planting a new one. One order per summoner means one marker
	// per summoner; skipping this leaves a trail of stale cones across the hamlet, each one claiming
	// to be current.
	if (FGSHordeOrder* Existing = ActiveOrders.Find(Summoner))
	{
		if (AGSHordeOrderMarker* OldMarker = Existing->Marker.Get())
		{
			OldMarker->Retire();
		}
	}

	// HOLD PLANTS WHERE THE PLAYER IS STANDING, not where he is looking. Michael's call, 2026-08-12:
	// "the default behavior for hold should be wherever the player is currently."
	//
	// It is the right reading of the verb. In play, "hold" almost always means "stop trailing me, stay
	// HERE" - you walk to the doorway you want held and press it - rather than "go to that spot over
	// there", which is a move order nobody asked for. It also makes Hold the one verb that cannot fail
	// to find a sensible point, which matters because it is the verb you reach for when things are
	// going wrong and you want the warband to stop following you into a fight.
	FVector Spot = Location;
	if (Verb == EGSHordeOrder::Hold)
	{
		if (const APawn* SummonerPawn = Summoner->GetPawn())
		{
			Spot = SummonerPawn->GetActorLocation();
		}
	}

	// Ground-correct the point. Same call FindArrivalTransform makes, and the same caveat applies:
	// this is NOT navmesh projection (GEN_NavBounds_Village is only 4000x4000uu), so a marker can
	// legitimately land somewhere the goblins cannot path to. That is not hidden - the beacon is
	// truthful about where the player pointed, and goblins failing to arrive is information.
	FVector Standable;
	if (UGSRaidLibrary::FindStandableSpotNear(this, Spot, Standable, nullptr))
	{
		Spot = Standable;
	}

	FGSHordeOrder Order;
	Order.Verb = Verb;
	Order.Subject = Subject;
	Order.Location = Spot;
	Order.IssuedTime = World->GetTimeSeconds();
	Order.DeliveryLocation = ResolveDeliveryLocation(Summoner, Spot);

	if (UClass* MarkerClass = ResolveOrderMarkerClass())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = Summoner;

		// Lifted clear of the ground so the beacon reads over grass, corpses and the goblins
		// themselves. A marker at ankle height is a marker nobody can see in a crowd.
		const FTransform MarkerTransform(FRotator::ZeroRotator, Spot + FVector(0.f, 0.f, 150.f));

		if (AGSHordeOrderMarker* Marker = World->SpawnActor<AGSHordeOrderMarker>(MarkerClass, MarkerTransform, Params))
		{
			Marker->InitialiseOrder(Verb, Subject, Summoner);
			Order.Marker = Marker;
		}
	}

	ActiveOrders.Add(Summoner, Order);

	// The call GSHordeSubsystem.h:132-133 predicted from the day the stimulus bus was written:
	// "called from the Frenzy hooks ... and from the point command". It is not redundant with the
	// first clause of GetAssignedTargetFor - that clause only helps goblins the order already covers,
	// and this is what lets a goblin summoned AFTER the order still find the victim by the normal
	// path, and what keeps him interesting once the order has expired.
	if (Verb == EGSHordeOrder::Attack && IsValid(Subject))
	{
		RegisterThreat(Subject);
	}

	UE_LOG(LogGSHorde, Log, TEXT("Order: %s at %s%s."),
		*UEnum::GetDisplayValueAsText(Verb).ToString(),
		*Spot.ToCompactString(),
		IsValid(Subject) ? *FString::Printf(TEXT(" on %s"), *GetNameSafe(Subject)) : TEXT(""));

	OnHordeOrderChanged.Broadcast(Verb, Subject, Spot);
}

void UGSHordeSubsystem::ClearOrder(AController* Summoner)
{
	if (!Summoner)
	{
		return;
	}

	FGSHordeOrder* Existing = ActiveOrders.Find(Summoner);
	if (!Existing)
	{
		// Recalling a horde that has no standing order is a no-op, not an error - it is the most
		// natural thing in the world to press Follow twice.
		return;
	}

	if (AGSHordeOrderMarker* Marker = Existing->Marker.Get())
	{
		Marker->Retire();
	}
	ActiveOrders.Remove(Summoner);

	UE_LOG(LogGSHorde, Log, TEXT("Order cleared - the warband is back on Follow."));
	OnHordeOrderChanged.Broadcast(EGSHordeOrder::None, nullptr, FVector::ZeroVector);
}

void UGSHordeSubsystem::PruneStaleOrders()
{
	if (ActiveOrders.Num() == 0)
	{
		return;
	}

	TArray<TWeakObjectPtr<AController>> Expired;

	for (TPair<TWeakObjectPtr<AController>, FGSHordeOrder>& Pair : ActiveOrders)
	{
		const FGSHordeOrder& Order = Pair.Value;

		// The summoner is gone (respawned, disconnected). Nothing left to command.
		if (!Pair.Key.IsValid())
		{
			Expired.Add(Pair.Key);
			continue;
		}

		// An order whose subject has died, been destroyed or been smashed is finished. Hold orders
		// have no subject and never expire this way - they stand until recalled, which is the whole
		// point of a hold.
		const bool bNeedsSubject =
			(Order.Verb == EGSHordeOrder::Attack || Order.Verb == EGSHordeOrder::Loot);

		if (!bNeedsSubject)
		{
			continue;
		}

		const AActor* Subject = Order.Subject.Get();
		bool bFinished = !IsValid(Subject);

		if (!bFinished)
		{
			if (const AGSCharacterBase* AsCharacter = Cast<AGSCharacterBase>(Subject))
			{
				// Same rule PruneStaleThreats applies, and for the same reason: chasing a ragdoll
				// hands every goblin within ~50m an AlreadyAtGoal freeze.
				bFinished = !AsCharacter->IsAlive();
			}
		}

		if (bFinished)
		{
			Expired.Add(Pair.Key);
		}
	}

	for (const TWeakObjectPtr<AController>& Key : Expired)
	{
		if (FGSHordeOrder* Order = ActiveOrders.Find(Key))
		{
			if (AGSHordeOrderMarker* Marker = Order->Marker.Get())
			{
				Marker->Retire();
			}
		}
		ActiveOrders.Remove(Key);

		UE_LOG(LogGSHorde, Log, TEXT("Order expired - its subject is gone. Back to Follow."));
		OnHordeOrderChanged.Broadcast(EGSHordeOrder::None, nullptr, FVector::ZeroVector);
	}
}

EGSHordeOrder UGSHordeSubsystem::GetOrderVerbFor(AGSHordeGoblin* Goblin) const
{
	const FGSHordeOrder* Order = FindOrderFor(Goblin);
	return Order ? Order->Verb : EGSHordeOrder::None;
}

AActor* UGSHordeSubsystem::GetOrderSubjectFor(AGSHordeGoblin* Goblin) const
{
	const FGSHordeOrder* Order = FindOrderFor(Goblin);
	return Order ? Order->Subject.Get() : nullptr;
}

FVector UGSHordeSubsystem::GetOrderLocationFor(AGSHordeGoblin* Goblin) const
{
	const FGSHordeOrder* Order = FindOrderFor(Goblin);
	return Order ? Order->Location : FVector::ZeroVector;
}

FVector UGSHordeSubsystem::GetDeliveryLocationFor(AGSHordeGoblin* Goblin) const
{
	const FGSHordeOrder* Order = FindOrderFor(Goblin);
	return Order ? Order->DeliveryLocation : FVector::ZeroVector;
}

FString UGSHordeSubsystem::DescribeOrders() const
{
	if (ActiveOrders.Num() == 0)
	{
		return TEXT("no standing orders - Follow and Frenzy");
	}

	TArray<FString> Lines;
	for (const TPair<TWeakObjectPtr<AController>, FGSHordeOrder>& Pair : ActiveOrders)
	{
		const FGSHordeOrder& Order = Pair.Value;
		Lines.Add(FString::Printf(TEXT("%s -> %s on %s at %s%s"),
			*GetNameSafe(Pair.Key.Get()),
			*UEnum::GetDisplayValueAsText(Order.Verb).ToString(),
			Order.Subject.IsValid() ? *GetNameSafe(Order.Subject.Get()) : TEXT("(bare ground)"),
			*Order.Location.ToCompactString(),
			// A beacon that failed to spawn is the single most likely reason for "I gave the order and
			// nothing appeared", and it is invisible from every other readout.
			Order.Marker.IsValid() ? TEXT("") : TEXT(" [NO MARKER]")));
	}
	return FString::Join(Lines, TEXT(" | "));
}
