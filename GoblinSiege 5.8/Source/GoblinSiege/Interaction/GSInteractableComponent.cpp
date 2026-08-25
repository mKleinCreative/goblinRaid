#include "Interaction/GSInteractableComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Combat/GSGameplayTags.h"

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
	// Availability is the ONLY thing this adds over CanFocus. Kept as an explicit early-out rather
	// than folded into the expression below, because "you may look at it but not use it" is the whole
	// distinction #187 rests on and it should be readable at a glance.
	if (!bIsAvailable)
	{
		return false;
	}

	return CanFocus(Interactor);
}

bool UGSInteractableComponent::CanFocus_Implementation(AActor* Interactor) const
{
	if (!IsValid(GetOwner()) || !IsValid(Interactor))
	{
		return false;
	}

	// One channel at a time; first goblin to start it owns it until they abort or finish. Excluded
	// from focus as well as from interaction: an interactable another goblin is mid-channel on should
	// not take your prompt, since the prompt would be offering something you cannot have.
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

void UGSInteractableComponent::InitialiseAsCarryable(int32 InLootValue, const FText& InPrompt,
	float InChannelSeconds, const FVector& InInteractionOffset)
{
	// LIFTS THE INTERACTION POINT OFF THE FLOOR, and it is not cosmetic. UGSInteractionComponent
	// line-of-sight traces from the eye to this point, and a decorative animal's actor origin sits
	// exactly ON the terrain - so the trace hits Landscape ~15 uu short and the focus is refused,
	// silently, on an actor whose every other field is correct. Measured on A_Pig_SitLoop7_5.
	InteractionOffset = InInteractionOffset;

	// Matched to BP_Livestock_Pig rather than invented, so a pig converted in place and an authored
	// one behave identically.
	VerbTag = GSTags::Interact_Carry;
	LootValue = FMath::Max(0, InLootValue);
	PromptText = InPrompt;
	ChannelSeconds = FMath::Max(0.f, InChannelSeconds);
	bIsCarryable = true;

	// NOT consumed on completion: livestock is picked up and carried, not made to disappear. The
	// component's default is true because it was written for chests.
	bConsumeOnComplete = false;
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

	// A carryable is never CONSUMED by being picked up - it is relocated, and the whole texture of the
	// courier run is picking a thing up, setting it down to fight, and picking it up again.
	// bConsumeOnComplete means "used up" (a looted chest, a fouled well on its last use); applied to a
	// carryable it silently means "usable exactly once", because CanInteract refuses an unavailable
	// target forever afterwards. The flag defaults to true, so every carryable in the project would
	// have inherited that by accident. Carryable wins over the flag rather than requiring every pig,
	// sack and prisoner to remember to clear it (#161).
	if (bConsumeOnComplete && !bIsCarryable)
	{
		SetAvailable(false);
	}

	if (bCollapseOnComplete)
	{
		ApplyCollapse();
	}
}

void UGSInteractableComponent::ApplyCollapse()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	// The root primitive, not "the first primitive found": a container's root IS its mesh here, and
	// picking an arbitrary child would topple a decoration while the body stayed put.
	UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
	if (!Body)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GS.Interact] %s has bCollapseOnComplete but its root is not a "
			"primitive component - nothing to simulate. Make the mesh the root."), *Owner->GetName());
		return;
	}

	// Physics needs real collision. A container set to query-only (or no collision) simulates into
	// the floor and through it, which looks worse than not collapsing at all - so widen it first.
	if (Body->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics)
	{
		Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	Body->SetSimulatePhysics(true);

	// Impulse scaled by mass so a heavy chest and a light crate topple at the same rate - an
	// unscaled impulse launches light props across the room. Sideways and slightly up, seeded off
	// the actor's own yaw so it does not always fall the same way in a row of containers.
	const FVector Shove = Owner->GetActorForwardVector() * CollapseImpulse
		+ FVector(0.f, 0.f, CollapseImpulse * 0.25f);
	Body->AddImpulse(Shove, NAME_None, /*bVelChange=*/true);

	UE_LOG(LogTemp, Log, TEXT("[GS.Interact] %s collapsed on loot."), *Owner->GetName());
}

void UGSInteractableComponent::ReceiveInteractionCompleted_Implementation(AActor* /*Interactor*/)
{
	// Intentionally empty - the payout belongs to the system that owns the verb.
}
