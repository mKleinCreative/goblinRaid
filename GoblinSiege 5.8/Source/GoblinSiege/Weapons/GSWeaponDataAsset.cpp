#include "Weapons/GSWeaponDataAsset.h"

// Pure data - see header. In-editor, create one instance per weapon kit under Content/Data/Weapons/:
//   DA_Weapon_Scout (bHasRangedMode=true, bow ranged mode; "DA_Weapon_Daggers" was this asset's
//   name before design doc §13 decision 36 folded the dual daggers into a single sword),
//   DA_Weapon_Greatclub, DA_Weapon_ShadowStaff, DA_Weapon_BloodStaff - values from
//   race-design-goblins.md.
//
// 2026-08-01 - the visual fields added today (GoblinSiege|Weapon|Visual) need SIX SOCKETS on the
// goblin skeleton that do not exist yet, and their C++ defaults are the names to author against:
//
//   hand_r_weapon  - sword, while drawn
//   hand_l_weapon  - bow, while drawn
//   hand_l_torch   - readied torch (off hand, its own socket - see HeldTorchSocket's comment)
//   back_sword     - sword, while the bow is out
//   back_bow       - bow, while the sword is out
//   spine_quiver   - quiver, permanently
//
// Until those sockets exist UGSWeaponComponent falls back to the character's root component and
// says so once per socket in the log, so a wrongly-placed weapon is visibly wrong rather than
// silently absent. Nothing here needs to be authored for the game to run - every mesh field may
// stay unset and the goblin simply fights empty-handed.
