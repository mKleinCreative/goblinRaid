#include "Attributes/GSAttributeSetBase.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Characters/GSCharacterBase.h"
#include "ACFStatisticsSet.h"
#include "AbilitySystemComponent.h"

UGSAttributeSetBase::UGSAttributeSetBase()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitArmor(0.f);
	InitMoveSpeedMultiplier(1.f);
	InitIncomingDamage(0.f);
}

void UGSAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetMoveSpeedMultiplierAttribute())
	{
		// Roots clamp to a floor rather than 0 so stacked slows never hard-freeze a character.
		//
		// CEILING RAISED 3 -> 6 (#348). The floor is the interesting half of this clamp and is
		// unchanged; the ceiling was silently capping the dodge. UGSGA_DodgeRoll lifts the walk
		// speed for the roll through this attribute, and a multiplier of 6 or 12 both landed here
		// and came out as 3 - measured live as MaxWalkSpeed 470 -> 1410 twice, from two different
		// requested values, which is what gave the clamp away. Nothing else in the project asks for
		// more than 3, so this widens a ceiling nobody else is touching rather than changing any
		// existing behaviour.
		NewValue = FMath::Clamp(NewValue, 0.1f, 6.f);
	}
}

void UGSAttributeSetBase::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// IncomingDamage is a meta attribute: written only by GSDamageExecCalculation, drained into
	// Health here, never read anywhere else (see header comment).
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = GetIncomingDamage();
		SetIncomingDamage(0.f);

		if (Damage > 0.f)
		{
			// Hand the attacker to the target before the write. This is the last point where the
			// effect context still exists - the Health attribute-change delegate that drives
			// hit reactions receives GEModData as null on a base-value write, so anything that
			// needs to know WHO hit you has to be captured here or not at all.
			if (AGSCharacterBase* TargetCharacter =
					Cast<AGSCharacterBase>(Data.Target.AbilityActorInfo.IsValid()
						? Data.Target.AbilityActorInfo->AvatarActor.Get() : nullptr))
			{
				TargetCharacter->SetPendingDamageInstigator(
					const_cast<AActor*>(Data.EffectSpec.GetContext().GetInstigator()));
			}

			// ARS OWNS HEALTH AS OF #228. The damage lands on UACFStatisticsSet::Health, not on this
			// set's Health, which is now a read-only mirror kept in step by
			// AGSCharacterBase::HandleHealthChanged.
			//
			// CLAMPED TO EXACTLY ZERO ON PURPOSE. ACF's death trigger is
			// UACFGASStatisticsComponent::HandleHealthReachesZero, and it tests
			// `Data.NewValue == 0.f` - an exact float comparison
			// (ACFGASStatisticsComponent.cpp:496). A clamp that left 0.0001 health would leave the
			// character alive at zero HP forever, with nothing in any log to say why.
			if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
			{
				const FGameplayAttribute HealthAttr = GetDefault<UACFStatisticsSet>()->HealthAttribute();
				const FGameplayAttribute MaxAttr = GetDefault<UACFStatisticsSet>()->MaxHealthAttribute();
				bool bFound = false;
				const float Current = ASC->GetGameplayAttributeValue(HealthAttr, bFound);
				if (bFound)
				{
					const float Max = ASC->GetGameplayAttributeValue(MaxAttr, bFound);
					ASC->SetNumericAttributeBase(HealthAttr, FMath::Clamp(Current - Damage, 0.f, Max));
				}
			}
			// Death is ACF's now: draining the attribute above fires its health-reaches-zero
			// delegate, which ends at AACFCharacter::HandleCharacterDeath. AGSCharacterBase binds
			// its own consequences to UACFDamageHandlerComponent::OnOwnerDeath rather than watching
			// health itself.
		}
	}
}

void UGSAttributeSetBase::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGSAttributeSetBase, Health, OldValue);
}

void UGSAttributeSetBase::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGSAttributeSetBase, MaxHealth, OldValue);
}

void UGSAttributeSetBase::OnRep_Armor(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGSAttributeSetBase, Armor, OldValue);
}

void UGSAttributeSetBase::OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGSAttributeSetBase, MoveSpeedMultiplier, OldValue);
}

void UGSAttributeSetBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UGSAttributeSetBase, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGSAttributeSetBase, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGSAttributeSetBase, Armor, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGSAttributeSetBase, MoveSpeedMultiplier, COND_None, REPNOTIFY_Always);
	// IncomingDamage is intentionally not replicated (meta attribute).
}
