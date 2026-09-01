#include "Horde/GSHordeSubsystem.h"

#include "Horde/GSHordeGoblin.h"
#include "Horde/GSHordeOrderMarker.h"
#include "AI/GSAIDebug.h"
#include "Characters/GSCharacterBase.h"
#include "Combat/GSEngagementComponent.h"
#include "Combat/GSGameplayTags.h"
#include "Destruction/GSBreakableComponent.h"
#include "Interaction/GSInteractableComponent.h"
#include "Raid/GSRaidMarker.h"
#include "Raid/GSRaidLibrary.h"
#include "Raid/GSRunicSite.h"
#include "Raid/GSWarren.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "CollisionShape.h"
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

	// THE WARREN FIRST (ledger ruling 18). Goblins climb out of the hole; the bare marker is what
	// they used before there was a hole to climb out of.
	//
	// The marker path below is NOT dead code and must not be deleted. Every map predating this
	// ticket - L_CombatArena included - answers the horn through it, and AGSHordeSpawnMarker was
	// ruled never to be built precisely so Marker.HordeArrival stays the general answer.
	const FVector SummonerLocation = (Summoner && Summoner->GetPawn())
		? Summoner->GetPawn()->GetActorLocation()
		: FVector::ZeroVector;

	if (const AGSWarren* Warren = AGSWarren::FindNearestArrivalMouth(this, SummonerLocation))
	{
		OutTransform = Warren->GetArrivalTransform();
		UE_LOG(LogGSHorde, Verbose, TEXT("Arrival: the Warren '%s'."), *Warren->GetName());
		return true;
	}

	// THEN THE GATE. Michael's ruling, 2026-08-21: "horn blasts working from the gate until a warren
	// is down on the map." Before a Warren is planted the horde comes out of the portal the raid
	// arrived through, which is somewhere the player has actually been and can find again - rather
	// than a treeline marker they have never seen.
	//
	// GetSpawnTransform() and not the actor transform: the runic site already solves "a standable
	// spot outside the extraction sphere, traced against the world", which is the same question
	// asked here and was got wrong once already (spawning inside a building, 2026-08-05).
	{
		const AGSRunicSite* Nearest = nullptr;
		float BestDistSq = TNumericLimits<float>::Max();
		for (TActorIterator<AGSRunicSite> It(World); It; ++It)
		{
			const AGSRunicSite* Site = *It;
			if (!IsValid(Site))
			{
				continue;
			}
			const float DistSq = FVector::DistSquared(Site->GetActorLocation(), SummonerLocation);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Nearest = Site;
			}
		}
		if (Nearest)
		{
			OutTransform = Nearest->GetSpawnTransform();
			UE_LOG(LogGSHorde, Verbose, TEXT("Arrival: the gate '%s' - no Warren planted yet."),
				*Nearest->GetName());
			return true;
		}
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

	// Logged for the same reason as the two branches above: until now nothing said WHICH source the
	// horde came out of, and "the goblins arrived from the wrong place" was unanswerable from a log.
	// Verbose, so it costs nothing until someone asks.
	//
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

	UE_LOG(LogGSHorde, Verbose, TEXT("Arrival: the marker '%s' - no Warren and no gate."),
		*Chosen->GetName());

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

int32 UGSHordeSubsystem::GetSummonableNow() const
{
	const int32 Headroom = FMath::Max(0, ActiveCap - GetActiveCount());
	return FMath::Min(ReserveRemaining, Headroom);
}

