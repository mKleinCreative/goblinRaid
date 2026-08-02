// Native gameplay tags for the damage pipeline (design doc §5, race matchup matrix columns).
// One Damage.* tag per weapon-kit damage type; the tag doubles as the SetByCaller key on the
// attacking GameplayEffectSpec, so GSDamageExecCalculation needs no separate lookup table.
// Reconstructed 2026-07-19 to match GSDamageExecCalculation.cpp's GSTags:: usage exactly.
// Extended 2026-07-24 (player character controller pass): State.Crouching/Dodging/Aiming for the
// universal kit's stealth stance, dodge roll, and aim-facing rotation mode (tech doc §16, design
// doc §7).
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

	/** Stealth stance active - crouch toggle, universal kit (design doc §7). Queried by the future
	 *  Stealth & Interaction Agent's detection-radius math; set/cleared by AGSPlayerCharacter's
	 *  OnStartCrouch/OnEndCrouch. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Crouching);

	/** Dodge roll in flight - i-frames + committed movement lock (design doc §7: 0.22s i-frames,
	 *  committed recovery). Owned by UGSGA_DodgeRoll's ActivationOwnedTags; GAS adds/removes it
	 *  automatically for the ability's lifetime. AGSPlayerCharacter::Input_Move checks it to block
	 *  movement input during the roll; future attack abilities should add it to their own
	 *  ActivationBlockedTags. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dodging);

	/** Facing the camera/aim direction instead of the movement direction (tech doc §16 - "the
	 *  character faces movement, and faces the aim while attacking or aiming"). Set/cleared by
	 *  AGSPlayerCharacter::UpdateRotationMode so animation Blueprints and future combat systems can
	 *  query it without reaching into player-only state. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Aiming);

	// ------------------------------------------------------------------ burn objective types
	/**
	 * The three burn-objective TYPE tags the burn-types spec §5 has owed since it was written,
	 * added 2026-07-31 for Q-37 (the state half of Q-32's Required -> Optional -> Complete ruling).
	 *
	 * Every placed burn carrier declares one of these through
	 * AGSBurnObjectiveBase::ObjectiveTypeTag, and it is the LOOKUP KEY for the win condition:
	 * ruled 2026-07-31, the raid is won by burning one of each TYPE, so the moment the first
	 * carrier of a type completes, the remaining carriers of that same type demote to Optional.
	 * "Same type" is exactly "same tag" - which is why this is a tag and not the
	 * EGSBurnObjectiveType enum it parallels. The enum is a C++ switch value that needs a recompile
	 * to extend; a fourth burn type (a granary, a tannery) should join the win condition by being
	 * placed and tagged in a level.
	 *
	 * Objective.Burn.* rather than the existing Damage.* / State.* roots because these describe a
	 * MISSION-LAYER object, not a hit or a character state, and grouping them under a shared
	 * Objective.Burn parent is what lets a future query match "any burn objective" with one
	 * MatchesTag call.
	 *
	 * The market is never demoted: only one exists, so it has no siblings to be demoted by. That
	 * falls out of the rule and needs no special case in the tag set.
	 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Objective_Burn_Mill);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Objective_Burn_Field);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Objective_Burn_Market);
}
