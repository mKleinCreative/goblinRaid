// Writes something worth attacking into the "TargetActor" blackboard key (the key name
// AGSAIControllerBase's header nominates as canonical for every archetype's tree), the stand-off
// slot to walk to, and whether that target is currently winding up a swing.
//
// ---- 2026-08-08: this node no longer knows what a player is -----------------------------------
// It used to be one line of UGameplayStatics::GetPlayerPawn(this, 0), and its header called itself
// "deliberately the crude version ... the piece expected to be replaced first". What was crude was
// the SELECTION RULE, not the node: the dead-target rejection, the clear-on-out-of-range that lets
// a Selector reach its idle branch, and the stand-off ring maths all took tickets #006 and #008 to
// get right, and all of them were already target-agnostic. So the rule was replaced in place.
//
// Selection is now "nearest live hostile", resolved through AGSCharacterBase::IsHostileTo on
// RaceTag. That is deliberately the SAME predicate the melee sweep uses to decide whether a hit
// connects: an AI that would chase something it cannot damage, or ignore something that can damage
// it, is a bug waiting to be found in a playtest instead of at compile time.
//
// Note what did NOT happen: no EQS query, no IGenericTeamAgentInterface, no revival of
// AGSAIControllerBase::HandlePerceptionUpdated. GSAIControllerBase.cpp:28-33 records the standing
// ruling that RaceTag is the only friend/foe truth in this project and that a second source would
// be worse than a crude first one. This is the "real selection" that comment was waiting for; it
// just reaches it through the tag rather than through the perception system.
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTService_AcquireTarget.generated.h"

class AGSCharacterBase;

/** Per-agent. BT nodes are shared between every tree running the asset, so a scan timer on the node
 *  itself would make one defender's scan silence the whole patrol's. */
struct FGSAcquireTargetMemory
{
	/** Throttles the expensive candidate scan independently of the node's own tick, which has to be
	 *  fast enough to catch a windup. */
	float NextScanTime = 0.f;

	/** The telegraph is latched rather than sampled, so a windup cannot fall between two ticks. */
	float TelegraphUntil = 0.f;

	/** GetUniqueID of the target the latch belongs to. Stored as a plain integer because BT node
	 *  memory is raw bytes - a TWeakObjectPtr here would never be constructed or destroyed. Exists
	 *  only so switching targets drops a latch that belonged to the previous one. */
	uint32 LatchedTargetId = 0;
};

