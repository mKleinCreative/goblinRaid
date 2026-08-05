#include "Attributes/GSAttributeSetBase.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Characters/GSCharacterBase.h"

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
		NewValue = FMath::Clamp(NewValue, 0.1f, 3.f);
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

			SetHealth(FMath::Clamp(GetHealth() - Damage, 0.f, GetMaxHealth()));
			// Death detection lives in AGSCharacterBase::HandleHealthChanged via the Health
			// attribute-changed delegate - not here - so AI and players share one code path.
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
