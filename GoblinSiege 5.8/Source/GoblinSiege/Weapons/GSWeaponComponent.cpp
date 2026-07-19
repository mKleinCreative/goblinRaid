#include "Weapons/GSWeaponComponent.h"
#include "Weapons/GSWeaponDataAsset.h"
#include "Characters/GSCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "TimerManager.h"

UGSWeaponComponent::UGSWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGSWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	// Apply the Blueprint-assigned starting kit through the same path as a runtime swap.
	if (EquippedWeapon)
	{
		UGSWeaponDataAsset* Initial = EquippedWeapon;
		EquippedWeapon = nullptr;
		EquipWeapon(Initial);
	}
}

void UGSWeaponComponent::EquipWeapon(UGSWeaponDataAsset* NewWeapon)
{
	if (!NewWeapon || NewWeapon == EquippedWeapon)
	{
		return;
	}

	EquippedWeapon = NewWeapon;
	bRangedMode = false;
	GrantAbilitiesFromWeapon();
}

void UGSWeaponComponent::GrantAbilitiesFromWeapon()
{
	AGSCharacterBase* OwnerCharacter = Cast<AGSCharacterBase>(GetOwner());
	UAbilitySystemComponent* ASC = OwnerCharacter ? OwnerCharacter->GetAbilitySystemComponent() : nullptr;
	if (!ASC || !EquippedWeapon || !OwnerCharacter->HasAuthority())
	{
		return;
	}

	// Class baseline stats ride on the weapon's init effect (design doc: class == weapon).
	if (EquippedWeapon->InitialAttributesEffect)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(EquippedWeapon);
		FGameplayEffectSpecHandle SpecHandle =
			ASC->MakeOutgoingSpec(EquippedWeapon->InitialAttributesEffect, 1.f, Context);
		if (SpecHandle.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : EquippedWeapon->GrantedAbilities)
	{
		if (AbilityClass)
		{
			ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, GetOwner()));
		}
	}
	// Revoking the previous kit's abilities on swap (ClearAbility on stored handles) is left for
	// the pass that adds a second equippable kit - the slice equips one kit per raid.
}

void UGSWeaponComponent::ToggleRangedMode()
{
	if (bSwapLocked || !EquippedWeapon || !EquippedWeapon->bHasRangedMode)
	{
		return;
	}

	bRangedMode = !bRangedMode;
	OnWeaponModeChanged.Broadcast(bRangedMode);

	bSwapLocked = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SwapLockTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]() { bSwapLocked = false; }),
			SwapInputLockSeconds, false);
	}
}

void UGSWeaponComponent::AddBloodOrb()
{
	BloodOrbs = FMath::Min(BloodOrbs + 1, MaxBloodOrbs);
	OnOrbCountChanged.Broadcast(BloodOrbs);
}

bool UGSWeaponComponent::ConsumeBloodOrbs(int32 Count)
{
	if (Count <= 0 || BloodOrbs < Count)
	{
		return false;
	}

	BloodOrbs -= Count;
	OnOrbCountChanged.Broadcast(BloodOrbs);
	return true;
}
