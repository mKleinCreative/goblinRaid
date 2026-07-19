// Native gameplay tags for the damage pipeline (design doc §5, race matchup matrix columns).
// One Damage.* tag per weapon-kit damage type; the tag doubles as the SetByCaller key on the
// attacking GameplayEffectSpec, so GSDamageExecCalculation needs no separate lookup table.
// Reconstructed 2026-07-19 to match GSDamageExecCalculation.cpp's GSTags:: usage exactly.
#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace GSTags
{
	// Damage types - one per weapon kit / hazard, matching the race-design-*.md matrix rows.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Dagger);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Bow);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Greatclub);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_ShadowMagic);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_BloodMagic);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Blast);

	/** Modifier tag: added alongside a Damage.* tag when the hit should skip flat armor
	 *  (Shadow/Blood magic per race-design-dwarves.md "armor beats the physical, not the arcane";
	 *  Blast skips armor unconditionally without needing this tag). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_IgnoresArmor);

	// Character state tags consumed by abilities/effects.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);
}
