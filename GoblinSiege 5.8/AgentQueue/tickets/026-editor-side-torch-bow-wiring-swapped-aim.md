---
id: 026
title: Editor-side torch/bow wiring: swapped aim materials, missing bow+quiver meshes, unset bow ability
agent: claude-ranged
status: done
claimed: 2026-08-06T06:19Z
build: none
waiting_on:
evaluated: 2026-08-06T06:20Z
files: 
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
  - Content/Data/Weapons/DA_Weapon_Scout.uasset
---

## Goal

Editor-side torch/bow wiring: swapped aim materials, missing bow+quiver meshes, unset bow ability

## Generate

First ticket with MCP access. Four editor-side asset fixes, all data - no C++, no build.

**The actual cause of "no reticule": the two aim materials were in each other's slots.**

    arc_material           = M_GS_AimLanding   (MD_DEFERRED_DECAL)  on spline meshes
    landing_decal_material = M_GS_AimArc       (MD_SURFACE)         on the decal

A deferred-decal material cannot render on a static mesh and a surface material cannot render
as a decal, so the ribbon drew as broken geometry and the reticle drew as nothing at all.
Swapped to match their domains; verified by re-reading `material_domain` off both.

**`DA_Weapon_Scout` had two empty mesh slots.** `ranged_mesh` and `quiver_mesh` were None, so
swapping to ranged mode equipped an invisible bow. Set to `GS_Bow_Only` and `GS_Quiver` -
`GS_Bow_Only` rather than `GS_Bow` because the quiver is a separate slot that is worn
continuously and must not be duplicated inside the bow mesh.

**`BowShotAbilityClass` was None** on `BP_GSPlayerCharacter`, which is what
`IsRangedAttackMode()` null-checks - so the attack button was falling through to the melee
path. Set to `GSGA_BowShot`, Blueprint recompiled.

Saved with `save_packages(..., only_dirty=False)` and verified by file mtime, then re-read
every value back off the assets.

## Evaluate

**Verified by evidence** (re-read after save, not by tool return value):
`ranged_mesh=GS_Bow_Only`, `quiver_mesh=GS_Quiver`, `held_torch_mesh=GS_Torch`,
`has_ranged_mode=True`, `bow_shot_ability_class=GSGA_BowShot`,
`arc_material=M_GS_AimArc (MD_SURFACE)`, `landing_decal_material=M_GS_AimLanding
(MD_DEFERRED_DECAL)`.

**Two of my earlier diagnoses were wrong, and this ticket is where that gets recorded.**
- #023 attributed "no reticule" to the decal's 40uu projection depth and to the ring being
  buried in the 400uu box. Neither was the cause. The cause was a swapped assignment I could
  not see without editor access, and I guessed twice rather than saying "I cannot know this".
- I repeatedly cited `GSWeaponComponent.h`'s comment that the goblin skeleton has NO weapon
  sockets, and built the "the torch parks at his feet" theory on it. **That comment is stale.**
  `GOB_Scout_v2` has `hand_l_torch` (L_Hand), `hand_r_weapon` (R_Hand), `hand_l_weapon`
  (L_Hand) and `spine_quiver` (Spine02), all with authored offsets.

**So why is the torch "never in your hand"?** Not a missing socket and not a missing mesh -
both are fine. `TorchWindupSeconds` is **0.25s**, and `EndAbility` un-readies the prop the
instant the projectile spawns. It appears for a quarter of a second, with no animation to
sell it. That is the real answer and it is a design problem, not a wiring one.

**Not tested.** I changed assets; I have not run PIE. Nothing here is observed working.

**Still broken until a build** (gated on #022/#023/#024): the ribbon is `1M_Cube` at
`arc_segment_width` 4.0, and the shipped code multiplies rather than converting - so segments
are still 400uu wide. The arc will now be the right COLOUR and the reticle should appear, but
the ribbon stays fat until #023 compiles.

**Owes AGENT_STATE.md**
- FAILED: `GSWeaponComponent.h`'s "the goblin skeleton has NO weapon sockets" comment is out
  of date and actively misled two diagnoses. Someone should delete it.
- DECISION/FAILED: paired assets whose slots are interchangeable by type (both
  `UMaterialInterface`) can be silently cross-assigned. `UGSAimComponent` could validate
  `material_domain` on resolve and warn - cheaper than the round trip this cost.

## Refine

- Verified by re-reading every property off the saved asset rather than trusting the setter,
  per the project rule that a successful call is not evidence.
- Chose `GS_Bow_Only` over `GS_Bow` deliberately; see Generate.
- **Did NOT set `arc_segment_width` to 0.04** to make the ribbon thin before the build. It
  would look right today and become 0.04uu - invisible - the moment #023 compiles, because
  that change makes the property mean world units. Trading a visible wrong for an invisible
  wrong is a bad trade, and it would land as a fresh bug report after a build nobody
  connected to it. The fat ribbon stays until the build fixes it properly.

**Deliberately left undone.** The throw animation (`AM_GS_ThrowTorch` is authored and
referenced by nothing) and the torch-as-weapon-slot wheel. Both are real work, not wiring,
and both belong to the radial-menu ticket rather than here.
