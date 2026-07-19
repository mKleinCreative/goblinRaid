// Elf-archetype sidestep on attack telegraph ("evade-on-telegraph", race-design-elves.md).
// PARKED with the Elves for the slice (tech doc §8/§14) - kept compiling as the landing pad.
// Reconstructed 2026-07-19.
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_EvadeTelegraph.generated.h"

UCLASS()
class GOBLINSIEGE_API UBTTask_EvadeTelegraph : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_EvadeTelegraph();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	float EvadeImpulse = 600.f;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	float EvadeUpImpulse = 150.f;
};
