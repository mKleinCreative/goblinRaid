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

	// Burn-objective types (burn-types spec §5, added 2026-07-31 for Q-37). The win needs one burn
	// of each of these; see GSGameplayTags.h and AGSBurnObjectiveBase::ObjectiveTypeTag.
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Objective_Burn_Mill, "Objective.Burn.Mill", "Windmill burn objective - dust-fuse detonation (design doc §6.3)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Objective_Burn_Field, "Objective.Burn.Field", "Wheat field burn objective - grid spread, >=70% of cells (Q-03)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Objective_Burn_Market, "Objective.Burn.Market", "Market burn objective - stall-to-stall cluster spread; never Optional, only one exists");
}
