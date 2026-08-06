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

	/** Guard up. Read by UGSDamageExecCalculation, which mitigates frontal hits only - a block
	 *  that protects your back is not a block. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Blocking);

	/** Present only while a hit-react montage is playing. Exists so a flurry cannot restart the
	 *  flinch every frame, which reads as a seizure rather than a stagger. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_HitReact);

	/** Guard kicked open. Held for the stagger window, during which UGSGA_Block refuses to
	 *  activate - that refusal is the whole point: an interrupt that lets you immediately re-block
	 *  punishes nothing, so the opening has to persist for long enough to be exploited. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_GuardBroken);

	/** Hold-E channel in progress (GDD §8). Owned by UGSGA_Interact's ActivationOwnedTags, so GAS
	 *  adds and removes it for exactly the channel's lifetime - nothing else should set it. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Interacting);

	/** Hands full. Applied as a loose tag by UGSCarryComponent for exactly as long as the object is
	 *  held; attack abilities and the torch toss block on it. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Carrying);

	// ------------------------------------------------------------------ interaction verbs
	// The slice's channelled verbs plus the carry pick-up/put-down channel. These are DATA: an
	// interactable advertises one through UGSInteractableComponent::VerbTag, no C++ branches on
	// them, and a fifth verb costs a tag rather than a subclass.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Loot);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Takedown);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_FoulWell);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Carry);

	/** RESERVED, deliberately unused as of 2026-08-04: extraction is an auto-bank circle (GDD §9),
	 *  not a hold-E channel. Kept declared because §12.1 lists extract among the slice verbs and the
	 *  ruling was "for now" - if it becomes channelled, this is the tag and nothing else changes. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Extract);

	// ------------------------------------------------------------------ races / factions
	// Who counts as "us". Melee refuses to damage a target sharing the attacker's race, which is
	// what stops a patrol of militia from cutting each other down the first time they crowd a
	// doorway. Deliberately NOT applied to fire: the torch is the goblin equalizer (design doc §6)
	// and burns everyone, including the goblin holding it.
	//
	// An unset race hits everything, so anything that has not opted in behaves exactly as before.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Race_Goblin);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Race_Human);

	// ------------------------------------------------------------------ SetByCaller data keys
	/** Magnitude key for UGSGE_MoveSpeedScalar. Same convention as the Damage.* tags, which double
	 *  as SetByCaller keys on the damage spec: one effect class, many callers, no GE per source. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_MoveSpeedScalar);

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

	/**
	 * Houses (2026-08-05). The fourth burn type, and the first one added since the win condition
	 * was written - which is the point ObjectiveTypeTag's own comment predicted: "a fourth burn type
	 * (a granary, a tannery) should be able to answer by being placed and tagged in a level rather
	 * than by editing an enum." Nothing in UGSRaidDirector changes to accept it.
	 *
	 * Consequence worth stating plainly: once houses are placed AND tagged, the raid needs one of
	 * FOUR types burnt to win, not three. Leaving a building untagged is the safe half-step - it
	 * burns and scores nothing rather than making the tutorial unwinnable.
	 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Objective_Burn_House);

	// ------------------------------------------------------------------ raid markers
	/**
	 * The marker set (added 2026-08-05 with AGSRaidMarker) - the placement contract between
	 * whoever decides WHERE things go and whoever decides how they behave. See GSRaidMarker.h for
	 * the boundary this encodes; in short: the marker says where, and nothing else.
	 *
	 * Tags rather than an enum for the same reason the Objective.Burn.* set is tagged: a ninth
	 * marker type should cost a tag and a placed actor, not a recompile - which on this machine is
	 * a six-minute editor-closed build.
	 *
	 * Marker.ObjectiveAnchor.* is a hierarchy on purpose: AGSRaidMarker::GatherByType uses
	 * MatchesTag, so a query for the parent finds every objective anchor while a query for
	 * .Field finds only the fields.
	 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_GuardPost);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_PatrolNode);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_CivilianAnchor);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_CoverProp);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_HordeArrival);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_ObjectiveAnchor_Field);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_ObjectiveAnchor_Mill);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_ObjectiveAnchor_Market);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_ObjectiveAnchor_Granary);
}
