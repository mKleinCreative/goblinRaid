#include "Combat/GSDamageExecCalculation.h"
#include "Combat/GSGameplayTags.h"
#include "Combat/GSRaceDataAsset.h"
#include "Attributes/GSAttributeSetBase.h"
#include "Characters/GSEnemyCharacter.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

// 2026-08-02: combat damage was completely silent, and this function has TWO paths that return
// without applying anything. On screen, "the trace missed", "the target has no ASC", "no Damage.*
// tag on the spec" and "armor ate it" are four different bugs that look identical - each costs an
// hour to tell apart by guessing. One cvar makes them one glance.
//   GS.Combat.LogDamage 1   -> log line per resolution
//   GS.Combat.LogDamage 2   -> also print on screen
static int32 GSCombatLogDamage = 0;
static FAutoConsoleVariableRef CVarGSCombatLogDamage(
	TEXT("GS.Combat.LogDamage"),
	GSCombatLogDamage,
	TEXT("0 = off, 1 = log every damage resolution incl. rejections, 2 = also print on screen."),
	ECVF_Cheat);

static void GSLogDamage(const FString& Msg, bool bIsRejection)
{
	if (GSCombatLogDamage <= 0)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[GS.Damage] %s"), *Msg);

	if (GSCombatLogDamage >= 2 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f,
			bIsRejection ? FColor::Orange : FColor::Yellow, FString::Printf(TEXT("[dmg] %s"), *Msg));
	}
}

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
		// Misconfigured attacking ability. By far the most likely cause is a Blueprint GE child
		// carrying Damage.* in its ASSET tags instead of the ability calling AddAssetTag on the
		// spec handle - the two look identical in the editor and only this path can tell you apart.
		GSLogDamage(FString::Printf(TEXT("REJECTED: no Damage.* tag on spec (dynamic tags: %s)"),
			*AssetTags.ToStringSimple()), true);
		return;
	}

	const float RawDamage = Spec.GetSetByCallerMagnitude(DamageTypeTag, false, 0.f);
	if (RawDamage <= 0.f)
	{
		GSLogDamage(FString::Printf(TEXT("REJECTED: %s tagged but SetByCaller magnitude is %.2f"),
			*DamageTypeTag.GetTagName().ToString(), RawDamage), true);
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

	if (GSCombatLogDamage > 0)
	{
		const UGSAttributeSetBase* Attrs = TargetASC ? TargetASC->GetSet<UGSAttributeSetBase>() : nullptr;
		GSLogDamage(FString::Printf(
			TEXT("%s -> %s  %s  raw %.1f  x%.2f race  %s armor %.1f  = %.1f   (HP %.0f/%.0f)"),
			*GetNameSafe(Spec.GetContext().GetInstigator()),
			*GetNameSafe(TargetActor),
			*DamageTypeTag.GetTagName().ToString(),
			RawDamage, RaceMultiplier,
			bSkipArmor ? TEXT("SKIP") : TEXT("-"),
			Attrs ? Attrs->GetArmor() : 0.f,
			FinalDamage,
			Attrs ? Attrs->GetHealth() : 0.f,
			Attrs ? Attrs->GetMaxHealth() : 0.f),
			FinalDamage <= 0.f);
	}

	if (FinalDamage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			UGSAttributeSetBase::GetIncomingDamageAttribute(), EGameplayModOp::Additive, FinalDamage));
	}
}
