#include "AI/Tasks/BTTask_PickUpCargo.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Interaction/GSCarryComponent.h"
#include "Interaction/GSInteractableComponent.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSPickUpCargo, Log, All);

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
	const float Dist = FVector::Dist2D(Cargo->GetActorLocation(), Self->GetActorLocation());
	if (Dist > PickUpRange)
	{
		UE_LOG(LogGSPickUpCargo, Log, TEXT("[GS.PickUp] %s: '%s' is %.0fuu away, need <= %.0fuu - not in range yet."),
			*Self->GetName(), *Cargo->GetName(), Dist, PickUpRange);
		return EBTNodeResult::Failed;
	}

	// NOT EVERY LOOT SUBJECT IS CARRYABLE. StartCarry itself has no such gate - it will attach anything
	// handed to it - so without this check an ordered crate got shouldered whole, unbroken, LootValue
	// and all, instead of going through UBTTask_SmashOrderTarget + UBTTask_LootInPlace the way a
	// container is meant to. A subject with an interactable component that says it is not carryable
	// fails here and falls to the tree's other Loot branch; a subject with no interactable component at
	// all (nothing has been placed to guard against yet) is left alone rather than guessed at.
	if (const UGSInteractableComponent* Interactable = Cargo->FindComponentByClass<UGSInteractableComponent>())
	{
		if (!Interactable->IsCarryable())
		{
			UE_LOG(LogGSPickUpCargo, Log, TEXT("[GS.PickUp] %s: '%s' is not carryable - falling to the smash-and-loot branch."),
				*Self->GetName(), *Cargo->GetName());
			return EBTNodeResult::Failed;
		}
	}

	UGSCarryComponent* Carry = Self->FindComponentByClass<UGSCarryComponent>();
	if (!Carry)
	{
		// Fails rather than crashes, and this is the honest state for anything that is not an
		// AGSHordeGoblin - the component was added to that class in #141 and to nothing else.
		UE_LOG(LogGSPickUpCargo, Warning, TEXT("[GS.PickUp] %s: no UGSCarryComponent - not an AGSHordeGoblin?"),
			*Self->GetName());
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
		UE_LOG(LogGSPickUpCargo, Warning, TEXT("[GS.PickUp] %s: StartCarry('%s') refused (already carried by someone else?)."),
			*Self->GetName(), *Cargo->GetName());
		return EBTNodeResult::Failed;
	}

	UE_LOG(LogGSPickUpCargo, Log, TEXT("[GS.PickUp] %s picked up '%s'."), *Self->GetName(), *Cargo->GetName());
	BB->SetValueAsObject(CargoKey.SelectedKeyName, Cargo);
	return EBTNodeResult::Succeeded;
}
