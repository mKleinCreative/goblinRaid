// "Defenders fight the fires" (design doc §4): find the nearest burning flammable in radius,
// extinguish it, and pay the alarm meter back down a little. Assign in defender Behavior Trees
// behind a "fire nearby && not in melee" decorator. Reconstructed 2026-07-19.
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_Firefight.generated.h"

UCLASS()
class GOBLINSIEGE_API UBTTask_Firefight : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_Firefight();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	float SearchRadius = 1500.f;

	/** How much alarm one extinguish refunds - see GSGameState::AddAlarm's negative-amount path. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	float AlarmReductionOnExtinguish = 2.f;
};
