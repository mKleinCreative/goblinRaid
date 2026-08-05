#include "Interaction/GSInteractionComponent.h"

#include "Characters/GSCharacterBase.h"
#include "Combat/GSGameplayTags.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/GSCarryComponent.h"
#include "Interaction/GSInteractableComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"

static TAutoConsoleVariable<int32> CVarInteractDebug(
	TEXT("GS.Interact.Debug"),
	0,
	TEXT("1 = draw the interact range, the facing axis, and the focused interactable."),
	ECVF_Cheat);

UGSInteractionComponent::UGSInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	OverlapObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	OverlapObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	OverlapObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	OverlapObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
}

void UGSInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UGSInteractionComponent, bIsInteracting, COND_SkipOwner);
}

void UGSInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AGSCharacterBase* Character = Cast<AGSCharacterBase>(GetOwner()))
	{
		Character->OnHealthChanged.AddDynamic(this, &UGSInteractionComponent::HandleOwnerHealthChanged);
	}
}

void UGSInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AbortChannel(EGSInteractEndReason::Cancelled);

	if (AGSCharacterBase* Character = Cast<AGSCharacterBase>(GetOwner()))
	{
		Character->OnHealthChanged.RemoveDynamic(this, &UGSInteractionComponent::HandleOwnerHealthChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void UGSInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// A simulated proxy gets bIsInteracting replicated to it but owns no channel and needs no prompt,
	// so it does neither: focus scanning is the local player's job, ticking the channel belongs to
	// whoever is actually running one.
	const bool bLocal = IsLocalInteractor();

	if (bIsInteracting)
	{
		if (bLocal || HasChannelAuthority())
		{
			TickChannel(DeltaTime);
		}
	}
	else if (bLocal)
	{
		FocusScanAccumulator += DeltaTime;
		if (FocusScanAccumulator >= FocusScanInterval)
		{
			FocusScanAccumulator = 0.f;
			RefreshFocus();
		}
	}

	if (bLocal && CVarInteractDebug.GetValueOnGameThread() > 0)
	{
		DrawDebugState();
	}
}

void UGSInteractionComponent::HandleOwnerHealthChanged(float /*NewHealth*/, float /*MaxHealth*/, float Delta)
{
	if (Delta < 0.f && bIsInteracting)
	{
		AbortChannel(EGSInteractEndReason::Damaged);
	}
}

bool UGSInteractionComponent::ResolveChannelTarget(UGSInteractableComponent*& OutTarget, bool& bOutPutDown)
{
	OutTarget = nullptr;
	bOutPutDown = false;

	// Focus is consulted FIRST, full hands or not. Preferring put-down whenever something was carried
	// meant a goblin holding a sack could not extract, loot, or foul a well - it made the one thing
	// carrying exists for impossible.
	RefreshFocus();

	const UGSCarryComponent* Carry = GetCarryComponent();
	const bool bHandsFull = Carry && Carry->IsCarrying();

	UGSInteractableComponent* Target = FocusedInteractable.Get();

	// One slot for the slice: a second carryable is not a valid target while the hands are full, or
	// its channel would consume the object (bConsumeOnComplete) and then fail to pick it up.
	const bool bTargetBlockedByFullHands = bHandsFull && IsValid(Target) && Target->IsCarryable();

	if (IsValid(Target) && !bTargetBlockedByFullHands && Target->CanInteract(GetOwner()))
	{
		OutTarget = Target;
		return true;
	}

	if (bHandsFull)
	{
		bOutPutDown = true;
		return true;
	}

	return false;
}

bool UGSInteractionComponent::StartChannelInternal(UGSInteractableComponent* Target, bool bPutDown)
{
	if (bIsInteracting || !IsValid(GetOwner()))
	{
		return false;
	}

	if (bPutDown)
	{
		// Put-down runs the same channel as pick-up, so the verb reads identically to the player and
		// inherits every abort rule for free.
		const UGSCarryComponent* Carry = GetCarryComponent();
		if (!Carry || !Carry->IsCarrying())
		{
			return false;
		}

		ActiveInteractable = nullptr;
		bActivePutDown = true;
		ActiveVerbTag = GSTags::Interact_Carry;
		ChannelDuration = FMath::Max(0.f, Carry->GetPutDownSeconds());
	}
	else
	{
		if (!IsValid(Target) || !Target->CanInteract(GetOwner()))
		{
			return false;
		}

		ActiveInteractable = Target;
		bActivePutDown = false;
		ActiveVerbTag = Target->GetVerbTag();
		ChannelDuration = FMath::Max(0.f, Target->GetChannelSeconds());

		// The contention lock is server state. Taking it on a client would lock nobody out of
		// anything and would desync the moment the server disagreed.
		if (HasChannelAuthority())
		{
			Target->NotifyChannelStarted(GetOwner());
		}
	}

	bIsInteracting = true;
	ChannelProgress = 0.f;

	OnChannelStarted.Broadcast(ActiveInteractable.Get(), ActiveVerbTag, ChannelDuration);

	if (ChannelDuration <= KINDA_SMALL_NUMBER)
	{
		// Instant verbs still go through the framework rather than short-circuiting it - one code
		// path to the payout is worth a frame of pointless bar.
		CompleteChannel();
	}

	return true;
}

bool UGSInteractionComponent::BeginChannel()
{
	if (bIsInteracting || !IsValid(GetOwner()))
	{
		return false;
	}

	UGSInteractableComponent* Target = nullptr;
	bool bPutDown = false;
	if (!ResolveChannelTarget(Target, bPutDown))
	{
		return false;
	}

	if (!StartChannelInternal(Target, bPutDown))
	{
		return false;
	}

	// The server runs the same state machine and owns the payout. On a listen server (and in every
	// single-player PIE session) we are already the authority and no RPC is sent.
	if (!HasChannelAuthority())
	{
		ServerBeginChannel(Target, bPutDown);
	}

	return true;
}

void UGSInteractionComponent::ServerBeginChannel_Implementation(UGSInteractableComponent* Target, bool bPutDown)
{
	if (bIsInteracting)
	{
		return;
	}

	if (!bPutDown)
	{
		// Re-validate the client's pick rather than trusting it. Same slack the abort checks use, so a
		// channel that was legal on the client is not refused here for a few centimetres of lag.
		if (!IsValid(Target))
		{
			return;
		}

		float FacingDot = 0.f;
		float Distance = 0.f;
		EvaluateGeometry(Target, FacingDot, Distance);

		if (Distance > InteractRange + RangeSlack || FacingDot < FacingDotMin - FacingSlack)
		{
			return;
		}
	}

	StartChannelInternal(Target, bPutDown);
}

void UGSInteractionComponent::ServerAbortChannel_Implementation(EGSInteractEndReason Reason)
{
	AbortChannel(Reason);
}

void UGSInteractionComponent::TickChannel(float DeltaTime)
{
	if (!bActivePutDown)
	{
		UGSInteractableComponent* Target = ActiveInteractable.Get();
		if (!IsValid(Target) || !Target->CanInteract(GetOwner()))
		{
			AbortChannel(EGSInteractEndReason::TargetLost);
			return;
		}

		float FacingDot = 0.f;
		float Distance = 0.f;
		EvaluateGeometry(Target, FacingDot, Distance);

		if (Distance > InteractRange + RangeSlack)
		{
			AbortChannel(EGSInteractEndReason::OutOfRange);
			return;
		}

		if (FacingDot < FacingDotMin - FacingSlack)
		{
			AbortChannel(EGSInteractEndReason::LostFacing);
			return;
		}
	}

	if (ChannelDuration > KINDA_SMALL_NUMBER)
	{
		ChannelProgress = FMath::Clamp(ChannelProgress + DeltaTime / ChannelDuration, 0.f, 1.f);
		OnChannelProgress.Broadcast(ChannelProgress);
	}
	else
	{
		ChannelProgress = 1.f;
	}

	if (ChannelProgress >= 1.f)
	{
		CompleteChannel();
	}
}

void UGSInteractionComponent::CompleteChannel()
{
	UGSInteractableComponent* Target = ActiveInteractable.Get();
	AActor* TargetActor = IsValid(Target) ? Target->GetOwner() : nullptr;
	const bool bPutDown = bActivePutDown;
	const bool bCarryable = IsValid(Target) && Target->IsCarryable();
	const bool bAuthority = HasChannelAuthority();
	UGSCarryComponent* Carry = GetCarryComponent();

	// State is cleared BEFORE the payout: an effect is allowed to open a chest, kill a guard, or
	// start another channel, and none of that may land inside the channel that is finishing.
	ClearChannelState();

	// Only the server pays out. The client's copy of this channel exists to drive the HUD bar and to
	// make the press feel immediate; when the two disagree, what the server did is what happened.
	if (bAuthority)
	{
		if (bPutDown)
		{
			if (Carry)
			{
				Carry->PutDown();
			}
		}
		else if (IsValid(Target))
		{
			Target->CompleteInteraction(GetOwner());

			// The hand-off lives here, not on the interactable: a crate has no business knowing what a
			// carrier is.
			if (bCarryable && Carry && IsValid(TargetActor))
			{
				Carry->StartCarry(TargetActor);
			}
		}
	}

	OnChannelEnded.Broadcast(true, EGSInteractEndReason::Completed);
}

void UGSInteractionComponent::AbortChannel(EGSInteractEndReason Reason)
{
	if (!bIsInteracting)
	{
		return;
	}

	// Tell the server before clearing: the authoritative channel has to stop too, or it pays out for
	// a loot the player walked away from.
	if (!HasChannelAuthority() && IsLocalInteractor())
	{
		ServerAbortChannel(Reason);
	}

	if (UGSInteractableComponent* Target = ActiveInteractable.Get())
	{
		// Releasing the contention lock is server state, like taking it.
		if (HasChannelAuthority())
		{
			Target->NotifyChannelAborted(GetOwner());
		}
	}

	ClearChannelState();
	OnChannelEnded.Broadcast(false, Reason);
}

void UGSInteractionComponent::ReleaseInteractInput()
{
	AbortChannel(EGSInteractEndReason::InputReleased);
}

void UGSInteractionComponent::ClearChannelState()
{
	bIsInteracting = false;
	bActivePutDown = false;
	ActiveInteractable = nullptr;
	ActiveVerbTag = FGameplayTag();
	ChannelDuration = 0.f;
	ChannelProgress = 0.f;
}

void UGSInteractionComponent::RefreshFocus()
{
	UGSInteractableComponent* Best = nullptr;
	float BestScore = TNumericLimits<float>::Lowest();

	AActor* Owner = GetOwner();
	if (IsValid(Owner) && !bIsInteracting)
	{
		TArray<AActor*> ActorsToIgnore;
		ActorsToIgnore.Add(Owner);
		if (const UGSCarryComponent* Carry = GetCarryComponent())
		{
			if (AActor* Held = Carry->GetCarriedActor())
			{
				ActorsToIgnore.Add(Held);
			}
		}

		TArray<AActor*> Overlapped;
		UKismetSystemLibrary::SphereOverlapActors(this, Owner->GetActorLocation(), InteractRange,
			OverlapObjectTypes, AActor::StaticClass(), ActorsToIgnore, Overlapped);

		for (AActor* Candidate : Overlapped)
		{
			UGSInteractableComponent* Interactable = IsValid(Candidate) ? Candidate->FindComponentByClass<UGSInteractableComponent>() : nullptr;
			if (!Interactable || !Interactable->CanInteract(Owner))
			{
				continue;
			}

			float FacingDot = 0.f;
			float Distance = 0.f;
			EvaluateGeometry(Interactable, FacingDot, Distance);

			if (Distance > InteractRange || FacingDot < FacingDotMin)
			{
				continue;
			}

			if (bRequireLineOfSight && !HasLineOfSight(Interactable))
			{
				continue;
			}

			// Facing decides, distance only breaks ties - the thing you are looking at wins over the
			// thing you are standing on.
			const float Score = FacingDot - (Distance / FMath::Max(InteractRange, 1.f)) * 0.25f;
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = Interactable;
			}
		}
	}

	SetFocus(Best);
}

