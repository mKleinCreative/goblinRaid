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

	/** Blowing the war-horn (GDD §2.5). Owned by UGSGA_Horn's ActivationOwnedTags and used by that
	 *  same ability as a block, so a second blast cannot start while the first is still sounding. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Horn);

	/** A swing is in flight - windup, damage window or recovery. Owned by UGSGA_SwordLight's
	 *  ActivationOwnedTags, so GAS maintains it for exactly the ability's lifetime.
	 *
	 *  Too coarse to react to on its own: it stays true through recovery and across a whole combo
	 *  chain, so an AI gating its guard on this would hold the shield up long after the danger had
	 *  passed. Use the Windup child for that. This one answers "is he busy swinging at all", which is
	 *  what a would-be blocker needs in order NOT to raise a guard mid-swing. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Attacking);

	/** THE TELEGRAPH. Present for exactly FGSSwingStage::WindupSeconds - from the frame the swing is
	 *  committed to the frame the damage window opens.
	 *
	 *  This is the ONLY channel by which an AI may learn that a hit is coming. Nothing may read the
	 *  opponent's ability internals, montage position or BT state instead: the player sees the windup
	 *  pose on the same frame this tag appears, so a defender reacting to it is reacting to something
	 *  the player also saw, and the fight stays honest when the player joins the exchange.
	 *
	 *  A LOOSE tag rather than ActivationOwnedTags because it spans a sub-range of the ability, and a
	 *  loose tag is the only thing that can be added and removed part-way through one. Deliberately
	 *  not an anim notify: GSGA_SwordLight.h records that these timings are tuned constantly and that
	 *  structural montage edits crash the editor with its window open, and a notify pass would cost a
	 *  montage edit per weapon per skeleton. The per-stage WindupSeconds already IS this window. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Attacking_Windup);

	// ---- ACF action tags for AI (#353) --------------------------------------------------------
	//
	// ACF drives an AI's attacks by TAG: UACFCombatBehaviourComponent picks an FActionChances row
	// and calls TriggerAction(ActionTag), and UACFAbilitySet resolves that tag to an ability class
	// by stamping it as a dynamic spec source tag at grant time. The tag is the whole contract.
	//
	// #343 reused State.Attacking for this because it was the only registered tag GA_HU_SwordLight
	// already carried, and it worked - but it was a stopgap. State.* tags describe what a character
	// IS DOING; these describe what an AI may be TOLD TO DO, and conflating them meant the bow could
	// never be addressed at all (there was no second State.* tag to borrow). ACF's own sample uses an
	// Actions.* namespace for exactly this, but declares those tags only in the sample project, so
	// Actions.Attack and friends resolve to nothing here. These are ours.
	//
	// Native rather than ini so they exist without an editor restart cycle and cannot be silently
	// dropped by a config merge - the same reasoning as every other tag in this file.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Actions_Defender_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Actions_Defender_Ranged);

	/** Momentary: the last hit this character DEALT was turned aside by plate (#355). Held for one
	 *  frame by the damage calculation so the swing that caused it can read the verdict back after
	 *  ApplyGameplayEffectSpecToTarget returns - the calc runs INSIDE the swing's sweep loop and may
	 *  not call back into the ability, so a loose tag is the channel. Same shape as State.Recoil for
	 *  a blocked swing, minus the punish: plate refuses the blow, it does not open the attacker. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_LastHitDeflected);

	/** Your swing was turned aside by a guard, and you are open.
	 *
	 *  Applied to the ATTACKER when UGSDamageExecCalculation resolves a hit as blocked. Blocks both
	 *  the attack abilities and the block ability, so a recoiling fighter can neither swing again nor
	 *  hide behind their own guard - which is the whole point. Dodging is deliberately still allowed,
	 *  so a player who reads their own mistake can still get out of it.
	 *
	 *  This is what makes blocking worth doing rather than merely cheaper than being hit: it turns a
	 *  successful guard into an opening, and it does it symmetrically. It is also the anti-turtle
	 *  answer for allied goblins, who carry no guard break by design - they cannot kick a shield
	 *  down, but they can punish the swing that bounces off one. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Recoil);

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

	// ------------------------------------------------------------------ ACF teams (#229)
	//
	// ACF's team system keys on tags under "Teams", not on RaceTag. These exist so
	// UACFTeamManagerComponent has something to look up; ruling 28 keeps RaceTag for everything
	// that is NOT hostility (race data, animation sets, bark selection).
	//
	// The relationships live in DA_GSTeams and must be authored in BOTH directions - ACF's lookup
	// finds TeamA's entry and reads Relationship.Find(TeamB) with nothing mirroring it
	// (ACFTeamManagerSubsystem.cpp:183-193), so a missing reverse entry silently falls through to
	// the default and A hunts B while B ignores A. Same-team friendliness must also be authored:
	// `if (TeamA == TeamB) return GetDefaultAttitude();` and that default is Neutral.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Teams_Goblin);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Teams_Human);

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
	 *
	 * The anchor set follows the GDD 2.8 roster as revised 2026-08-14 (queue #156): the raid's
	 * three REQUIRED objectives are Market, Statue and Mill; the wheat Field is OPTIONAL,
	 * worth points but not gating extraction. Marker_ObjectiveAnchor_Granary was retired with
	 * that ruling - the granary had no mesh, no Blueprint and no placed instance, and nothing
	 * ever queried the tag.
	 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_GuardPost);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_PatrolNode);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_CivilianAnchor);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_CoverProp);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_HordeArrival);
	// Required trio (2.8).
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_ObjectiveAnchor_Market);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_ObjectiveAnchor_Statue);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_ObjectiveAnchor_Mill);
	// Optional (2.8).
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_ObjectiveAnchor_Field);

	// ---- ACF equipment slots -------------------------------------------------------------------
	// ACF addresses equipment slots by GameplayTag, where this project has always used the
	// EGSWeaponSlot enum. These exist so UACFEquipmentComponent has somewhere to put an item at
	// all: AvailableEquipmentSlot is empty on every character in the project, which is why nothing
	// can be equipped through ACF today.
	//
	// Declared in C++ rather than an ini because that is how all 45 tags here are declared - this
	// project has no DefaultGameplayTags.ini. ACF's own config ships only "ItemSlot.ItemSlot",
	// which is a category root and not a usable slot.
	//
	// Named for the SOCKETS they correspond to on GOB_Scout_v2_Skeleton, so the mapping stays
	// obvious: RightHand is hand_r_weapon, LeftHand is hand_l_weapon, Back is the back_* holsters.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(ItemSlot_RightHand);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(ItemSlot_LeftHand);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(ItemSlot_Back);

	// ---- Weapon wheel slots (#274) ----------------------------------------------------------
	// These replace the EGSWeaponSlot enum. They are a DIFFERENT AXIS from the ItemSlot.* tags
	// above and must not be confused with them: an ItemSlot is where an item physically hangs
	// (hand_r_weapon, back_sword), while a WeaponSlot is which loadout the player has chosen. A
	// torch and a sword both live in ItemSlot.RightHand; they are different WeaponSlots.
	//
	// The enum they replace carried an APPEND ONLY warning because its integer values were
	// serialised into character CDOs. Tags are stored by name, so that hazard is gone and the
	// wheel's contents are now data (see UGSWeaponComponent::WheelSlots).
	// PRIMARY, not "Sword" (#284). The slot holds whatever this character's main melee weapon is,
	// and for the horde that is an AXE - the old name described the player's kit rather than the
	// slot's job, and read as a bug every time a goblin "held a sword".
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponSlot_Torch);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponSlot_Bow);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponSlot_Primary);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponSlot_Grapple);
}
