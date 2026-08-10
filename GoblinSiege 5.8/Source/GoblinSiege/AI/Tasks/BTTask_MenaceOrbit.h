// What an agent does while it is NOT allowed to attack.
//
// This node exists because of the single most consistent note in the crowd-combat literature: an
// enemy without permission to attack must still be visibly participating. DOOM's token system is
// justified in exactly these terms - the point was to stop demons "standing around looking stupid
// instead of trying to kill you". The melee-AI roundups say the same: enemies "play idle
// animations, cheering, or taunting when not attacking - maintaining apparent participation rather
// than passive waiting".
//
// Without it, rationing attacks makes the fight WORSE than not rationing them: instead of ten
// goblins swinging at once you get two goblins swinging and eight standing in a rest pose, which
// reads as a broken AI rather than as a crowd waiting its turn. The token system and this node are
// one feature and should never ship apart.
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_MenaceOrbit.generated.h"

/** Per-agent: BT nodes are shared across every tree running the asset, so a shared feint clock
 *  would make a whole warband lunge on the same frame. */
struct FGSMenaceOrbitMemory
{
	float NextFeintTime = 0.f;
	float FeintUntilTime = 0.f;

	/** When this run of the orbit gives the tree a chance to reconsider. */
	float ReevaluateAtTime = 0.f;

	/** +1 or -1. Rolled ONCE and then kept across re-entries - because the node deliberately ends
	 *  every OrbitReevaluateSeconds, re-rolling on entry would reverse the agent's direction twice a
	 *  second and it would shuffle on the spot instead of circling. 0 means "not yet chosen". */
	float StrafeSign = 0.f;
};

UCLASS()
class GOBLINSIEGE_API UBTTask_MenaceOrbit : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MenaceOrbit();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FGSMenaceOrbitMemory); }
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	FBlackboardKeySelector TargetKey;

	/**
	 * Where to stand. MUST be the same key the MoveTo sibling uses, or the two nodes disagree about
	 * where the agent belongs and fight over it every time this task hands the branch back - which
	 * is exactly the jitter this key was added to end (#108).
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	FBlackboardKeySelector StationKey;

	/** Distance held while circling. Outside the stand-off ring, so a menacing agent is visibly
	 *  waiting rather than crowding the slot of someone who has earned one. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float OrbitRadius = 300.f;

	/** Sideways speed as a fraction of the agent's walk speed. Low - this is a wary shuffle, not a
	 *  sprint, and a fast orbit reads as the crowd sliding on ice. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StrafeSpeedScale = 0.35f;

	/**
	 * How close to its own station on the ring counts as arrived.
	 *
	 * Must stay well under half the orbit-slot spacing or "on station" could mean standing on the
	 * neighbour's bearing, which is the whole bug this exists to fix. At 6 slots and OrbitRadius 300
	 * the spacing is 2*300*sin(30deg) = 300uu, so half is 150 and 60 leaves ample room.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float OnStationTolerance = 60.f;

	/** Once on station, how much of the strafe speed remains as an idle shuffle. Not zero: a waiting
	 *  attacker frozen in a rest pose is the failure the research warns about, and it is also what a
	 *  pile of corpses looks like. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OnStationShuffleScale = 0.25f;

	/** A short lunge toward the target and back. The feint is what sells intent: a circling enemy
	 *  that never threatens is scenery, and one that threatens without committing is pressure. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float FeintIntervalMin = 1.5f;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float FeintIntervalMax = 2.5f;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float FeintDurationSeconds = 0.35f;

	/** Speed toward the target during a feint, as a fraction of walk speed. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float FeintSpeedScale = 0.9f;

	/**
	 * Beyond this the node FAILS so the Selector falls through to the chase branch.
	 *
	 * This node steers directly (AddMovementInput), which is right for circling a target you can see
	 * and wrong for crossing a courtyard - it has no pathfinding and would walk into walls. So it
	 * sits ABOVE MoveTo in the tree and hands long approaches back to the path follower, which is
	 * the only node that should ever be responsible for getting somewhere.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float MaxOrbitDistance = 600.f;

	/**
	 * Finish (Succeeded) after this long so the Selector re-evaluates from the top.
	 *
	 * A Selector only reconsiders its children when the running one finishes, so a latent orbit that
	 * never ended would hold the branch forever and the agent would circle even after a token freed
	 * up. Ending on a short clock is the cheap version of "poll for permission"; it costs at most
	 * this much delay before a waiting attacker takes its turn.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.1"))
	float OrbitReevaluateSeconds = 0.6f;
};
