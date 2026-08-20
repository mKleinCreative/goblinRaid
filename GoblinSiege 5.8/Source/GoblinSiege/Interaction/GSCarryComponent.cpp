#include "Interaction/GSCarryComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Characters/GSCharacterBase.h"
#include "Combat/GSGE_MoveSpeedScalar.h"
#include "Combat/GSGameplayTags.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"

UGSCarryComponent::UGSCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	CarrySlowEffectClass = UGSGE_MoveSpeedScalar::StaticClass();
}

void UGSCarryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UGSCarryComponent, CarriedActor);
}

void UGSCarryComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AGSCharacterBase* Character = Cast<AGSCharacterBase>(GetOwner()))
	{
		Character->OnDied.AddDynamic(this, &UGSCarryComponent::HandleOwnerDied);
	}
}

void UGSCarryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AGSCharacterBase* Character = Cast<AGSCharacterBase>(GetOwner()))
	{
		Character->OnDied.RemoveDynamic(this, &UGSCarryComponent::HandleOwnerDied);
	}

	if (EndPlayReason != EEndPlayReason::EndPlayInEditor && EndPlayReason != EEndPlayReason::Quit)
	{
		PutDown();
	}

	Super::EndPlay(EndPlayReason);
}

void UGSCarryComponent::HandleOwnerDied()
{
	PutDown();
}

void UGSCarryComponent::DestroyCarried()
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority() || !IsCarrying())
	{
		return;
	}

	// PutDown FIRST, then destroy what it hands back. Going straight to Destroy() would leave the
	// carrier's own state - the attach, the move-speed effect, the replicated CarriedActor - pointing
	// at a dead actor and relying on the null checks downstream to paper over it. PutDown is the one
	// path that unwinds all of that, and it returns the actor precisely so a caller can decide its
	// fate.
	AActor* Dropped = PutDown();
	if (IsValid(Dropped))
	{
		UE_LOG(LogTemp, Log, TEXT("[GoblinSiege] %s lost '%s' - destroyed rather than dropped."),
			*GetNameSafe(GetOwner()), *Dropped->GetName());
		Dropped->Destroy();
	}
}

bool UGSCarryComponent::StartCarry(AActor* Object)
{
	if (!IsValid(Object) || IsCarrying() || !IsValid(GetOwner()))
	{
		return false;
	}

	// Authority owns the carry. A client writing CarriedActor would look right for one frame and then
	// be stomped by replication, with the server none the wiser.
	if (!GetOwner()->HasAuthority())
	{
		return false;
	}

	CarriedActor = Object;
	SuppressCarriedCollision(Object);
	AttachCarried();
	ApplyCarryState();

	OnCarriedActorChanged.Broadcast(CarriedActor);
	return true;
}

AActor* UGSCarryComponent::PutDown()
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return nullptr;
	}

	AActor* Object = CarriedActor;
	CarriedActor = nullptr;

	if (IsValid(Object))
	{
		Object->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Object->SetActorLocation(FindDropLocation(), false, nullptr, ETeleportType::TeleportPhysics);
		RestoreCarriedCollision(Object);
	}

	ClearCarryState();
	OnCarriedActorChanged.Broadcast(nullptr);
	return Object;
}

void UGSCarryComponent::OnRep_CarriedActor(AActor* OldCarried)
{
	if (IsValid(CarriedActor))
	{
		SuppressCarriedCollision(CarriedActor);
		AttachCarried();
		ApplyCarryState();
	}
	else
	{
		if (IsValid(OldCarried))
		{
			OldCarried->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			RestoreCarriedCollision(OldCarried);
		}
		ClearCarryState();
	}

	OnCarriedActorChanged.Broadcast(CarriedActor);
}

