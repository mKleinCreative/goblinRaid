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