int32 UGSHordeSubsystem::SummonOne(AController* Summoner)
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
		return 0;
	}

	if (GetSummonableNow() <= 0)
	{
		UE_LOG(LogGSHorde, Verbose, TEXT("Horn held at the active cap (%d/%d) - nothing more climbs out."),
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

	// Rotate the sideways offset through a five-wide line at the mouth. Goblins now emerge ONE AT A
	// TIME rather than as a fanned blast, so the old centre-the-line-on-the-marker maths no longer
	// has a line to centre - but two climbing out on the same tick still need somewhere to put their
	// capsules. Deterministic on purpose: a random bearing makes an arrival that cannot be reproduced.
	const float Lateral = (static_cast<float>(SpawnOrdinal % 5) - 2.f) * 140.f;
	FTransform Spawn = ArrivalTransform;
	Spawn.AddToTranslation(ArrivalTransform.GetRotation().GetRightVector() * Lateral);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.Owner = Summoner;

	AGSHordeGoblin* Goblin = World->SpawnActor<AGSHordeGoblin>(GoblinClass, Spawn, Params);
	if (!Goblin)
	{
		return 0;
	}

	++SpawnOrdinal;

	// ---- THE FOLLOW SLOT IS ASSIGNED HERE, ONCE, AND NEVER RECOMPUTED (#239) ------------------
	// Lowest slot no living sibling already holds, so a casualty leaves a gap that the next summon
	// fills rather than the whole horde shuffling forward. See AGSHordeGoblin::GetFollowSlot.
	{
		TArray<TWeakObjectPtr<AGSHordeGoblin>>& Roster = ActiveGoblins.FindOrAdd(Summoner);

		TSet<int32> Taken;
		for (const TWeakObjectPtr<AGSHordeGoblin>& Entry : Roster)
		{
			if (Entry.IsValid() && Entry->GetFollowSlot() != INDEX_NONE)
			{
				Taken.Add(Entry->GetFollowSlot());
			}
		}

		int32 Slot = 0;
		while (Taken.Contains(Slot))
		{
			++Slot;
		}
		Goblin->SetFollowSlot(Slot);

		Roster.Add(Goblin);
	}

	// THE ONLY DEBIT IN THIS CLASS (decision 40). Death does not debit again - the goblin was
	// already paid for here - and a delivery credits back against this same counter.
	ReserveRemaining = FMath::Max(0, ReserveRemaining - 1);

	UE_LOG(LogGSHorde, Log, TEXT("A goblin answers. Reserve %d, active %d/%d."),
		ReserveRemaining, GetActiveCount(), ActiveCap);

	if (IsPoolDry() && !bDryAnnounced)
	{
		bDryAnnounced = true;
		OnPoolDry.Broadcast();
	}
	BroadcastPoolChanged();

	return 1;
}

int32 UGSHordeSubsystem::SummonWave(AController* Summoner)
{
	// Kept as the batch entry point for GS.Horde.SpawnTest and for anything that wants a whole
	// blast at once. The player's horn no longer comes through here - it streams SummonOne on a
	// timer while the button is held (UGSGA_Horn) - so this is no longer the summon path a designer
	// tuning feel should reach for.
	int32 Spawned = 0;
	for (int32 Index = 0; Index < SummonsPerBlast; ++Index)
	{
		if (SummonOne(Summoner) == 0)
		{
			break;
		}
		++Spawned;
	}

	if (Spawned == 0)
	{
		UE_LOG(LogGSHorde, Log, TEXT("Horn blown and nothing answered - reserve %d, active %d/%d."),
			ReserveRemaining, GetActiveCount(), ActiveCap);
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
				// SPILL-OVER (#221, Michael 2026-08-21: "are they focusing on another enemy if one is
				// over saturated? that's how I would love them to work").
				//
				// This REFINES ruling 34 rather than reversing it. The order still converges the
				// warband on the named victim - that is what the player asked for and it is why
				// nothing here caps the FIRST six. But past capacity, the surplus used to pile onto
				// a target with no place left for them: measured 2026-08-21 as "engaged 10/6 OVER
				// ENGAGED" with four goblins parked on the outer ring watching six fight.
				//
				// Now the surplus falls through to the ambient path below, which already skips any
				// candidate without engagement room and takes the nearest that has some. So the
				// order reads "kill that one, and if there is no room left, kill what is next to
				// you" - which is what a warband does.
				//
				// An agent ALREADY registered on the victim passes HasEngagementRoom (see
				// UGSEngagementComponent::HasEngagementRoom), so incumbents are never displaced by
				// this and there is no rotation on every scan.
				// ---- THE ORDER IS A PLACE, NOT A PERSON (#264) ---------------------------------
				//
				// Michael: "have them move to the location, then sample in front of them if there's
				// anything to attack, then attack what's around them rather than a specific item."
				//
				// The warband still converges where you pointed, because the anchor below is the
				// ORDER LOCATION and that is where the named victim was standing. What changes is
				// what each goblin does once it gets there: it takes whatever is nearest to IT with
				// room, instead of queueing for a place on one body while a guard stands beside it
				// unhit.
				//
				// Swept DIRECTLY off the world rather than through the Threats registry, and that is
				// the point. The registry only knows a guard once a goblin or the summoner has been
				// within AutoThreatRadius of him during a 0.5s scan, so the bystanders around the
				// marked victim are learned about strictly after the warband arrives - which is
				// exactly the "takes them a bit to realise there's people to engage" that Michael
				// described. A local sweep has no such lag.
				const FVector Anchor = Order->Location;
				const FVector From = Goblin->GetActorLocation();
				const float AnchorRadiusSq = OrderEngageRadius * OrderEngageRadius;

				AActor* BestWithRoom = nullptr;
				float BestWithRoomSq = TNumericLimits<float>::Max();
				AActor* BestAny = nullptr;
				float BestAnySq = TNumericLimits<float>::Max();

				if (UWorld* World = GetWorld())
				{
					for (TActorIterator<AGSCharacterBase> It(World); It; ++It)
					{
						AGSCharacterBase* Candidate = *It;
						if (!IsValid(Candidate) || !Candidate->IsAlive())
						{
							continue;
						}

						// Same race test the threat sweep uses, and for the same reason: an unset
						// RaceTag reads as hostile to IsHostileTo, which would make a target dummy
						// a valid order victim.
						const FGameplayTag Race = Candidate->GetRaceTag();
						if (!Race.IsValid() || Race == GSTags::Race_Goblin)
						{
							continue;
						}

						// Bounded to the ordered area so a warband cannot wander into a different
						// fight it happens to be able to see.
						if (FVector::DistSquared(Anchor, Candidate->GetActorLocation()) > AnchorRadiusSq)
						{
							continue;
						}

						// Nearest to the GOBLIN, not to the anchor: while it is still walking in,
						// everything in the area is roughly equidistant and it heads for the group;
						// once it arrives, this is literally "what is next to me".
						const float DistSq = FVector::DistSquared(From, Candidate->GetActorLocation());
						if (DistSq < BestAnySq)
						{
							BestAnySq = DistSq;
							BestAny = Candidate;
						}

						const UGSEngagementComponent* Engagement =
							Candidate->FindComponentByClass<UGSEngagementComponent>();
						if (!Engagement || Engagement->HasEngagementRoom(Goblin))
						{
							if (DistSq < BestWithRoomSq)
							{
								BestWithRoomSq = DistSq;
								BestWithRoom = Candidate;
							}
						}
					}
				}

				// Prefer somewhere there is room; fall back to the nearest regardless, then to the
				// named victim. The last fallback matters: if the sweep finds nothing - the order
				// was placed on empty ground, or everyone in the area is already dead - the goblin
				// keeps the victim the player actually named rather than going idle.
				AActor* Chosen = BestWithRoom ? BestWithRoom : (BestAny ? BestAny : Victim);
				if (IsValid(Chosen))
				{
					if (UGSEngagementComponent* ChosenEngagement =
							Chosen->FindComponentByClass<UGSEngagementComponent>())
					{
						ChosenEngagement->RegisterEngaged(Goblin);
					}
					return Chosen;
				}
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

FVector UGSHordeSubsystem::GetFollowPostFor(AGSHordeGoblin* Goblin) const
{
	AActor* Leader = GetFollowTargetFor(Goblin);
	if (!Leader || !IsValid(Goblin))
	{
		return Goblin ? Goblin->GetActorLocation() : FVector::ZeroVector;
	}

	const int32 Slot = FMath::Max(0, GetFollowSlotFor(Goblin));
	const int32 PerRank = FMath::Max(1, FollowPostPerRank);
	const int32 Rank = Slot / PerRank;
	const int32 File = Slot % PerRank;

	// Centre the rank on the summoner's spine: with three per rank the files sit at -1, 0, +1.
	const float FileOffset = (static_cast<float>(File) - (PerRank - 1) * 0.5f) * FollowPostFileSpacing;
	const float Depth = FollowPostDepth + Rank * FollowPostRankSpacing;

	FVector Forward = Leader->GetActorForwardVector();
	Forward.Z = 0.f;
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}
	Forward.Normalize();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();

	// BEHIND the leader - minus forward. The whole point is that the band reads as following him.
	return Leader->GetActorLocation() - Forward * Depth + Right * FileOffset;
}

int32 UGSHordeSubsystem::GetFollowSlotFor(AGSHordeGoblin* Goblin) const
{
	// Was: the goblin's INDEX in the roster array, counted fresh on every call and skipping dead
	// entries. RemoveFromActive compacts that array, so one death renumbered every goblin behind the
	// casualty and the whole formation jostled forward together. The slot now lives on the goblin.
	if (IsValid(Goblin) && Goblin->GetFollowSlot() != INDEX_NONE)
	{
		return Goblin->GetFollowSlot();
	}

	// Slot 0 for a goblin the roster has never seen. Same fallback as before, and it means an
	// unregistered goblin stacks on the summoner rather than vanishing to a formation position that
	// does not exist.
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

bool UGSHordeSubsystem::IsStillLootable(const AActor* Candidate) const
{
	if (!IsValid(Candidate))
	{
		return false;
	}

	// THE SAME TWO TESTS UGSHordeCommandComponent::ResolveOrderSubject applies when the player aims
	// directly at something (a carryable, OR an unbroken breakable container) - not narrowed to
	// carryable alone. A direct aim at a crate already resolved it as a valid Subject before this
	// search existed; excluding crates HERE just meant the area search silently ignored the exact
	// thing Michael pointed near and only worked if he landed the narrow aim trace precisely on it,
	// which is the "loot whatever's nearby, which they're not right now" bug (2026-08-30).
	const UGSInteractableComponent* Interactable = Candidate->FindComponentByClass<UGSInteractableComponent>();
	const bool bIsCarryableHit = Interactable && Interactable->IsCarryable();

	// NOT "unbroken" alone - a real regression caught 2026-08-30 night: the instant
	// UBTTask_SmashOrderTarget broke a barrel open, this went from true to false (the barrel is no
	// longer unbroken), GetOrderSubjectFor dropped the goblin's own claim mid-sequence, OrderVerb
	// degraded to None via the revert-to-Follow fix, and UBTTask_LootInPlace never got a chance to
	// run - a goblin visibly smashed its target and then just stopped, with no loot ever collected.
	// A container is still legitimately "in progress" for the goblin currently working it either
	// BEFORE it is broken (still needs smashing) or AFTER, for as long as it remains available (not
	// yet looted - LootInPlace's own CompleteInteraction is what finally sets bIsAvailable false).
	// Only that second state change is what should end a claim.
	const UGSBreakableComponent* Breakable = Candidate->FindComponentByClass<UGSBreakableComponent>();
	const bool bIsUnbrokenContainerHit = Breakable && !Breakable->IsBroken();
	const bool bIsBrokenButNotYetLootedHit = Breakable && Breakable->IsBroken() && Interactable && Interactable->IsAvailable();

	return bIsCarryableHit || bIsUnbrokenContainerHit || bIsBrokenButNotYetLootedHit;
}

AActor* UGSHordeSubsystem::FindNearestLootable(const FVector& Location, const TSet<TWeakObjectPtr<AActor>>& Exclude) const
{
	const UWorld* World = GetWorld();
	if (!World || LootSearchRadius <= 0.f)
	{
		return nullptr;
	}

	// SAME object types UGSHordeCommandComponent::TraceForOrder queries by, for the same reason its
	// own comment gives: querying by OBJECT TYPE rather than sweeping ECC_Visibility cannot be
	// blocked by the landscape (WorldStatic, not in this query), which is the exact bug that made
	// every aimed Attack/Loot read as "the trace found bare ground" before that fix landed. A local
	// carryable search with the same failure mode would be the same bug wearing a new radius.
	FCollisionObjectQueryParams SubjectTypes;
	SubjectTypes.AddObjectTypesToQuery(ECC_Pawn);
	SubjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	SubjectTypes.AddObjectTypesToQuery(ECC_PhysicsBody);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Location, FQuat::Identity, SubjectTypes,
		FCollisionShape::MakeSphere(LootSearchRadius));

	AActor* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (!IsValid(Candidate) || Exclude.Contains(TWeakObjectPtr<AActor>(Candidate)))
		{
			continue;
		}

		if (!IsStillLootable(Candidate))
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Location, Candidate->GetActorLocation());
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Candidate;
		}
	}

	return Nearest;
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

	// 0. The Warren, if the level has one (ruling 18: it is "the turn-in point for cargo").
	//    Above the stones deliberately - the Warren is the forgiving bank, and a courier walking
	//    past an open hole to reach a portal on the far side of the map is the wrong picture.
	if (const AGSWarren* Warren = AGSWarren::FindNearest(this, From))
	{
		return Warren->GetActorLocation();
	}

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

	// LOOT SEARCHES THE AREA BEFORE GIVING UP (2026-08-30, design ticket #379). Michael: "goblins
	// under my control will look around the area I pointed to, for anything they can loot." A Loot
	// order whose aim trace found bare ground is no longer refused outright - it searches
	// LootSearchRadius of Location for the nearest carryable first, and only falls through to the
	// refusal below if that search also comes up empty. This is Shape A from the design ticket
	// (nearest-in-radius, reusing 100% of the existing single-subject order machinery unchanged -
	// the resolved Subject flows into the exact same marker/ActiveOrders/logging path as if the
	// player had aimed at it directly) rather than Shape B (each goblin foraging independently):
	// Shape B needed a claim/reservation mechanism the design ticket flagged as unbuilt, and,
	// separately discovered while implementing this, there is no BT task anywhere that makes a
	// goblin actually interact with/carry a Loot order's subject once it arrives - Loot currently
	// only plants a marker and walks the warband to the location, for the aimed-at-directly case
	// exactly as much as for this one. That is a real, larger gap this change does not attempt to
	// close blind - it is not a regression this change introduces, since it was already true before
	// tonight for the case that already "worked".
	if (Verb == EGSHordeOrder::Loot && !IsValid(Subject))
	{
		Subject = FindNearestLootable(Location);
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
			TEXT("Ignored a %s order: it needs something to point at and the trace found bare ground%s."),
			*UEnum::GetDisplayValueAsText(Verb).ToString(),
			Verb == EGSHordeOrder::Loot
				? *FString::Printf(TEXT(", and nothing carryable was within %.0fuu of where you aimed"), LootSearchRadius)
				: TEXT(" - aim at a defender"));
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
	if (!Order)
	{
		LootClaims.Remove(Goblin);
		return nullptr;
	}

	// ONLY LOOT FORAGES INDEPENDENTLY. Attack/Hold share one Subject on purpose - the whole warband
	// piling onto the one guard you pointed at is the correct read of "get 'em", and there is no
	// "claim" concept for a living target's health total the way there is for a pile of separate loot.
	if (Order->Verb != EGSHordeOrder::Loot)
	{
		LootClaims.Remove(Goblin);
		return Order->Subject.Get();
	}

	// STICKY: once a goblin has committed to a nearby lootable, keep sending it there every refresh
	// rather than re-rolling and risking a mid-walk retarget. Only re-search once the claim goes stale
	// - the item this goblin was after got looted (by it or by someone else) or destroyed.
	if (TWeakObjectPtr<AActor>* Existing = LootClaims.Find(Goblin))
	{
		AActor* Claimed = Existing->Get();
		if (IsStillLootable(Claimed))
		{
			return Claimed;
		}
		LootClaims.Remove(Goblin);
	}

	// AREA-FORAGE (#379 Shape B, 2026-08-30 morning). Michael: "loot everything within a radius...
	// it makes it more interactive to see your group of goblins giggling as they smash and loot" -
	// each goblin under this Loot order searches for its OWN nearest UNCLAIMED item near the order's
	// location, instead of the whole warband sharing whatever the player's aim first resolved.
	TSet<TWeakObjectPtr<AActor>> Claimed;
	Claimed.Reserve(LootClaims.Num());
	for (const TPair<TWeakObjectPtr<AGSHordeGoblin>, TWeakObjectPtr<AActor>>& Pair : LootClaims)
	{
		if (Pair.Value.IsValid())
		{
			Claimed.Add(Pair.Value);
		}
	}

	AActor* Found = FindNearestLootable(Order->Location, Claimed);
	if (!Found)
	{
		// Nothing left UNCLAIMED nearby - fall back to the order's own resolved Subject (if any) so a
		// lone goblin, or one late to a small cluster, still has something to do rather than every
		// goblin past the first idling the moment the search radius runs dry. When there is genuinely
		// only one item in range this is exactly today's single-target behaviour, unchanged.
		//
		// Still gated by IsStillLootable - the Subject the player originally aimed at is exactly as
		// capable of having been smashed-and-looted out from under this goblin as anything the area
		// search would have found, and trusting it unconditionally is how a goblin ends up circling a
		// spent crate forever once nothing else is left to claim.
		AActor* Subject = Order->Subject.Get();
		Found = IsStillLootable(Subject) ? Subject : nullptr;
	}

	if (Found)
	{
		LootClaims.Add(Goblin, Found);
	}
	return Found;
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
