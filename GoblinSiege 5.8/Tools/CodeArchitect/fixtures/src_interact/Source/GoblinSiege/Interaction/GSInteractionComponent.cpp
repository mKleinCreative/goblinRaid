#include "Interaction/GSInteractionComponent.h"

#include "Characters/GSCharacterBase.h"
#include "Interaction/GSInteractableComponent.h"
#include "Kismet/KismetSystemLibrary.h"

UGSInteractionComponent::UGSInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f; // per-frame: the channel bar and facing checks want it
}

void UGSInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AGSCharacterBase* Char = Cast<AGSCharacterBase>(GetOwner()))
	{
		// The damage-abort rule rides the delegate that exists for exactly this kind of listener.
		Char->OnHealthChanged.AddDynamic(this, &UGSInteractionComponent::HandleOwnerHealthChanged);
	}
}

float UGSInteractionComponent::GetChannelProgress01() const
{
	return (Active && Active->ChannelSeconds > 0.f) ? FMath::Clamp(ChannelElapsed / Active->ChannelSeconds, 0.f, 1.f) : 0.f;
}

UGSInteractableComponent* UGSInteractionComponent::FindBestInteractable() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}
	TArray<AActor*> Overlaps;
	UKismetSystemLibrary::SphereOverlapActors(
		Owner, Owner->GetActorLocation(), SearchRadius,
		{ UEngineTypes::ConvertToObjectType(ECC_WorldDynamic), UEngineTypes::ConvertToObjectType(ECC_WorldStatic), UEngineTypes::ConvertToObjectType(ECC_Pawn) },
		nullptr, { Owner }, Overlaps);

	UGSInteractableComponent* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	const FVector From = Owner->GetActorLocation();
	const FVector Facing = Owner->GetActorForwardVector();
	for (AActor* Candidate : Overlaps)
	{
		UGSInteractableComponent* Interactable = Candidate->FindComponentByClass<UGSInteractableComponent>();
		if (!Interactable || !Interactable->CanInteract(Owner))
		{
			continue;
		}
		const FVector To = Candidate->GetActorLocation() - From;
		const float Dist = To.Size();
		if (Dist > Interactable->MaxRange)
		{
			continue;
		}
		const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Facing, To.GetSafeNormal())));
		if (AngleDeg > Interactable->FacingHalfAngleDeg)
		{
			continue;
		}
		// Most-centered first, then nearest: reads as "the one I'm looking at".
		const float Score = AngleDeg * 10.f + Dist;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Interactable;
		}
	}
	return Best;
}

bool UGSInteractionComponent::StillEligible(const UGSInteractableComponent* Interactable) const
{
	const AActor* Owner = GetOwner();
	const AActor* Target = Interactable ? Interactable->GetOwner() : nullptr;
	if (!Owner || !Target || !Interactable->CanInteract(const_cast<AActor*>(Owner)))
	{
		return false;
	}
	const FVector To = Target->GetActorLocation() - Owner->GetActorLocation();
	if (To.Size() > Interactable->MaxRange)
	{
		return false; // range break — stealth-spec abort rule
	}
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Owner->GetActorForwardVector(), To.GetSafeNormal())));
	return AngleDeg <= Interactable->FacingHalfAngleDeg; // facing break — abort rule
}

bool UGSInteractionComponent::BeginInteract()
{
	if (Active)
	{
		return true; // already channeling — idempotent under input repeat
	}
	UGSInteractableComponent* Target = FindBestInteractable();
	if (!Target)
	{
		return false;
	}
	Active = Target;
	ChannelElapsed = 0.f;
	Active->NotifyChannelStarted(GetOwner());
	OnChannelProgress.Broadcast(Active, 0.f);
	return true;
}

void UGSInteractionComponent::EndInteract()
{
	if (Active)
	{
		Abort(); // release before completion is an abort by definition (hold-E, not press-E)
	}
}

void UGSInteractionComponent::HandleOwnerHealthChanged(float /*NewHealth*/, float /*MaxHealth*/, float Delta)
{
	if (Active && Delta < 0.f)
	{
		Abort(); // damage abort — "interruptible like every channel"
	}
}

void UGSInteractionComponent::Abort()
{
	UGSInteractableComponent* Was = Active;
	Active = nullptr;
	ChannelElapsed = 0.f;
	if (Was)
	{
		Was->NotifyChannelAborted(GetOwner());
		OnChannelAborted.Broadcast(Was);
	}
}

void UGSInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Focus maintenance for the prompt, cheap enough per-frame at slice scale.
	UGSInteractableComponent* NewFocus = Active ? Active.Get() : FindBestInteractable();
	if (NewFocus != Focused.Get())
	{
		Focused = NewFocus;
		OnFocusChanged.Broadcast(NewFocus);
	}

	if (!Active)
	{
		return;
	}
	if (!StillEligible(Active))
	{
		Abort();
		return;
	}
	ChannelElapsed += DeltaTime;
	const float Progress = GetChannelProgress01();
	OnChannelProgress.Broadcast(Active, Progress);
	if (Progress >= 1.f)
	{
		UGSInteractableComponent* Done = Active;
		Active = nullptr;
		ChannelElapsed = 0.f;
		Done->NotifyChannelCompleted(GetOwner());
		OnChannelCompleted.Broadcast(Done);
	}
}
