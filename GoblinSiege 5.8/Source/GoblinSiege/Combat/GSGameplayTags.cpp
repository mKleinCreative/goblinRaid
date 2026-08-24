#include "Combat/GSGameplayTags.h"

namespace GSTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Dagger, "Damage.Dagger", "Slasher dual daggers (melee mode)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Bow, "Damage.Bow", "Slasher hunting bow (ranged mode)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Greatclub, "Damage.Greatclub", "Brute great-club, incl. Ground Slam concussive AoE");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_ShadowMagic, "Damage.ShadowMagic", "Shaman Shadow path - ignores physical armor");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_BloodMagic, "Damage.BloodMagic", "Shaman Blood path - Blood Nova etc.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Fire, "Damage.Fire", "Torches and fire volumes - the goblin equalizer");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Blast, "Damage.Blast", "Explosive barrels - always bypasses armor");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_IgnoresArmor, "Damage.IgnoresArmor", "Modifier: skip flat armor mitigation");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Character is dead - blocks ability activation");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invulnerable, "State.Invulnerable", "Respawn i-frames - damage GEs check this");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Crouching, "State.Crouching", "Stealth stance active - crouch toggle (design doc §7)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dodging, "State.Dodging", "Dodge roll in flight - i-frames + committed movement lock (design doc §7)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Aiming, "State.Aiming", "Facing camera/aim direction instead of movement direction (tech doc §16)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Blocking, "State.Blocking", "Guard up - frontal hits are mitigated in GSDamageExecCalculation and stagger the blocker instead of wounding them");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_HitReact, "State.HitReact", "Flinching. Present only for the length of a hit-react montage; blocks a second flinch from stacking");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_GuardBroken, "State.GuardBroken", "Guard has been kicked open. Blocks re-raising the guard for the stagger window, which is what makes turtling punishable rather than merely interrupted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Interacting, "State.Interacting", "Hold-E channel in progress.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Carrying, "State.Carrying", "Carrying an object - slower, cannot attack, cannot throw a torch.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Horn, "State.Horn", "Blowing the war-horn - blocks a second blast until it finishes.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Attacking, "State.Attacking", "A swing is in flight - windup, damage window or recovery. Too coarse to react to; see the Windup child.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Attacking_Windup, "State.Attacking.Windup", "THE TELEGRAPH. Present for exactly the swing's windup. The only channel by which an AI may learn a hit is coming.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Recoil, "State.Recoil", "Your swing was turned aside by a guard. Cannot attack or block for the window - this is what makes a block an opening rather than just cheaper damage.");

	// Interaction verbs (GDD §8). Data, not subclasses - see GSGameplayTags.h.
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interact_Loot, "Interact.Loot", "Loot a container or a corpse.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interact_Takedown, "Interact.Takedown", "Stealth takedown on an unaware defender.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interact_FoulWell, "Interact.FoulWell", "Foul a village well.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interact_Carry, "Interact.Carry", "Pick up / put down a carryable object.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interact_Extract, "Interact.Extract", "RESERVED - extraction auto-banks on a circle for now, not a channel.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Race_Goblin, "Race.Goblin", "Player, allied goblins and the horde - melee will not hit its own race.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Race_Human, "Race.Human", "Village defenders - militia, archers, knights, civilians.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Teams_Goblin, "Teams.Goblin", "ACF combat team for the player, the horde and allied goblins (#229).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Teams_Human, "Teams.Human", "ACF combat team for militia, archers, knights and every other defender (#229).");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_MoveSpeedScalar, "Data.MoveSpeedScalar", "SetByCaller key: multiplier fed to UGSGE_MoveSpeedScalar.");

	// Burn-objective types (burn-types spec §5, added 2026-07-31 for Q-37). The win needs one burn
	// of each of these; see GSGameplayTags.h and AGSBurnObjectiveBase::ObjectiveTypeTag.
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Objective_Burn_Mill, "Objective.Burn.Mill", "Windmill burn objective - dust-fuse detonation (design doc §6.3)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Objective_Burn_Field, "Objective.Burn.Field", "Wheat field burn objective - grid spread, >=70% of cells (Q-03)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Objective_Burn_Market, "Objective.Burn.Market", "Market burn objective - stall-to-stall cluster spread; never Optional, only one exists");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Objective_Burn_House, "Objective.Burn.House", "A house - lit through a broken window or across the roof, never from outside (2026-08-05)");

	// Raid markers (added 2026-08-05 with AGSRaidMarker). Placement data only - a marker never
	// carries counts or behaviour. See GSRaidMarker.h.
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Marker_GuardPost, "Marker.GuardPost", "Where a defender stands watch. Facing is load-bearing - the arrow is the post's look direction.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Marker_PatrolNode, "Marker.PatrolNode", "One node of a patrol loop. GroupId names the loop, OrderIndex is walking order.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Marker_CivilianAnchor, "Marker.CivilianAnchor", "A civilian's daily-routine anchor - the well, a stall, a pen.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Marker_CoverProp, "Marker.CoverProp", "A prop placed to break a sightline, per the cover-guarantee rule (GDD 2.4, restated 3.1).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Marker_HordeArrival, "Marker.HordeArrival", "Treeline edge the horde walks in from when the horn is blown.");
	// Required trio (2.8, revised 2026-08-14). Granary retired with that ruling.
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Marker_ObjectiveAnchor_Market, "Marker.ObjectiveAnchor.Market", "Where the market square belongs - the village heart.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Marker_ObjectiveAnchor_Statue, "Marker.ObjectiveAnchor.Statue", "Where the king's statue belongs - the village square, where the guards are thickest. Toppled, never burned.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Marker_ObjectiveAnchor_Mill, "Marker.ObjectiveAnchor.Mill", "Where a windmill belongs - wants a flat, visible rise.");
	// Optional (2.8).
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Marker_ObjectiveAnchor_Field, "Marker.ObjectiveAnchor.Field", "Where a wheat field belongs.");

	// ---- ACF equipment slots ----
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(ItemSlot_RightHand, "ItemSlot.RightHand", "ACF equipment slot for the weapon hand - the socket hand_r_weapon, where the axe and the sword go.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(ItemSlot_LeftHand, "ItemSlot.LeftHand", "ACF equipment slot for the off hand - the socket hand_l_weapon, where a drawn bow goes.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(ItemSlot_Back, "ItemSlot.Back", "ACF equipment slot for a sheathed weapon - the back_sword and back_bow holsters.");
}
