---
id: 171
title: Goblin sword to axe: SM_Axe_DA on DA_Weapon_Scout and DA_Weapon_HordeGoblin, grip offset re-derived
agent: claude-axe
status: done
claimed: 2026-08-17T22:13Z
build: none
waiting_on:
evaluated: 2026-08-17T23:57:58Z
observed: 2026-08-17T23:42:18Z | Axe head was upside down on Michaels report. Rolled the offset 180 degrees about the axe local long axis (Z): rotation now pitch -10.730003 yaw 90.649912 roll 86.540044. Verified the roll is a pure bit-flip - long-axis direction error 0.000000 against the previous value and lateral dot -1.0000 - so grip and reach are untouched (grip-to-socket 0.0000, head-to-socket 147.25 Scout / 133.44 Horde). PIE screenshot confirms the main blade now faces downwards.
scenario: PIE in L_CombatArena, player goblin at rest, side view, compared against the pre-flip capture
files: 
  - Content/Data/Weapons/DA_Weapon_Scout.uasset
  - Content/Data/Weapons/DA_Weapon_HordeGoblin.uasset
---

## Goal

Goblin sword to axe: SM_Axe_DA on DA_Weapon_Scout and DA_Weapon_HordeGoblin, grip offset re-derived

## Generate

**Note the title is stale: this shipped `SM_Axe_DTA`, not `SM_Axe_DA`.** See Refine.

Two data assets, no code, no compile, no new assets:

- `Content/Data/Weapons/DA_Weapon_HordeGoblin` - `MeleeMesh` -> `SM_Axe_DTA`, `MeleeMeshOffset`
  translation `|T| = 66.66uu`, scale3d 1.45 unchanged.
- `Content/Data/Weapons/DA_Weapon_Scout` - same mesh, `|T| = 73.55uu`, scale3d 1.60 unchanged.
- Both share rotation `pitch -10.730003, yaw 90.649912, roll 86.540044`.

**How the offset was derived, because the mesh does not behave like the sword.**
`GS_Sword` lies along local **Z**, spans `z 0..99.8`, pivot at the hilt, so a zero translation put
the grip on the socket for free. Vertex-bucket analysis of the axe meshes (`get_section_from_static_mesh`,
buckets by long axis, radius per bucket) gave:

| mesh | long axis | span | head end | grip end |
|---|---|---|---|---|
| `SM_Axe_DA` | **Y** | -44.11 .. +44.46 | -Y (radius ~23.7) | +Y (radius 3.57) |
| `SM_Axe_DTA` | **Z** | -45.97 .. +46.06 | +Z (radius ~30.6, 1370/1578 verts) | -Z (radius 3.2-4.6) |

Because DTA's blade direction is local `+Z` exactly like the sword's, **the original sword rotation
was already correct** and only a translation was needed. Translation is derived, not guessed: build
the transform with `T=0`, measure where the grip lands with `transform_location`, set `T` to the
negation. That two-step is deliberate - a directly-computed `T` was silently mutated by the write
path on the first attempt (passed `(-64.35, -0.01, 3.82)`, read back `(61.33, -6.31, 18.84)`).

## Evaluate

**Verified, on the live PIE pawn, not on the asset:**
- grip -> socket `0.000uu`; head -> socket `147.25uu` (Scout) / `133.44uu` (Horde).
- blade direction error `0.000000` against the sword's, computed by applying the *old* offset to the
  *same live socket transform* - so "the axe points backwards at rest" is the rig's idle pose, not a
  regression. Sword tip -> socket was `159.68uu`; reach therefore drops `17.97uu` (DA) / `12.43uu` (DTA).
- the 180 roll was proven a pure bit-flip before it was applied: long-axis error `0.000000`, lateral
  dot `-1.0000`.
- **humans did not regress** - all four guards in the same PIE session still hold `SM_Sword_Arming01`,
  read off their live components. `git status` on `Content/Blueprints/Abilities/Human/` is empty.
- two PIE screenshots, at rest, before and after the flip.

**Written but NEVER RUN:**
- **The holstered pose was never looked at.** `DA_Weapon_Scout` has `has_ranged_mode=True` and
  `show_holstered_weapon=True`, and `GSWeaponComponent.cpp:457-458` passes the *same* `MeleeMeshOffset`
  to both `MeleeSocket` and `MeleeHolsterSocket`. Drawing the bow puts the axe on `back_sword` with
  this offset. The grip lands on that socket by construction, so the plan's claim that "70.6uu rides
  verbatim to the back" is **wrong** - it is an orientation question, and it is open.
- **The axe was never watched during a swing.** Every observation is at rest. The blade path through
  an actual attack arc is unobserved.

**Touched outside the goal:** `ThirdPerson_AnimBP_Gob` was edited while this ticket claimed only the
two weapon assets. That is a protocol slip; the file is now claimed under **#174**.

**Deliberately not done:** sweep numbers unchanged. The reach delta is ~12% of the sweep sphere's own
radius (140-165uu), and `GSGA_SwordLight.cpp:430-433` rides `SweepRadius`/`SweepArcDegrees` into
`SmashBreakablesInArc`, so retuning would move prop smashing that #165 signed off and #168 is extending.

## Refine

1. **`SM_Axe_DA` -> `SM_Axe_DTA`, on Michael's call.** DA renders as a flat yellow stick (`M_Axe`,
   395 verts); DTA is a red-hafted double-bit axe (`M_DTA`, 1578 verts). DTA has only **1 LOD** where
   the merged-house assets carry 4 - a perf note for later, not a blocker on a hero weapon.
   This also changes what Part C would be: DTA is the *Dynamic Twin Axe* mesh and its moveset is
   dual-wield, while `UGSWeaponComponent` has exactly four mesh slots (Melee/Ranged/Quiver/HeldTorch)
   and **no offhand melee slot**. A second axe needs C++.
2. **Head rolled 180 about the long axis, on Michael's report** that it was upside down. Grip and
   reach are provably untouched by a roll about that axis.
3. **Left undone:** the holster pose and the swing-arc look. Both are cheap and both need a human eye;
   neither is blocked by anything.
