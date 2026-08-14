// Swings at the blackboard target, through the same AGSEnemyCharacter verb a designer would call
// by hand: TryLightAttack. The attack itself is NOT reimplemented here - GSEnemyCharacter.h is
// explicit that a defender running its own swing code would drift from the player's within a week.
// This node only decides "close enough, facing roughly right, ask the character to swing".
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_MeleeAttack.generated.h"

/** Per-agent, because BT nodes are shared between every tree running the asset - storing the next
 *  allowed swing on the node itself would pace the whole patrol as one. */
struct FGSMeleeAttackMemory
{
	float NextAllowedAttackTime = 0.f;

	/** When this node last stepped the facing. The turn is rate-limited against elapsed WALL CLOCK
	 *  rather than frame delta, because ExecuteTask runs on tree re-activation and not every frame -
	 *  see the facing block in ExecuteTask. */
	float LastFacingStepTime = 0.f;

	/** Separate from the swing cooldown so a guard break is a read rather than rotation filler. */
	float NextAllowedGuardBreakTime = 0.f;
};

UCLASS()
class GOBLINSIEGE_API UBTTask_MeleeAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MeleeAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FGSMeleeAttackMemory); }

protected:
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	FBlackboardKeySelector TargetKey;

	/** Distance between actor origins, not a weapon reach - the ability does its own trace. Generous
	 *  on purpose: refusing here just means the chase branch runs for another tick. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	float AttackRange = 250.f;

	/** Turn to face the target before swinging. The swing traces forward, so an unturned defender
	 *  would flail at empty air next to a player it is standing beside. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	bool bFaceTargetBeforeSwing = true;

	/** Largest instant yaw correction allowed before a swing, in degrees.
	 *
	 *  This used to be uncapped, which made every defender a turret: it would snap 180 degrees onto
	 *  whoever was behind it and swing in the same frame, so circling did nothing and no attack
	 *  could ever be made to whiff. Missing has to be possible or there is no spacing, and with no
	 *  spacing there is nothing to watch. A defender that needs more correction than this simply
	 *  fails and lets the chase branch walk it round. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxFacingSnapDegrees = 120.f;

	/** How near to square-on counts as "facing", when the CONTROLLER owns the turn (#133).
	 *
	 *  Only consulted while GS.Combat.FaceTarget is on. In that mode this node no longer turns the
	 *  pawn at all - AGSAIControllerBase::TickFacing does, every frame, whatever branch the tree is in
	 *  - so all this node still has to decide is whether the body has come round far enough to swing
	 *  at something rather than past it.
	 *
	 *  It needs a TOLERANCE rather than the exact-equality test the old turn-and-check used, and that
	 *  is not a detail. bUseControllerDesiredRotation interpolates asymptotically toward the control
	 *  rotation, so the remaining error approaches zero without reaching it, and against a target that
	 *  is itself moving it never even settles. Demanding exactness would refuse every swing forever -
	 *  #089's deadlock ("stood next to its target at 130uu with zero velocity and never swung") with a
	 *  new cause. 25 degrees is inside the swing's own forward trace, so a hit that passes this gate
	 *  still connects, and it is tight enough that flanking keeps costing the attacker real time. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "1.0", ClampMax = "90.0"))
	float FacingToleranceDegrees = 25.f;

	// ---- guard break (2026-08-08) ---------------------------------------------------------
	// Folded into this node rather than given its own BTTask_GuardBreak: it answers the same
	// question ("what do I do at melee range"), and a separate node would duplicate the range test,
	// the facing test and the cooldown bookkeeping while letting a tree order the two in a way that
	// makes no sense. The cost is that the order is fixed in code; that is acceptable for a verb
	// only one faction has.
	//
	// The ASYMMETRY is the design, not an oversight (GSCharacterBase.h:228-231): humans carry
	// GuardBreakAbilityClass, allied goblins leave it unset. CanGuardBreak() is exactly that null
	// check, so a goblin skips this branch for free and must out-flank or out-wait a raised guard
	// instead. BlockHoldSeconds on UBTTask_Block is therefore what decides whether goblin-vs-human
	// is winnable at all.

	/** Chance to answer a raised guard with a guard break instead of another blocked swing. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|GuardBreak", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GuardBreakChance = 0.7f;

	/** Gap between this agent's guard breaks. Long enough that it reads as a decision rather than
	 *  part of a rotation, and short enough to answer a defender that re-blocks on cooldown. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|GuardBreak", meta = (ClampMin = "0.0"))
	float GuardBreakCooldownSeconds = 4.f;

	/** Shorter than AttackRange: the kick has less reach than the blade. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|GuardBreak", meta = (ClampMin = "0.0"))
	float GuardBreakRange = 200.f;

	/** Fallback pause between this agent's swings when its archetype has no AttackCooldownSeconds.
	 *  Three defenders reaching the player together were swinging on the same frame forever, which
	 *  is both unreadable and unsurvivable. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float DefaultAttackCooldownSeconds = 1.2f;

	/** Random slice added to each agent's cooldown so a patrol that arrived together does not stay
	 *  in lockstep. Without it they simply attack in unison at a slower rate. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float AttackCooldownJitter = 0.6f;
};
