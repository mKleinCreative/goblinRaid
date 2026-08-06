#include "Interaction/GSInteractableComponent.h"

#include "Net/UnrealNetwork.h"

UGSInteractableComponent::UGSInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

bool UGSInteractableComponent::CanInteract_Implementation(AActor* Interactor) const
{
	return bAvailable && Interactor != nullptr;
}

void UGSInteractableComponent::NotifyChannelStarted(AActor* Interactor)
{
	OnInteractionStarted.Broadcast(Interactor);
}

void UGSInteractableComponent::NotifyChannelAborted(AActor* Interactor)
{
	OnInteractionAborted.Broadcast(Interactor);
}

void UGSInteractableComponent::NotifyChannelCompleted(AActor* Interactor)
{
	OnInteractionCompleted.Broadcast(Interactor);
}

void UGSInteractableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGSInteractableComponent, bAvailable);
}
