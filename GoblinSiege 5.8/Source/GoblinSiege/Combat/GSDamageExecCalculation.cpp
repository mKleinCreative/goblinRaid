#include "Combat/GSDamageExecCalculation.h"
#include "Combat/GSGameplayTags.h"
#include "Combat/GSRaceDataAsset.h"
#include "Attributes/GSAttributeSetBase.h"
#include "Characters/GSEnemyCharacter.h"
#include "AbilitySystemComponent.h"

/** SetByCaller tag the attacking GameplayAbility must set: EffectSpec.SetSetByCallerMagnitude(
 *  GSTags::Damage_Dagger (or Bow/Greatclub/ShadowMagic/BloodMagic/Fire/Blast), RawDamageValue). */
static const FGameplayTag& GetRawDamageSetByCallerTagForType(const FGameplayTagContainer& AssetTags)
{
	// The attacking ability tags its GameplayEffectSpec with exactly one Damage.* asset tag; that
	// same tag doubles as the SetByCaller key, so the exec calc only needs to know the enum of
	// possible types, not a separate lookup table.
	static const TArray<FGameplayTag> DamageTypeTags = {
		GSTags::Damage_Dagger, GSTags::Damage_Bow, GSTags::Damage_Greatclub,
		GSTags::Damage_ShadowMagic, GSTags::Damage_BloodMagic, GSTags::Damage_Fire, GSTags::Damage_Blast
	};

	for (const FGameplayTag& Tag : DamageTypeTags)
	{
		if (AssetTags.HasTagExact(Tag))
		{
			return Tag;
		}
	}
	return FGameplayTag::EmptyTag;
}

UGSDamageExecCalculation::UGSDamageExecCalculation()
{
	// No FGameplayEffectAttributeCaptureDefinition members are declared for Armor here because we
	// read it directly off the target's AttributeSetBase in Execute_Implementation instead of via
	// the capture-definition indirection - keeps this scaffold readable. A production
	// implementation should add a proper FProperty-based capture definition for Armor (and
	// snapshot=false so late armor buffs/debuffs are respected) instead of the direct read below.
}

void UGSDamageExecCalculation::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	// The attacking ability calls Spec.AddDynamicAssetTag(GSTags::Damage_Fire) (etc.) before
	// commit, so exactly one Damage.* tag lives here alongside SetByCaller magnitude - simpler and
	// more idiomatic than reading captured source tags for this scaffold's purposes.
	const FGameplayTagContainer& AssetTags = Spec.GetDynamicAssetTags();

	const FGameplayTag DamageTypeTag = GetRawDamageSetByCallerTagForType(AssetTags);
	if (!DamageTypeTag.IsValid())
	{
		return; // misconfigured attacking ability - no Damage.* tag set, nothing to apply
	}

	const float RawDamage = Spec.GetSetByCallerMagnitude(DamageTypeTag, false, 0.f);
	if (RawDamage <= 0.f)
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	AActor* TargetActor = TargetASC ? TargetASC->GetAvatarActor() : nullptr;

	// Race matchup multiplier (design doc §5 / every race-design-*.md table).
	float RaceMultiplier = 1.f;
	if (const AGSEnemyCharacter* EnemyTarget = Cast<AGSEnemyCharacter>(TargetActor))
	{
		if (const UGSRaceDataAsset* RaceData = EnemyTarget->GetRaceData())
		{
			RaceMultiplier = RaceData->GetDamageMultiplier(DamageTypeTag);
		}
	}

	float DamageAfterRace = RawDamage * RaceMultiplier;

	// Armor mitigation - skipped for Blast (barrels) and anything explicitly flagged
	// Damage.IgnoresArmor (Shadow/Blood magic), per every race doc's "explosives/curses bypass
	// armor" callouts.
	const bool bSkipArmor = DamageTypeTag == GSTags::Damage_Blast || AssetTags.HasTagExact(GSTags::Damage_IgnoresArmor);

	float FinalDamage = DamageAfterRace;
	if (!bSkipArmor)
	{
		if (const UGSAttributeSetBase* TargetAttributes = TargetASC
				? TargetASC->GetSet<UGSAttributeSetBase>()
				: nullptr)
		{
			FinalDamage = FMath::Max(0.f, DamageAfterRace - TargetAttributes->GetArmor());
		}
	}

	if (FinalDamage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			UGSAttributeSetBase::GetIncomingDamageAttribute(), EGameplayModOp::Additive, FinalDamage));
	}
}
