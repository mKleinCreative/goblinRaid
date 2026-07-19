#include "AI/Tasks/BTTask_EvadeTelegraph.h"
#include "AIController.h"
#include "GameFramework/Character.h"

UBTTask_EvadeTelegraph::UBTTask_EvadeTelegraph()
{
	NodeName = TEXT("Evade Telegraph (sidestep)");
}

EBTNodeResult::Type UBTTask_EvadeTelegraph::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	ACharacter* Character = AIController ? Cast<ACharacter>(AIController->GetPawn()) : nullptr;
	if (!Character)
	{
		return EBTNodeResult::Failed;
	}

	// Random left/right sidestep - enough to feel slippery without full dodge-roll animation work.
	const float Direction = FMath::RandBool() ? 1.f : -1.f;
	const FVector Impulse = Character->GetActorRightVector() * Direction * EvadeImpulse
		+ FVector(0.f, 0.f, EvadeUpImpulse);
	Character->LaunchCharacter(Impulse, true, false);

	return EBTNodeResult::Succeeded;
}
