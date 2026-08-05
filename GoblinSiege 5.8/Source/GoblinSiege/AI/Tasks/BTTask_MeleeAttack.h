// Swings at the blackboard target, through the same AGSEnemyCharacter verb a designer would call
// by hand: TryLightAttack. The attack itself is NOT reimplemented here - GSEnemyCharacter.h is
// explicit that a defender running its own swing code would drift from the player's within a week.
// This node only decides "close enough, facing roughly right, ask the character to swing".
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_MeleeAttack.generated.h"

UCLASS()
class GOBLINSIEGE_API UBTTask_MeleeAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MeleeAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

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
};
