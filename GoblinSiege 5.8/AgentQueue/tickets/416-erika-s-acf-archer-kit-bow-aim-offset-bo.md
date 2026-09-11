---
id: 416
title: Erika's ACF archer kit: bow aim offset, bow overlay, animated bow prop, combat montages
agent: claude-archer-kit
status: queued
claimed: 2026-09-11T03:20Z
build: none
waiting_on:
evaluated:
observed:
scenario:
files: 
  - Content/Blueprints/Adversaries/BP_ErikaArcher.uasset
  - Content/Items/BP_Item_ErikaBow.uasset
  - Content/Characters/Humans/ErikaArcher
  - Content/Data/Weapons/DA_Weapon_Erika.uasset
---

## Goal

Erika is on ACF's animation stack but has no combat animation: her `AM_HU_*` montages and
`Anims_Bow` clips are stranded on the old Mixamo skeleton, and the bow has never been held
correctly. Michael: *"use ACF's animations, they're better."*

## Generate

**17 bow assets retargeted onto `ACF_UE5Manny`**, using ACF's OWN shipped retargeters - nothing
hand-authored:
- `RTG_UE4Manny_UE5Manny` -> the 9-direction bow aim set (`ACF_BowAim`, the `BowAim_AO`
  AimOffsetBlendSpace, and 9x `BowAim_LookAt_{C,U,D,L,R,LU,LD,RU,RD}`), which was on
  `UE4_Mannequin_Skeleton`.
- `RTG_UEFN_to_UE5_Mannequin` -> the 6 bow poses, which were on `SK_UEFN_Mannequin`.
All land in `Content/Characters/Humans/ErikaArcher/Bow/`. This closes the "there is no aim offset"
gap that has been sitting in AGENT_STATE.

**`ABP_GS_Archer`** - a data-only Blueprint child of `ACF_Humanoid_ABP` (the horse/wyvern pattern),
inheriting ACF's whole layer map and adding the one entry it lacks: `Moveset.Bow` in
`moveset_layers`. Erika's anim class points at it.

**Two item tags on `BP_Item_ErikaBow`:** `Moveset` -> `Moveset.Bow`, and `MovesetOverlay` ->
`Moveset.Bow`.

**`.gitignore`:** the MEASURED dependency closure of `ACF_Humanoid_ABP` is now tracked in place -
292 packages, 126.4 MB, 2.7% of FullSample - plus the 1.6 MB bow prop kit. Negated in place rather
than copied into `Content/Characters/`, because duplicating would mean repointing every reference
and would permanently FORK those assets from ACF.

## Evaluate

**The bow was never going to work from the `Moveset` tag alone, and this is the finding worth
keeping.** `ACFWeapon` has THREE separate tag fields - `Moveset` (:68), `MovesetActions` (:76) and
`MovesetOverlay` (:84) - and the overlay is chosen from `MovesetOverlay`, never from `Moveset`
(`ACFEquipmentComponent.cpp:231-241` -> `ACFCharacter.cpp:249`). `BP_Item_ErikaBow` had
`MovesetOverlay` unset, so `SetAnimationOverlay` received an empty tag, missed the map, and fell
into `RemoveOverlay()` (`ACFAnimInstance.cpp:111`) - which nulls `currentOverlayInstance`.

Note the asymmetry that made this confusing to read from a live probe: `SetMoveset` has NO else
branch (`:51-64`), so a miss leaves the previous moveset linked, while `SetAnimationOverlay` has one
and a miss DESTROYS the overlay. That is exactly the `moveset = ACF_UnarmedMoveset_C` /
`overlay = None` pair we measured. ACF's own `ACFBowBP` sets all three tags; ours set one.

**A trap avoided, and it would have been a regression.** `ACF_Humanoid_ABP` maps `Moveset.Bow` to an
OVERLAY but to no MOVESET, and `FindByKey` is an EXACT `FGameplayTag` match (`ACFAnimTypes.h:35`).
Retagging the bow to `Moveset.Bow` without first adding a moveset entry would have made `SetMoveset`
silently find nothing and killed her locomotion outright. The `Moveset.Bow` moveset entry in
`ABP_GS_Archer` is what makes the tag safe to set.

**Verified live in PIE, all four archers:**
```
animBP=ABP_GS_Archer_C  moveset=ACF_UnarmedMoveset_C  overlay=ACF_MMBowOverlay_C  |acc|=3493.5
```
Moveset for the legs, bow overlay for the upper body, real acceleration driving locomotion.

**NOT VERIFIED - needs Michael's eyes.** Whether the bow is now held correctly is a visual
judgement and no measurement here can answer it. The overlay LINKS; that is all this ticket
establishes. Single-frame pose assets played standalone on a probe do not show what the overlay
produces in game, so the stills in the scratchpad are not evidence either way.

**Also corrected in passing:** my own earlier claim that the bow was broken because our skeleton
lacks `UpperBodyMask` was wrong in three separate links, and is retracted in `AGENT_STATE.md`. The
real cause is one unset field.

## Refine

1. Michael looks at her holding the bow. If the pose is wrong, the next suspect is the overlay's
   blend mask resolving across skeletons (`AnimationRuntime.cpp:2488-2491`), NOT the tag chain.
2. `MovesetActions` is still unset on the bow - that is the third tag, and it selects the action/
   montage set. Likely needed before she can draw and loose with animation.
3. The `AM_HU_*` hit reacts remain stranded on the Mixamo skeleton. ACF's `Actions` set (16 montages
   incl. `AM_HitLarge`, `AM_Impact`, `MeleeCombo_AM`) is already on `ACF_UE5Manny` and needs NO
   retarget - adopt those rather than retargeting ours.
4. The eight `cc_base_*twist*` bones still carry weight but are never driven; ACF's clips animate
   `lowerarm_twist_01_l` etc. Pure rename, cosmetic, deliberately not done unsupervised.
