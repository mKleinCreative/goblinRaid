#include "AI/Tasks/BTTask_DeliverCargo.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Horde/GSHordeGoblin.h"
#include "Horde/GSHordeSubsystem.h"
#include "Interaction/GSCarryComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UBTTask_DeliverCargo::UBTTask_DeliverCargo()
{
	NodeName = TEXT("Deliver Cargo (NotifyCourierDelivered)");

	CargoKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_DeliverCargo, CargoKey),
		AActor::StaticClass());
	CargoKey.SelectedKeyName = TEXT("CargoActor");

	DeliveryLocationKey.AddVectorFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTTask_DeliverCargo, DeliveryLocationKey));
	DeliveryLocationKey.SelectedKeyName = TEXT("DeliveryLocation");
}

void UBTTask_DeliverCargo::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		CargoKey.ResolveSelectedKey(*BBAsset);
		DeliveryLocationKey.ResolveSelectedKey(*BBAsset);
	}
}

EBTNodeResult::Type UBTTask_DeliverCargo::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!BB || !Controller)
	{
		return EBTNodeResult::Failed;
	}

	AGSHordeGoblin* Self = Cast<AGSHordeGoblin>(Controller->GetPawn());
	if (!Self)
	{
		// Only a horde goblin can rejoin the horde pool. A defender running this node would be a
		// tree-authoring mistake, not a runtime condition worth handling.
		return EBTNodeResult::Failed;
	}

	const FVector Destination = BB->GetValueAsVector(DeliveryLocationKey.SelectedKeyName);
	if (FVector::Dist2D(Destination, Self->GetActorLocation()) > DeliverRange)
	{
		return EBTNodeResult::Failed;
	}

	UGSCarryComponent* Carry = Self->FindComponentByClass<UGSCarryComponent>();
	if (!Carry || !Carry->IsCarrying())
	{
		return EBTNodeResult::Failed;
	}

	Carry->PutDown();
	BB->ClearValue(CargoKey.SelectedKeyName);

	UGSHordeSubsystem* Horde = UGSHordeSubsystem::Get(Self);
	if (!Horde)
	{
		return EBTNodeResult::Failed;
	}

	// DEFERRED BY ONE TICK, and this is not a stylistic choice. NotifyCourierDelivered destroys the
	// goblin, which unpossesses the controller that owns the behaviour tree component currently
	// executing this very function. Destroying it inline means returning a result into a tree whose
	// owner is being torn down underneath the call stack. SetTimerForNextTick lets this activation
	// finish first, and the frame of delay is invisible.
	//
	// Timer is registered against the SUBSYSTEM, not the goblin: a timer owned by the object it is
	// about to destroy is the same problem one indirection out.
	if (UWorld* World = Self->GetWorld())
	{
		TWeakObjectPtr<AGSHordeGoblin> WeakSelf(Self);
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(Horde, [Horde, WeakSelf]()
			{
				if (AGSHordeGoblin* Goblin = WeakSelf.Get())
				{
					Horde->NotifyCourierDelivered(Goblin);
				}
			}));
	}

	return EBTNodeResult::Succeeded;
}
