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

	/** Latched while backing out of personal space, and only cleared past MinDist + BackOffExitMargin.
	 *  See the long note on BackOffMinScale: the latch is what buys a minimum push without buying the
	 *  limit cycle that a bare minimum push would produce. */
	bool bBackingOff = false;
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
	 * WAS 60, and the justification above it was measuring a station that no longer exists. It read:
	 * "at 6 slots and OrbitRadius 300 the spacing is 300uu, so half is 150 and 60 leaves ample room."
	 * Since #108 the station is NOT at OrbitRadius 300 - it is the claimed ring slot the service
	 * writes, at RingRadius 180, where the slot spacing is 180uu and half of that is 90.
	 *
	 * That stale number is the slack the crowd has been living in, and it is slack in two directions
	 * at once:
	 *
	 *  - RADIALLY, "within 60 of a point 180 out" means anywhere from 120uu to 240uu from the victim.
	 *    #131 raised the near end to capsule contact with the personal-space floor; the far end still
	 *    means two agents at the same nominal station can be 120uu apart in depth.
	 *  - TANGENTIALLY, two neighbours on adjacent slots 180uu apart may each drift 60uu toward the
	 *    other, so the ring's guaranteed 180uu of separation is really 60uu - which is inside capsule
	 *    contact for every pair in the game. The ring geometry #107 computed so carefully was being
	 *    given away by the arrival test.
	 *
	 * 40 leaves 100uu of guaranteed neighbour clearance instead of 60, and is still comfortably under
	 * the 90uu half-spacing bound. It does not reach guard-to-guard contact (138.8uu) on its own -
	 * that needs either a ring wider than #107 was willing to buy, or steering, which is why the
	 * separation steer in AGSAIControllerBase exists rather than a further cut here.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float OnStationTolerance = 40.f;

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

	// ---- personal space ------------------------------------------------------------------------
	// These are MARGINS on top of the two bodies, never absolute distances. The absolute-distance
	// approach is what produced the bug: every spacing number in this system was chosen watching
	// 52uu goblins, and the 68.6uu guards inherited numbers that never fitted them.

	/**
	 * Surface daylight guaranteed between an attacker and its victim at rest, on top of both capsule
	 * radii. This is THE dial for how much room "personal space" means.
	 *
	 * 30 -> 50 on Michael's call of 2026-08-11 ("increase the amount of distance for personal space
	 * as well"). The hard ceiling is set by the station, not by taste: the floor must stay INSIDE
	 * RingRadius or the back-off pushes past the station while the station's spring pulls back in,
	 * and the two argue forever. The worst pair in the game is guard-to-guard at 68.6 + 70.2 =
	 * 138.8uu of capsule, so the ceiling is RingRadius - 138.8.
	 *
	 * At the old RingRadius 180 that ceiling was 41, which is why this could not simply be turned up
	 * - and why RingRadius moved to 200 in the same change. At 200 the ceiling is 61 and 50 leaves
	 * 11uu of headroom for a body bigger than a guard arriving later, which is exactly the failure
	 * #131 was written about (every distance in this system was tuned on 52uu goblins, and the
	 * 68.6uu humans inherited numbers that never fitted them).
	 *
	 * Resulting floors: 188.8 guard->guard, 170.6 guard->player, 154.0 goblin->goblin. Surface
	 * daylight between two goblins goes from 30uu to 50uu.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Personal Space", meta = (ClampMin = "0.0"))
	float PersonalSpaceMargin = 50.f;

	/** The feint may still close to capsule contact - a real step in - but not through it. 0 is the
	 *  most aggressive setting that is not clipping. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Personal Space", meta = (ClampMin = "0.0"))
	float FeintMargin = 0.f;

	/** Depth inside the floor at which the back-off saturates.
	 *
	 *  40 -> 15. 40 was chosen so the ONE measured 129uu guard-on-guard case sat at full push, which
	 *  quietly meant every ordinary intrusion sat far below it: at 10uu inside the floor the gain was
	 *  0.25 and the resulting movement scale 0.09, which is not a step backwards, it is a lean. The
	 *  taper is meant to stop a limit cycle at the boundary, not to make the common case invisible. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Personal Space", meta = (ClampMin = "1.0"))
	float BackOffFullDepth = 15.f;

	/**
	 * Speed of the back-off, as a fraction of walk speed. Its own dial, where it used to borrow
	 * StrafeSpeedScale (0.35).
	 *
	 * Sharing the strafe's number was the bug in disguise. StrafeSpeedScale is deliberately low
	 * because a fast orbit "reads as the crowd sliding on ice" - but backing out of someone's body is
	 * not a wary shuffle, it is the one motion in this node that has somewhere it urgently needs to
	 * be. Capping it at a third of walk speed against a crowd pressing in at full speed meant the
	 * push lost, and the agent stayed where it was: "they don't back up".
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Personal Space", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BackOffSpeedScale = 0.8f;

	/**
	 * Floor under the tapered gain, so a shallow intrusion still produces a real step rather than a
	 * sub-visible dribble.
	 *
	 * This is the part #131 could not have without the latch below. Its taper drives both the
	 * magnitude AND the derivative to zero at the boundary specifically so the push cannot overshoot
	 * and re-enter - a bare minimum push is the textbook way to build a two-agent limit cycle, which
	 * is what that comment was guarding against. The latch replaces that guarantee with a different
	 * one: the push does not stop at the boundary, it stops at BackOffExitMargin past it, so there is
	 * no threshold for the pair to chatter across.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Personal Space", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BackOffMinScale = 0.35f;

	/** Once backing off, keep going until this far OUTSIDE the floor. Deadband, so the pair separates
	 *  cleanly instead of oscillating about the boundary - and so the result reads as a deliberate
	 *  step back rather than a twitch. 20uu is roughly one stride. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Personal Space", meta = (ClampMin = "0.0"))
	float BackOffExitMargin = 20.f;

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
