---
id: 123
title: Animation pass 2a: lengthen the outlier montage blends so flinches stop cutting in
agent: claude-animsmooth
status: done
claimed: 2026-08-10T23:33Z
build: none
waiting_on:
evaluated: 2026-08-10T23:35:16Z
files: 
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_HitReact_Front.uasset
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_HitReact_Left.uasset
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_Block_React.uasset
  - GoblinSiege 5.8/Content/Characters/ScoutV2/Montages/AM_GS_HitReact_Front.uasset
  - GoblinSiege 5.8/Content/Characters/ScoutV2/Montages/AM_GS_HitReact_Left.uasset
  - GoblinSiege 5.8/Content/Characters/ScoutV2/Montages/AM_GS_Block_React.uasset
---

## Goal

Animation pass 2a: lengthen the outlier montage blends so flinches stop cutting in

## Generate

Step 2 of Michael's animation pass, addressing "popping between actions".

Surveyed all 14 combat montages across both rigs. Attack montages blend in over 0.18s and out over
0.25s, which is unremarkable. Three per rig were outliers, cutting in at 50-60ms:

| montage | blend in | blend out |
|---|---|---|
| `AM_*_HitReact_Front` | 0.060 -> **0.120** | 0.200 -> **0.250** |
| `AM_*_HitReact_Left` | 0.060 -> **0.120** | 0.200 -> **0.250** |
| `AM_*_Block_React` | 0.050 -> **0.100** | 0.180 -> **0.220** |

A 60ms blend-in is a cut, not a blend, and the flinch is precisely the montage that INTERRUPTS
another one - a swing in progress - so it is the transition most likely to read as a pop. Doubling
it to 120ms is still fast enough to feel like a reaction rather than a delay.

Attack and block-idle montages deliberately untouched: their blends were already in a sane range,
and changing everything at once would make it impossible to tell what did what.

Applied identically to both rigs so humans and goblins keep matching feel.

## Evaluate

**Verified:** every montage re-read after writing - all six landed on the intended values, all
`saved=True`, and a full listing of all 14 confirms the attack montages were not disturbed. No
rebuild needed; these are asset properties.

**NOT verified:** how it looks. Blend feel is subjective and this is a taste change - the numbers
are defensible (a 60ms blend is a cut) but only Michael can say whether the flinch now reads as
smooth or as mushy. If 0.12 feels laggy on the flinch, the dial is `set_blend_in` and 0.09 is the
obvious next stop.

**Not addressed by this ticket:** popping caused by something other than blend duration. If actions
still snap after this, the remaining suspects are the `LayeredBoneBlend` in `ABP_Human` (its
LayerSetup was never readable through the Python API) and montage interruption behaviour rather than
the blend times themselves.

## Refine

**Deliberately conservative.** The temptation was to raise every blend in the set and call the
popping fixed. Changing 14 values at once would have made the result unattributable, and the attack
blends had no evidence against them - the outliers did.

**Left undone in this pass:** the melee facing snap and the locomotion blendspace, both continuing
in their own tickets.
