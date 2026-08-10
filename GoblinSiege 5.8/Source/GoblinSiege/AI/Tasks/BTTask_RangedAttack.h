// Looses an arrow at the blackboard target, through the same AGSCharacterBase verb the player's bow
// runs: TryRangedAttack -> UGSGA_BowShot.
//
// ---- the whole trick is AIMING -----------------------------------------------------------------
// UGSGA_BowShot::FireArrow spawns the arrow along the pawn's CONTROL ROTATION when there is no aim
// component - which is the AI case, and was written that way deliberately. But an AI controller's
// control rotation does not point at anything by default: it follows the pawn's own facing, so an
// archer that simply activated the ability would fire wherever its body happened to be turned,
// usually into the ground or past its target's shoulder.
//
// AAIController::SetFocus is what fixes that: it makes the control rotation track the focal actor,
// pitch included. So this task sets focus, waits for the rotation to actually arrive (the controller
// interpolates - firing on the first frame would loose the arrow mid-turn), and only then shoots.
// That "wait for the aim to land" beat is also what makes an archer READ as aiming rather than
// snap-firing, which matters more than the accuracy does.
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_RangedAttack.generated.h"

/** Per-agent: BT nodes are shared across every tree running the asset, so a shared draw clock would
 *  make a rank of archers loose in unison like a firing squad. */
struct FGSRangedAttackMemory
{
	float NextAllowedShotTime = 0.f;

	/** When the current draw resolves into a released arrow. */
	float ShotAtTime = 0.f;

	bool bDrawing = false;
};

UCLASS()
class GOBLINSIEGE_API UBTTask_RangedAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_RangedAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FGSRangedAttackMemory); }
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	FBlackboardKeySelector TargetKey;

	/** Closest an archer will shoot from. Inside this he would rather back off than loose - it is
	 *  what stops an archer standing in a melee plinking at someone hitting him. The tree's chase
	 *  branch does the backing off, by walking to a stand-off slot placed further out. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float MinRange = 350.f;

	/** Beyond this he closes instead. Comfortably inside AGSAIControllerBase's 1200uu sight so an
	 *  archer never shoots at something it could not plausibly have seen. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float MaxRange = 1100.f;

	/** Seconds spent visibly aiming before the arrow leaves. Doubles as the telegraph: an archer who
	 *  fires the instant he acquires you is a hitscan trap, and one who takes a beat is a threat you
	 *  can break line of sight against. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float DrawSeconds = 0.8f;

	/** Gap between this archer's shots, plus jitter so a rank does not volley in lockstep. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float ShotCooldownSeconds = 2.5f;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float ShotCooldownJitter = 1.f;

	/** Refuse the shot without line of sight. An arrow is a real projectile in this game - it would
	 *  otherwise be fired into the wall the archer is standing behind, which reads as a bug rather
	 *  than as a miss. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	bool bRequireLineOfSight = true;

	/** Upward bias in degrees applied to the aim while drawing.
	 *
	 *  Arrows carry 0.2 gravity, so a flat shot drops over distance. This is a lob, not a ballistic
	 *  solution: enough that long shots land near the target rather than short, cheap enough that it
	 *  costs no solver. Set to 0 for a flat trajectory. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float AimLoftDegrees = 3.f;
};