void UGSCarryComponent::AttachCarried()
{
	AActor* Object = CarriedActor;
	if (!IsValid(Object) || !IsValid(GetOwner()))
	{
		return;
	}

	USceneComponent* AttachTo = nullptr;
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		AttachTo = Character->GetMesh();
	}
	if (!AttachTo)
	{
		AttachTo = GetOwner()->GetRootComponent();
	}
	if (!AttachTo)
	{
		return;
	}

	// A missing socket is NOT a soft failure. AttachToComponent with a name the mesh does not carry
	// attaches at the component ORIGIN and returns success - so the sack rides at the goblin's feet,
	// looking like a physics bug or a bad pivot rather than a naming problem. Verified 2026-08-14:
	// "CarrySocket" exists on NONE of the 25 skeletal meshes under /Game, including the livestock,
	// so every carry this project has ever done would have attached to the floor. GSHordeGoblin.h:74
	// already called this a "known cosmetic gap"; it is not cosmetic, it is the visible half of the
	// courier run. Warn once per attach, name all three parties, and attach anyway - the fallback is
	// deliberate so the feature still functions while the socket is missing (#161).
	const bool bHasSocket = CarrySocketName.IsNone() || AttachTo->DoesSocketExist(CarrySocketName);
	if (!bHasSocket)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GS.Carry] Socket '%s' does not exist on %s (owner %s) - "
			"attaching '%s' at the component origin instead, which will look like it is at the "
			"character's feet. Add the socket to the SKELETON (GOB_Scout_v2_Skeleton, parented to "
			"Spine02) so player and horde goblin both get it, or clear CarrySocketName to attach at "
			"the origin on purpose."),
			*CarrySocketName.ToString(), *AttachTo->GetName(), *GetOwner()->GetName(),
			*Object->GetName());
	}

	Object->AttachToComponent(AttachTo, FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		bHasSocket ? CarrySocketName : NAME_None);
	Object->SetActorRelativeTransform(CarryRelativeTransform);
}

void UGSCarryComponent::ApplyCarryState()
{
	if (bCarryStateApplied)
	{
		return;
	}
	bCarryStateApplied = true;

	UAbilitySystemComponent* ASC = GetOwnerASC();
	if (!ASC)
	{
		return;
	}

	// Loose rather than a GameplayEffect: the tag's entire job is to be present for exactly as long
	// as the object is held, and the object - not a duration - is the authority on that. Loose tags
	// do not replicate, which is why OnRep_CarriedActor routes through here on clients as well.
	ASC->AddLooseGameplayTag(GSTags::State_Carrying);

	// The slow, unlike the tag, IS replicated: a GameplayEffect on MoveSpeedMultiplier, applied only
	// by the server. Clients see the attribute change and re-derive their walk speeds from it, so a
	// carried sack looks equally heavy on every machine without a second code path.
	if (IsValid(GetOwner()) && GetOwner()->HasAuthority() && CarrySlowEffectClass)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);

		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(CarrySlowEffectClass, 1.f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(GSTags::Data_MoveSpeedScalar, CarrySpeedMultiplier);
			CarrySlowHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	}
}

void UGSCarryComponent::ClearCarryState()
{
	if (!bCarryStateApplied)
	{
		return;
	}
	bCarryStateApplied = false;

	if (UAbilitySystemComponent* ASC = GetOwnerASC())
	{
		ASC->RemoveLooseGameplayTag(GSTags::State_Carrying);

		// Removing by handle, not by class: another slow from an unrelated source is none of our
		// business, and it must survive putting the sack down.
		if (CarrySlowHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(CarrySlowHandle);
		}
	}

	CarrySlowHandle = FActiveGameplayEffectHandle();
}

void UGSCarryComponent::SuppressCarriedCollision(AActor* Object)
{
	if (!IsValid(Object))
	{
		return;
	}

	bCarriedSimulatedPhysics = false;
	if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Object->GetRootComponent()))
	{
		bCarriedSimulatedPhysics = Root->IsSimulatingPhysics();
		if (bCarriedSimulatedPhysics)
		{
			Root->SetSimulatePhysics(false);
		}
	}

	Object->SetActorEnableCollision(false);
}

void UGSCarryComponent::RestoreCarriedCollision(AActor* Object)
{
	if (!IsValid(Object))
	{
		return;
	}

	Object->SetActorEnableCollision(true);

	if (bCarriedSimulatedPhysics)
	{
		if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Object->GetRootComponent()))
		{
			Root->SetSimulatePhysics(true);
		}
	}
	bCarriedSimulatedPhysics = false;
}

FVector UGSCarryComponent::FindDropLocation() const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return FVector::ZeroVector;
	}

	const FVector Ahead = Owner->GetActorLocation() + Owner->GetActorForwardVector() * DropForwardOffset;
	UWorld* World = GetWorld();
	if (!bDropTraceToGround || !World)
	{
		return Ahead;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSCarryDrop), false, Owner);
	Params.AddIgnoredActor(CarriedActor);

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Ahead + FVector(0.f, 0.f, 100.f), Ahead - FVector(0.f, 0.f, 300.f), ECC_Visibility, Params))
	{
		return Hit.ImpactPoint + FVector(0.f, 0.f, 5.f);
	}

	return Ahead;
}

UAbilitySystemComponent* UGSCarryComponent::GetOwnerASC() const
{
	if (const IAbilitySystemInterface* AbilityInterface = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		return AbilityInterface->GetAbilitySystemComponent();
	}
	return nullptr;
}