UCLASS()
class GOBLINSIEGE_API UBTService_AcquireTarget : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_AcquireTarget();

	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FGSAcquireTargetMemory); }

	/**
	 * Resolves the key selectors against the tree's blackboard.
	 *
	 * MUST exist, and its absence was invisible for as long as this node only WROTE keys. Writing
	 * goes through SelectedKeyName and needs no resolution at all, so TargetKey and TargetLocationKey
	 * worked from the day this node was written despite never being resolved. The moment anything
	 * asks FBlackboardKeySelector::IsSet() - which tests SelectedKeyType, populated only here - the
	 * answer is a permanent false, and the feature guarded by it silently never runs.
	 *
	 * That is exactly what happened to the telegraph on 2026-08-08: TargetIsAttacking was never
	 * written, the Block decorator therefore never passed, and in PIE it read as "the AI just does
	 * not block" with no error anywhere.
	 */
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
	/** Blackboard key to write. Defaults to TargetActor to match the controller's documented set. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	FBlackboardKeySelector TargetKey;

	/** The same target as a plain location, written alongside TargetActor.
	 *
	 *  Not redundant: MoveTo against an ACTOR goal resolves "have I arrived" against that actor's
	 *  collision extent, and a RAGDOLLED character's extent is enormous - a dead player measured
	 *  ~5385x4748 units, against ~75x58 alive - so every defender within ~50m of a corpse gets
	 *  PathFollowingRequestResult::AlreadyAtGoal and never takes a step. (A live player is fine;
	 *  the original diagnosis blamed the player Blueprint and was wrong.) The dead-target check in
	 *  TickNode is the real cure, and chasing a location rather than an actor keeps the tree honest
	 *  regardless of what any goal actor's bounds happen to be doing. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	FBlackboardKeySelector TargetLocationKey;

	/** True while the current target holds State.Attacking.Windup. UBTTask_Block's whole reason to
	 *  exist: this is the one channel by which an AI may know a hit is coming.
	 *
	 *  Written here rather than read directly by the block task because the latch (below) has to
	 *  live somewhere that ticks faster than the task runs, and because a tree without the key
	 *  should still chase and swing rather than fail to compile its blackboard. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	FBlackboardKeySelector TargetIsAttackingKey;

	/** Turn off for a tree whose target is chosen elsewhere, and this node becomes slot-and-telegraph
	 *  only. That is exactly BT_HordeGoblin's case: AGSHordeAIController writes TargetActor from
	 *  UGSHordeSubsystem's threat registry every 0.2s, and a scan here would fight it every tick.
	 *
	 *  This is not a hole in GDD §3.4's "perception-less agents fed stimuli by a central subsystem".
	 *  The forbidden cost is a per-agent SEARCH; reading one gameplay tag off the actor the
	 *  subsystem already told you to attack is not a search. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	bool bSelectTarget = true;

	/** How far out this agent will pick up a new hostile.
	 *
	 *  1500 rather than the old 3000: it now matches AGSAIControllerBase's SightRadius (1200) /
	 *  LoseSightRadius (1500) closely enough that a defender does not sprint at something it has no
	 *  business having noticed. At 3000, with selection widened past the player, every defender in
	 *  the hamlet acquired the first goblin to arrive. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float AcquireRadius = 1500.f;

	/** How far the target may get before it is dropped. Strictly greater than AcquireRadius, or an
	 *  agent sitting exactly on the boundary acquires and drops the same target on alternate ticks. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float LoseRadius = 2000.f;

	/** A new candidate must be this much closer than the current target to steal it.
	 *
	 *  Without it, two defenders standing between two goblins swap target every scan and neither
	 *  ever closes the distance - they oscillate on the spot looking broken. 0.75 is loose enough
	 *  that a genuinely better target still wins immediately. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TargetSwitchHysteresis = 0.75f;

	/** Seconds between candidate scans. Decoupled from Interval on purpose: the node has to tick
	 *  fast enough to catch a 0.22s windup, but the O(actors) scan does not need to run that often. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float ReacquireIntervalSeconds = 0.5f;

	/** How long a seen windup keeps TargetIsAttacking true.
	 *
	 *  Roughly 3x the tick interval, so no windup can be missed between two samples, and short
	 *  enough that it expires well before the next swing. This is also the honest version of a
	 *  reaction window: the AI does not know the attack is coming until it has been visible, and it
	 *  stops knowing shortly after. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float TelegraphLatchSeconds = 0.45f;

	/** How far from the target each agent actually walks TO.
	 *
	 *  This is the crowding fix. Writing the target's own location made every defender path to one
	 *  identical point, so three of them arrived on the same spot and shoved - capsules block, so
	 *  they never interpenetrate, but they pile into a single knot and read as one merged blob.
	 *
	 *  180 with 8 slots puts adjacent slots 2*180*sin(22.5) = 138uu apart, which is the ~137uu
	 *  capsule-touch distance ticket #008 measured. The old pairing (30 degrees of jitter at radius
	 *  170) gave only ~89uu and was recorded there as known-undersized.
	 *
	 *  Keep it under BTTask_MeleeAttack's AttackRange (250) or they stand off and never swing. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float StandoffRadius = 180.f;

	/** Number of evenly spaced bearings around the target.
	 *
	 *  Replaces the old per-pawn FName-hash jitter. Snapping to a shared lattice keeps the property
	 *  that made the hash version work - you approach from the side you are already on, so nobody
	 *  crosses the pack to reach their slot - while making the spacing a known quantity instead of
	 *  a hash collision. Fewer than 8 and three attackers cannot spread; more and adjacent slots
	 *  fall back inside one capsule. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "1", ClampMax = "32"))
	int32 ApproachSlotCount = 8;

	/**
	 * Overrides how far out this tree's agents stand. 0 = use the target's own ring radius.
	 *
	 * The ring lives on the VICTIM (#090), which is right for spacing a melee crowd - one ring, one
	 * set of exclusive slots, no two attackers in one place. But it means every attacker stands at
	 * the same distance, and an archer must not: sent to a melee ring an archer walks into the fight
	 * it is supposed to be shooting into.
	 *
	 * So the slot still decides the DIRECTION - the archer keeps its exclusive place in the ring and
	 * counts against capacity exactly like anyone else - and this decides the DISTANCE along it.
	 * BT_Archer sets ~700; melee trees leave it at 0.
	 *
	 * This is also what kites: the slot is recomputed around the target every tick, so as an enemy
	 * closes, the archer's destination slides away from it and the chase branch walks him back out.
	 * No retreat behaviour needed.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float StandoffRadiusOverride = 0.f;

private:
	/** Nearest live hostile within AcquireRadius, or null. Hostility is IsHostileTo on RaceTag.
	 *
	 *  Candidates must carry a VALID RaceTag. IsHostileTo treats "no opinion" as hostile, which is
	 *  the right default for DAMAGE - nothing becomes accidentally immune - and the wrong one for
	 *  ACQUISITION, because BP_GS_TargetDummy has no RaceData and every defender in the level would
	 *  jog over to it. Damage semantics are untouched by this; only who gets chased. */
	AGSCharacterBase* FindNearestHostile(const AGSCharacterBase& Self, AActor* CurrentTarget) const;

	/** Alive, valid, still carries a hostile race. Shared by the scan and by the keep-or-drop test
	 *  on an existing target, so the two can never disagree about what a legal target is. */
	bool IsEngageable(const AGSCharacterBase& Self, const AActor* Candidate) const;
};
