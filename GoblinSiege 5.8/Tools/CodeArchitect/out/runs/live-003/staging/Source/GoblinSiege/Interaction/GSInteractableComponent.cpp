#include "Interaction/GSInteractableComponent.h"

#include "Net/UnrealNetwork.h"

UGSInteractableComponent::UGSInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGSInteractableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UGSInteractableComponent, bIsAvailable);
}

bool UGSInteractableComponent::CanInteract_Implementation(AActor* Interactor) const
{
	if (!bIsAvailable || !IsValid(GetOwner()) || !IsValid(Interactor))
	{
		return false;
	}

	// One channel at a time; first goblin to start it owns it until they abort or finish.
	return !ChannellingInteractor.IsValid() || ChannellingInteractor.Get() == Interactor;
}

FVector UGSInteractableComponent::GetInteractionLocation() const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return FVector::ZeroVector;
	}

	return Owner->GetActorLocation() + Owner->GetActorRotation().RotateVector(InteractionOffset);
}

void UGSInteractableComponent::SetAvailable(bool bNewAvailable)
{
	// Availability is replicated state, so only the server may set it; clients learn through OnRep.
	// A client that flipped this locally would show a looted chest to one player and a full one to
	// everyone else, until the next update quietly disagreed with them.
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (bIsAvailable == bNewAvailable)
	{
		return;
	}

	bIsAvailable = bNewAvailable;
	if (!bIsAvailable)
	{
		ChannellingInteractor.Reset();
	}

	// Server takes the same path clients take, so the prompt updates identically on both.
	OnRep_IsAvailable();
}

void UGSInteractableComponent::OnRep_IsAvailable()
{
	OnAvailabilityChanged.Broadcast(this, bIsAvailable);
}

void UGSInteractableComponent::NotifyChannelStarted(AActor* Interactor)
{
	ChannellingInteractor = Interactor;
}

void UGSInteractableComponent::NotifyChannelAborted(AActor* Interactor)
{
	if (ChannellingInteractor.Get() == Interactor)
	{
		ChannellingInteractor.Reset();
	}

	OnInteractionAborted.Broadcast(this, Interactor);
}

void UGSInteractableComponent::CompleteInteraction(AActor* Interactor)
{
	// Server only - this is the payout. UGSInteractionComponent already gates the call; the guard is
	// here too because this is the one function in the framework that changes the world.
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return;
	}

	ChannellingInteractor.Reset();

	ReceiveInteractionCompleted(Interactor);
	OnInteractionCompleted.Broadcast(this, Interactor);

	if (bConsumeOnComplete)
	{
		SetAvailable(false);
	}
}

void UGSInteractableComponent::ReceiveInteractionCompleted_Implementation(AActor* /*Interactor*/)
{
	// Intentionally empty - the payout belongs to the system that owns the verb.
}
