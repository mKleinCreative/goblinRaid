// Raises the guard, through the same AGSCharacterBase verb the player's input calls: StartBlocking.
//
// Until this node existed, StartBlocking / StopBlocking had ZERO callers outside player input, even
// though all six adversary Blueprints have carried GA_GS_Block_C the whole time. Every defender in
// the game owned a shield it could not raise, and the entire block/guard-break loop in
// GSDamageExecCalculation was unreachable unless a human being was holding the other end of it.
//
// ---- why a latent task, and not a timer ------------------------------------------------------
// The obvious version is "call StartBlocking, set a timer to stop". That leaks a permanent guard
// the first time the branch is aborted - a target dies, a higher-priority branch wins - because the
// timer outlives the decision that made it. A latent task gets OnTaskFinished on EVERY exit
// including abort, so the guard drops exactly when the tree stops wanting it. There is one owner of
// the raised guard and it is this node.
//
// ---- why a task, and not folded into BTService_AcquireTarget ----------------------------------
// The service already ticks, already knows the target, and already computes the telegraph, so it
// could do this for free. It should not: GSAIControllerBase.h:1-5 is explicit that per-archetype
// BEHAVIOUR differences belong in Behavior Tree assets and task nodes rather than in shared
// controller/perception code, and how much a unit turtles is precisely a per-archetype dial - a
// Knight should block far more than a militia levy, and an allied goblin somewhere between. A
// service also cannot be aborted by the tree, which is the property the paragraph above depends on.
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_Block.generated.h"

/** Per-agent: BT nodes are shared between every tree running the asset, so cooldowns stored on the
 *  node would pace a whole patrol as one - the same reason FGSMeleeAttackMemory exists. */
struct FGSBlockMemory
{
	float NextAllowedBlockTime = 0.f;
	float BlockUntilTime = 0.f;
};

UCLASS()
class GOBLINSIEGE_API UBTTask_Block : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_Block();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FGSBlockMemory); }

	/** Resolves the key selectors. Without this `IsSet()` is a permanent false - see the long note on
	 *  UBTService_AcquireTarget::InitializeFromAsset, which is where that cost an hour. */
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	FBlackboardKeySelector TargetKey;

	/** Written by UBTService_AcquireTarget from the target's State.Attacking.Windup tag. If this key
	 *  is unset on the blackboard the node still works - it just falls back to IdleBlockChance and
	 *  the defender guards on vibes rather than on a read. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	FBlackboardKeySelector TargetIsAttackingKey;

	/** Chance to raise the guard when the target IS visibly winding up.
	 *
	 *  0.55 rather than something decisive: a block cuts a hit to 20% (GS.Combat.BlockMultiplier),
	 *  so expected damage taken is 0.55*0.2 + 0.45*1.0 = 0.56 of raw. That roughly halves incoming
	 *  DPS without becoming a wall. At 1.0 an allied goblin - which has no guard break, by design -
	 *  could never kill a human at all, and the fight would end in a stalemate rather than a death. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TelegraphBlockChance = 0.55f;

	/** Chance to raise the guard with nothing incoming. Small on purpose: it exists so the guard is
	 *  SEEN going up and down while closing, which is most of what makes a fight readable from ten
	 *  metres away. Much higher and defenders turtle through the approach and never trade. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IdleBlockChance = 0.1f;

	/** How long the guard stays up once raised, plus a random slice.
	 *
	 *  0.9s covers a 0.22s windup plus a 0.16s damage window with slop for the tick. It is also two
	 *  numbers at once: the human's punish window for a guard break, and the goblin's wait-it-out
	 *  window. Below ~0.6 the guard flickers; above ~1.5 a goblin can never get through one. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float BlockHoldSeconds = 0.9f;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float BlockHoldJitter = 0.3f;

	/** Gap after a block ends before another may start. With BlockHoldSeconds this caps guard uptime
	 *  near 50% in the worst case, which is what guarantees openings without needing a stamina
	 *  system on the AI (UGSStaminaComponent is player-only and has no C++ callers at all). */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float BlockCooldownSeconds = 1.f;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float BlockCooldownJitter = 0.4f;

	/** Gap after a FAILED roll before rolling again.
	 *
	 *  Without this every probability above is a lie. The tree re-runs this node several times
	 *  inside a single 0.22s windup, and n rolls at p compound to 1-(1-p)^n - three ticks turn a
	 *  0.55 chance into 0.91 and the defender blocks essentially everything. This is the single
	 *  most load-bearing number in the node and the least obvious one. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float DeclineCooldownSeconds = 0.35f;

	/** Do not guard against someone who is not close enough to hit you. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float BlockRange = 320.f;

	/** Drop the guard once the target is this far away. Larger than BlockRange so it is a release,
	 *  not a flicker - and it matters because UGSGA_Block applies a 0.45x move-speed scalar, so a
	 *  blocker that never released could not chase a target that disengaged. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float BlockBreakRange = 450.f;

	/** Turn to face the target while guarding.
	 *
	 *  Not decoration - it is what makes the block mean anything. GSDamageExecCalculation mitigates
	 *  only within a 140-degree FRONTAL arc, and these pawns run bOrientRotationToMovement with
	 *  bUseControllerRotationYaw false, so a defender standing still to block never turns at all and
	 *  an unfaced guard mitigates exactly nothing. This is also the first AI-side consumer of
	 *  AGSCharacterBase::GetTurnRateRadPerSec, which existed as a per-archetype identity dial with
	 *  nothing reading it. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	bool bFaceTargetWhileBlocking = true;
};
