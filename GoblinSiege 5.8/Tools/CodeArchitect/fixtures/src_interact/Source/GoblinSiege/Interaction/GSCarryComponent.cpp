#include "Interaction/GSCarryComponent.h"

#include "AbilitySystemComponent.h"
#include "Characters/GSCharacterBase.h"
#include "Combat/GSGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

UGSCarryComponent::UGSCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

bool UGSCarryComponent::PickUp(AActor* Cargo, float SpeedMultiplier, bool bAllowsCombat)
{
	if (!Cargo || CarriedActor)
	{
		return false; // one cargo at a time; the Brute's double-carry is a second component slot later
	}
	ACharacter* Char = Cast<ACharacter>(GetOwner());
	if (!Char)
	{
		return false;
	}
	CarriedActor = Cargo;
	ActiveSpeedMultiplier = SpeedMultiplier;
	bCargoAllowsCombat = bAllowsCombat;

	Cargo->AttachToComponent(Char->GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale, CarrySocketName);
	Cargo->SetActorEnableCollision(false);

	ApplySpeed(SpeedMultiplier);
	if (!bAllowsCombat)
	{
		ApplyCarryTag(true); // chickens: no tag, fight one-handed (design doc §9)
	}
	OnCarryChanged.Broadcast(Cargo);
	return true;
}

AActor* UGSCarryComponent::Drop()
{
	if (!CarriedActor)
	{
		return nullptr;
	}
	AActor* Cargo = CarriedActor;
	CarriedActor = nullptr;

	Cargo->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Cargo->SetActorEnableCollision(true);

	ApplySpeed(1.f / FMath::Max(ActiveSpeedMultiplier, KINDA_SMALL_NUMBER));
	ActiveSpeedMultiplier = 1.f;
	if (!bCargoAllowsCombat)
	{
		ApplyCarryTag(false);
	}
	bCargoAllowsCombat = false;
	OnCarryChanged.Broadcast(nullptr);
	return Cargo;
}

void UGSCarryComponent::ApplyCarryTag(bool bCarrying)
{
	AGSCharacterBase* Char = Cast<AGSCharacterBase>(GetOwner());
	UAbilitySystemComponent* ASC = Char ? Char->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}
	if (bCarrying)
	{
		ASC->AddLooseGameplayTag(GSTags::State_Carrying);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(GSTags::State_Carrying);
	}
}

void UGSCarryComponent::ApplySpeed(float Multiplier)
{
	// Multiplicative against whatever GAS/weapon speed is current, and exactly inverted on Drop —
	// deliberately NOT a MaxWalkSpeed overwrite, which would fight the MoveSpeed attribute.
	if (const ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		UCharacterMovementComponent* Move = Char->GetCharacterMovement();
		Move->MaxWalkSpeed *= Multiplier;
		Move->MaxWalkSpeedCrouched *= Multiplier;
	}
}

void UGSCarryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGSCarryComponent, CarriedActor);
}
