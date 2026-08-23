// Copyright Goblin Siege.

#include "Combat/GSACFDamageCalculation.h"
#include "Characters/GSCharacterBase.h"
#include "Combat/GSGameplayTags.h"
#include "Attributes/GSAttributeSetBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Game/ACFDamageType.h"
#include "Game/ACFFunctionLibrary.h"
#include "Combat/GSDamageTypes.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

// The same cvars UGSDamageExecCalculation exposes, deliberately NOT duplicated as new names: while
// both paths exist they must be tunable together, or a session spent tuning one silently leaves the
// other alone. Read through IConsoleManager rather than redeclared.
static float GSGetCVarFloat(const TCHAR* Name, float Fallback)
{
	if (const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
	{
		return Var->GetFloat();
	}
	return Fallback;
}

bool UGSACFDamageCalculation::IsStraightOn(const AActor* Target, const AActor* Attacker) const
{
	if (!Target || !Attacker)
	{
		// No attacker means fire, a falling beam, a fall. Plate helps against none of that, and
		// treating it as a frontal hit would hand a knight a 0.3 scalar against a burning field.
		return false;
	}

	FVector ToAttacker = Attacker->GetActorLocation() - Target->GetActorLocation();
	ToAttacker.Z = 0.f;
	if (ToAttacker.IsNearlyZero())
	{
		return false;
	}

	const float ArcDegrees = GSGetCVarFloat(TEXT("GS.Combat.PlateArc"), 150.f);
	const float Facing = FVector::DotProduct(ToAttacker.GetSafeNormal(), Target->GetActorForwardVector());
	return Facing >= FMath::Cos(FMath::DegreesToRadians(ArcDegrees * 0.5f));
}

float UGSACFDamageCalculation::CalculateFinalDamage_Implementation(const FACFDamageEvent& InDamageEvent,
	TArray<FAttributeData>& OtherAffectedAttributes)
{
	const float RawDamage = InDamageEvent.FinalDamage;
	if (RawDamage <= 0.f)
	{
		return 0.f;
	}

	const AGSCharacterBase* Target = Cast<AGSCharacterBase>(InDamageEvent.DamageReceiver);
	const AGSCharacterBase* Attacker = Cast<AGSCharacterBase>(InDamageEvent.DamageDealer);

	// ---- 1. race matchup ------------------------------------------------------------------------
	// Melee refuses to damage the attacker's own race, which is what stops a patrol cutting itself
	// down in a doorway. NOT applied to fire - see the class comment.
	//
	// TODO(2b-2b): this asks IsHostileTo, which since #229 consults ACF teams rather than RaceTag.
	// That is the correct question and a slightly different one: a civilian will be neither hostile
	// nor the same race. Worth watching the first time civilians exist.
	if (Target && Attacker && Attacker != Target && !Attacker->IsHostileTo(Target))
	{
		return 0.f;
	}

	float Damage = RawDamage;

	// ---- 2. NPC vs NPC --------------------------------------------------------------------------
	const bool bPlayerInvolved =
		(Target && Target->IsPlayerControlled()) || (Attacker && Attacker->IsPlayerControlled());
	if (!bPlayerInvolved && Attacker && Target)
	{
		Damage *= GSGetCVarFloat(TEXT("GS.Combat.NPCvNPCScalar"), 0.55f);
	}

	// ---- 3. directional plate -------------------------------------------------------------------
	// Armour only matters to a straight-on hit. From the flank, the back, or a takedown, the flat
	// value is skipped entirely rather than reduced - the gaps in the harness are gaps, not thinner
	// plate.
	// The bow finds the gaps. Read off the damage CLASS's tag container rather than a class check, so
	// a new ranged weapon only has to carry Damage.Bow to inherit the behaviour (#235).
	bool bFindsGaps = false;
	if (InDamageEvent.DamageClass)
	{
		if (const UACFDamageType* TypeCDO = InDamageEvent.DamageClass.GetDefaultObject())
		{
			bFindsGaps = TypeCDO->DamageTags.HasTag(GSTags::Damage_Bow)
				|| TypeCDO->DamageTags.HasTag(GSTags::Damage_IgnoresArmor)
				|| TypeCDO->DamageTags.HasTag(GSTags::Damage_Blast);
		}
	}

	if (Target && !bFindsGaps)
	{
		// Armour still lives on UGSAttributeSetBase - #228 moved HEALTH to ARS, not Armor. Ruling 25
		// maps it to UACFAttributeSet::PhysicalDefense eventually, and this is the line that changes
		// when it does.
		float Armor = 0.f;
		if (const UAbilitySystemComponent* ASC =
				UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target))
		{
			bool bFound = false;
			Armor = ASC->GetGameplayAttributeValue(UGSAttributeSetBase::GetArmorAttribute(), bFound);
			if (!bFound) { Armor = 0.f; }
		}

		if (Armor > 0.f && IsStraightOn(Target, Attacker))
		{
			const float Scalar = GSGetCVarFloat(TEXT("GS.Combat.PlateFrontalScalar"), 0.3f);
			Damage = FMath::Max(0.f, Damage * Scalar - Armor);
		}
	}

	// ---- 4. the floor ---------------------------------------------------------------------------
	// LAST, so it catches every mitigation above rather than needing a guard in each. Gated on the
	// raw damage having been positive: a hit that was cancelled outright (wrong race) stays zero.
	const float Floor = GSGetCVarFloat(TEXT("GS.Combat.MinimumDamage"), 1.f);
	if (Floor > 0.f)
	{
		Damage = FMath::Max(Damage, Floor);
	}

	// ---- THE RETURN VALUE IS NOT WHAT GETS APPLIED ----------------------------------------------
	//
	// This cost the first ACF damage build. UACFDamageHandlerComponent does:
	//
	//     tempDamageEvent.FinalDamage = DamageCalculator->CalculateFinalDamage(tempDamageEvent,
	//                                       tempDamageEvent.AttributesData);   // :196
	//     ...
	//     for (const auto& damageData : LastDamageReceived.AttributesData) {   // :113 - APPLIED HERE
	//
	// The float is recorded on the event; the DAMAGE ACTUALLY DEALT comes entirely from the
	// out-array. Returning the right number and leaving the array empty means ACF computes a perfect
	// hit and then applies nothing - which is exactly what Michael saw: "I turned
	// GS.Combat.ACFDamage 0 and damage started to work again."
	//
	// The value is a COST, not a delta: ConsumeStatistics treats the array as costs to subtract
	// (see UACFGASStatisticsComponent::ModifyStatistic, which negates before calling it), so a
	// POSITIVE number here removes health.
	OtherAffectedAttributes.Add(FAttributeData(UACFFunctionLibrary::GetHealthTag(), Damage));

	return Damage;
}
