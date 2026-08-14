#include "AI/Tasks/BTTask_PickUpCargo.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Interaction/GSCarryComponent.h"
#include "GameFramework/Pawn.h"

UBTTask_PickUpCargo::UBTTask_PickUpCargo()
{
	NodeName = TEXT("Pick Up Cargo (StartCarry)");

	CargoSourceKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_PickUpCargo, CargoSourceKey),
		AActor::StaticClass());
	CargoSourceKey.SelectedKeyName = TEXT("OrderSubject");

	CargoKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_PickUpCargo, CargoKey),
		AActor::StaticClass());
	CargoKey.SelectedKeyName = TEXT("CargoActor");
}

void UBTTask_PickUpCargo::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		CargoSourceKey.ResolveSelectedKey(*BBAsset);
		CargoKey.ResolveSelectedKey(*BBAsset);
	}
}

EBTNodeResult::Type UBTTask_PickUpCargo::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!BB || !Controller)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Self = Controller->GetPawn();
	AActor* Cargo = Cast<AActor>(BB->GetValueAsObject(CargoSourceKey.SelectedKeyName));
	if (!Self || !IsValid(Cargo))
	{
		return EBTNodeResult::Failed;
	}

	// Dist2D for UBTTask_MeleeAttack's reason: a height difference between a 240uu goblin's origin
	// and a sack sitting on the floor should not eat the reach budget.
	if (FVector::Dist2D(Cargo->GetActorLocation(), Self->GetActorLocation()) > PickUpRange)
	{
		return EBTNodeResult::Failed;
	}

	UGSCarryComponent* Carry = Self->FindComponentByClass<UGSCarryComponent>();
	if (!Carry)
	{
		// Fails rather than crashes, and this is the honest state for anything that is not an
		// AGSHordeGoblin - the component was added to that class in #141 and to nothing else.
		return EBTNodeResult::Failed;
	}

	// Already carrying something (a previous order, a re-entered branch). Not a failure: the cargo is
	// on our back, which is exactly the postcondition this node exists to establish.
	if (Carry->IsCarrying())
	{
		BB->SetValueAsObject(CargoKey.SelectedKeyName, Carry->GetCarriedActor());
		return EBTNodeResult::Succeeded;
	}

	// SERVER ONLY by contract (GSCarryComponent.h:30). The behaviour tree only ever runs on the
	// authority, so there is no client path to guard here.
	if (!Carry->StartCarry(Cargo))
	{
		return EBTNodeResult::Failed;
	}

	BB->SetValueAsObject(CargoKey.SelectedKeyName, Cargo);
	return EBTNodeResult::Succeeded;
}