void UGSInteractionComponent::SetFocus(UGSInteractableComponent* NewFocus)
{
	if (FocusedInteractable.Get() == NewFocus)
	{
		return;
	}

	FocusedInteractable = NewFocus;
	OnFocusChanged.Broadcast(NewFocus);
}

void UGSInteractionComponent::EvaluateGeometry(const UGSInteractableComponent* Interactable, float& OutFacingDot, float& OutDistance) const
{
	OutFacingDot = -1.f;
	OutDistance = TNumericLimits<float>::Max();

	const AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Interactable)
	{
		return;
	}

	const FVector ToTarget = Interactable->GetInteractionLocation() - Owner->GetActorLocation();
	OutDistance = ToTarget.Size();

	const FVector Flat = ToTarget.GetSafeNormal2D();
	if (!Flat.IsNearlyZero())
	{
		OutFacingDot = FVector::DotProduct(Flat, GetInteractorForward());
	}
	else
	{
		// Standing on top of it - facing is meaningless, so do not fail on it.
		OutFacingDot = 1.f;
	}
}

bool UGSInteractionComponent::HasLineOfSight(const UGSInteractableComponent* Interactable) const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!IsValid(Owner) || !World || !Interactable)
	{
		return false;
	}

	FVector EyeLocation = Owner->GetActorLocation();
	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		EyeLocation = Pawn->GetPawnViewLocation();
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSInteractLOS), false, Owner);
	Params.AddIgnoredActor(Interactable->GetOwner());

	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, EyeLocation, Interactable->GetInteractionLocation(), ECC_Visibility, Params);
	return !bBlocked;
}

