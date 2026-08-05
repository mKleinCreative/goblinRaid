#include "AI/Tasks/BTTask_MeleeAttack.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/GSEnemyCharacter.h"
#include "Kismet/KismetMathLibrary.h"

UBTTask_MeleeAttack::UBTTask_MeleeAttack()
{
	NodeName = TEXT("Melee Attack (TryLightAttack)");

	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MeleeAttack, TargetKey),
		AActor::StaticClass());
	TargetKey.SelectedKeyName = TEXT("TargetActor");
}

EBTNodeResult::Type UBTTask_MeleeAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* /*NodeMemory*/)
{
	const UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!BB || !Controller)
	{
		return EBTNodeResult::Failed;
	}

	AGSEnemyCharacter* Self = Cast<AGSEnemyCharacter>(Controller->GetPawn());
	AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!Self || !IsValid(Target))
	{
		return EBTNodeResult::Failed;
	}

	if (FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation()) > AttackRange)
	{
		return EBTNodeResult::Failed;
	}

	if (bFaceTargetBeforeSwing)
	{
		const FRotator Look = UKismetMathLibrary::FindLookAtRotation(
			Self->GetActorLocation(), Target->GetActorLocation());
		Self->SetActorRotation(FRotator(0.f, Look.Yaw, 0.f));
	}

	// Failing when the swing is refused is deliberate: GAS refuses re-activation while a swing is
	// already running, and UGSGA_SwordLight treats that refusal as its combo buffer. Reporting
	// Failed lets the Selector fall through to the chase branch instead of the tree stalling on a
	// task waiting for an ability that will never start.
	return Self->TryLightAttack() ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}
