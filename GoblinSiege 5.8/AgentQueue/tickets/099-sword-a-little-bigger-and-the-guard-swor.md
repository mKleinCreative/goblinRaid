---
id: 099
title: Sword a little bigger, and the guard sword has no material
agent: claude-gobkit
status: done
claimed: 2026-08-09T07:47Z
build: none
waiting_on:
evaluated: 2026-08-09T07:56:04Z
files: 
  - Content/Data/Weapons/DA_Weapon_HordeGoblin.uasset
  - Content/Data/Weapons/DA_Weapon_Guard.uasset
  - Content/_Import/Weapons/GS_Sword_Guard.uasset
---

## Goal

Sword a little bigger, and the guard sword has no material

## Generate

Editor-only; no C++, so `build: none`.

**The blank material - root cause is a blend weight, not a missing texture.** The parent material
`FBXLegacyPhongSurfaceMaterial` mixes a flat `DiffuseColor` with `DiffuseColorMap` through a scalar,
**`DiffuseColorMapWeight`**. The goblin sword's instance sets it to 1.0. The guard's never overrode it,
so it sat at the 0 default: the map was multiplied in at nothing and the flat white `DiffuseColor`
won outright. **Set `DiffuseColorMapWeight = 1.0`** on the guard MIC - matching the working asset
rather than picking a number.

The texture was genuinely absent as well: the FBX carried no embedded image, so the MIC had **zero**
texture parameters. Found the source in `medieval sword 3d model.zip` sitting beside the FBX in
Downloads (2.9 MB against the FBX's 164 KB - the size gap was the tell), extracted the `.fbm` sidecar
and imported `GS_Sword_Guard_basecolor` (4096x4096).

**Size** - `MeleeMeshOffset.Scale3D` **1.2 -> 1.45** (~+20%) on `DA_Weapon_Guard` and
`DA_Weapon_HordeGoblin`.

## Evaluate

**The material is verified BY EYE, which is what this ticket needed.** Captured the guard sword beside
the goblin sword as a control, in **unlit** so lighting could not be blamed: before, the guard was a
flat white silhouette while the control showed its full texture; after, it reads as a steel blade with
the fuller and a darkened edge, crossguard and pommel. Screenshots in the session scratchpad.

**Four wrong hypotheses, each killed by evidence rather than by argument** - worth recording because
each one looked right:

1. *Texture missing.* Half true - it was missing, but importing it changed nothing.
2. *Mesh has no UVs.* Killed by putting the **goblin's** material on the **guard's** mesh: it rendered
   fully textured, so the UVs are fine and the material system works.
3. *Source JPG is white/progressive/CMYK.* Killed by decoding it directly - plain 24-bit RGB, sampling
   steel grey (153,154,148) and (116,117,112).
4. *Wrong FBX* (the loose file is 164 KB, the zip's is 143 KB - genuinely different files). Re-imported
   from the zip's FBX, still white. **Bounds came back byte-identical (11.8, 2.6, 49.9, long axis Z),
   which is the useful part: the tuned `MeleeMeshOffset` from #097 survives the re-import.**

Only then did diffing the two instances' full override sets against the parent's parameter list show
the weight. **The lesson: on these Interchange/Phong instances, an assigned texture parameter proves
nothing - the `*MapWeight` scalar decides whether it is used at all.**

**Scale verified in PIE**, all three combatants at once:

```
BP_HordeGoblin_C_0       GS_Sword        scale 1.45  0.0uu from hand_r_weapon  mat=tripo_...3f0d378a
BP_KnightDPelegrini_C_0  GS_Sword_Guard  scale 1.45  0.0uu from RightHand      mat=tripo_...ea1861e8
BP_CastleGuard01_C_0     GS_Sword_Guard  scale 1.45  0.0uu from RightHand      mat=tripo_...ea1861e8
```

**Not verified:** whether 1.45 is the size Michael actually wants - "a little bit bigger" is a taste
judgement and only he can close it. One knob, no rebuild. The guard sword's **rotation** in the hand
is still unverified from #097 and this ticket did not address it.

## Refine

**Reverted `DA_Weapon_Scout`.** I bumped the player's sword to 1.45 in the same pass out of a wish for
consistency, then put it back to 1.2: Michael raised the NPC swords, and silently resizing the weapon
he looks at every frame is a change he did not ask for.

**Kept the re-imported mesh** even though the re-import was not what fixed it - the zip's FBX is the
one the texture's `.fbm` sidecar actually belongs to, and its bounds are identical, so it is the more
honest source with no cost.

**Deliberately left undone:** the guard sword's rotation (needs an eye), Erika's missing bow, and the
guard MIC's `SpecularColorMapWeight` / `NormalMapWeight` - the download shipped a basecolor only, so
there is nothing for those to weight.
