#include "Combat/GSGE_MoveSpeedScalar.h"

#include "Attributes/GSAttributeSetBase.h"
#include "Combat/GSGameplayTags.h"

UGSGE_MoveSpeedScalar::UGSGE_MoveSpeedScalar()
{
	// Infinite and removed by handle: the lifetime is "while the sack is in your hands" or "while the
	// guard is up" - object state, not a duration anyone can author.
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FSetByCallerFloat Scalar;
	Scalar.DataTag = GSTags::Data_MoveSpeedScalar;

	FGameplayModifierInfo Mod;
	Mod.Attribute = UGSAttributeSetBase::GetMoveSpeedMultiplierAttribute();

	// MultiplyCompound, NOT MultiplyAdditive. UE aggregates same-op mods before applying them:
	// ((Base + AddBase) * MultiplyAdditive / DivideAdditive * MultiplyCompound) + AddFinal. Two 0.55
	// slows under MultiplyAdditive would ADD to 1.10 and make a doubly-slowed goblin faster than an
	// unencumbered one. Compounded they behave the way a player expects: 0.55 * 0.55 = 0.30.
	Mod.ModifierOp = EGameplayModOp::MultiplyCompound;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(Scalar);

	Modifiers.Add(Mod);
}