FVector UGSInteractionComponent::GetInteractorForward() const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return FVector::ForwardVector;
	}

	if (bUseControlRotationForFacing)
	{
		if (const APawn* Pawn = Cast<APawn>(Owner))
		{
			if (Pawn->Controller)
			{
				return FRotator(0.f, Pawn->GetControlRotation().Yaw, 0.f).Vector();
			}
		}
	}

	return Owner->GetActorForwardVector().GetSafeNormal2D();
}

UGSCarryComponent* UGSInteractionComponent::GetCarryComponent() const
{
	AActor* Owner = GetOwner();
	return IsValid(Owner) ? Owner->FindComponentByClass<UGSCarryComponent>() : nullptr;
}

bool UGSInteractionComponent::IsLocalInteractor() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled();
}

bool UGSInteractionComponent::HasChannelAuthority() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) && Owner->HasAuthority();
}

void UGSInteractionComponent::DrawDebugState() const
{
#if ENABLE_DRAW_DEBUG
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!IsValid(Owner) || !World)
	{
		return;
	}

	const FVector Origin = Owner->GetActorLocation();
	DrawDebugSphere(World, Origin, InteractRange, 16, FColor(80, 160, 255), false, -1.f, 0, 1.f);
	DrawDebugLine(World, Origin, Origin + GetInteractorForward() * InteractRange, FColor::Yellow, false, -1.f, 0, 1.5f);

	if (const UGSInteractableComponent* Focus = FocusedInteractable.Get())
	{
		const FColor FocusColour = bIsInteracting ? FColor::Green : FColor::White;
		DrawDebugLine(World, Origin, Focus->GetInteractionLocation(), FocusColour, false, -1.f, 0, 3.f);
		DrawDebugPoint(World, Focus->GetInteractionLocation(), 12.f, FocusColour, false, -1.f, 0);
	}

	if (bIsInteracting)
	{
		DrawDebugString(World, Origin + FVector(0.f, 0.f, 120.f),
			FString::Printf(TEXT("%s %.0f%%"), *ActiveVerbTag.ToString(), ChannelProgress * 100.f),
			nullptr, FColor::Green, 0.f, true);
	}
#endif
}
