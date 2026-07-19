// Shared attribute set for every character - goblins and the three defender races alike, so the
// damage pipeline (GSDamageExecCalculation, Combat module) only has to know one attribute layout.
// Per-race/per-archetype differences (HP pools, armor, speed) come from data (GSRaceDataAsset /
// GSWeaponDataAsset) applied as GameplayEffects at spawn, not from separate attribute set classes.
#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GSAttributeSetBase.generated.h"

// Note: GAMEPLAYATTRIBUTE_PROPERTY_GETTER / _VALUE_GETTER / _VALUE_SETTER / _VALUE_INITTER are
// already defined by the engine in AttributeSet.h (included above) - do not redefine them here
// (was causing C4005 macro-redefinition warnings).

/**
 * Base attribute set. Armor is a flat mitigation value consumed by GSDamageExecCalculation,
 * which also applies the race/weapon matchup multiplier matrix (race-design-*.md) before armor.
 * IncomingDamage is a "meta" attribute: it is never read directly, only written to by a
 * GameplayEffect execution, then immediately drained from Health in PostGameplayEffectExecute.
 */
UCLASS()
class GOBLINSIEGE_API UGSAttributeSetBase : public UAttributeSet
{
	GENERATED_BODY()

public:
	UGSAttributeSetBase();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes")
	FGameplayAttributeData Health;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UGSAttributeSetBase, Health)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Health)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Health)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes")
	FGameplayAttributeData MaxHealth;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UGSAttributeSetBase, MaxHealth)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(MaxHealth)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(MaxHealth)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(MaxHealth)

	/** Flat mitigation, e.g. Knight = 6 (design doc §5). Explosive/blast damage types skip this entirely. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Armor, Category = "Attributes")
	FGameplayAttributeData Armor;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UGSAttributeSetBase, Armor)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Armor)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Armor)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Armor)

	/** Multiplier applied to base movement speed - lets abilities/effects (roots, slows, buffs) stack cleanly. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeedMultiplier, Category = "Attributes")
	FGameplayAttributeData MoveSpeedMultiplier;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UGSAttributeSetBase, MoveSpeedMultiplier)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(MoveSpeedMultiplier)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(MoveSpeedMultiplier)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(MoveSpeedMultiplier)

	/** Meta attribute: written by damage GameplayEffects, drained into Health, never replicated directly. */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData IncomingDamage;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UGSAttributeSetBase, IncomingDamage)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(IncomingDamage)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(IncomingDamage)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(IncomingDamage)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Armor(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue);
};
