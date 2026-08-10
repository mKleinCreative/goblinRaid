---
id: 103
title: Bake GS_Sword_Guard pivot to the grip so socket preview is WYSIWYG
agent: claude-gobkit
status: done
claimed: 2026-08-09T18:57Z
build: none
waiting_on:
evaluated: 2026-08-09T18:59:49Z
files: 
  - Content/_Import/Weapons/GS_Sword_Guard.uasset
  - Content/Data/Weapons/DA_Weapon_Guard.uasset
---

## Goal

Bake GS_Sword_Guard pivot to the grip so socket preview is WYSIWYG

## Generate

Michael wants to tune the socket rotation with a live preview. The Skeleton Tree's **Add Preview
Asset** does exactly that - but it attaches the mesh by its ORIGIN with only the socket transform,
and knows nothing about `MeleeMeshOffset`. With `GS_Sword_Guard`'s origin at its TIP and the data
asset compensating with `pitch -90` plus a 144.7uu pull-back, the preview would have lied precisely
where he needed it to be honest.

**So the compensation was removed from the data asset and baked into the mesh**, via Geometry Script:

```
copy_mesh_from_static_mesh -> transform_mesh(Roll 180, +99.8047 Z) -> copy_mesh_to_static_mesh
```

Roll 180 sends z -> -z, which is a **true rotation, not a mirror** (determinant +1), so winding and
normals survive and no normal/tangent recompute was needed. The translation then lands the grip
exactly on the origin. `GS_Sword_Guard` now matches `GS_Sword`'s convention: grip at the pivot, blade
running out along +Z.

`DA_Weapon_Guard.MeleeMeshOffset` rotation and translation both go to **identity/zero**.

## Evaluate

**The pivot really moved, checked by geometry rather than by eye** - the vertex radius profile is now
the mirror of what it was:

```
before:  z 0-10 r=3.0 (tip) ... z 60-80 r=11.8 (crossguard) ... z 80-99.8 (grip)
after :  z 0-20 r=4.0/2.6 (grip/pommel) ... z 20-40 r=11.8/10.7 (crossguard) ... z 90-99.8 r=3.0 (tip)
```

**Verified in PIE with a completely identity offset**, which is the whole point - nothing is
compensating any more:

```
identity offset + hand_r_weapon:  GRIP 0.0uu   TIP 144.7uu   (blade 144.7uu)
```

Vert count unchanged (5814) and the material survived the round-trip
(`tripo_node_ea1861e8..._material`), which was the real risk in rewriting an imported asset.

**One place the preview still will not match the game: SCALE.** The preview shows the sword at 1.0
while `DA_Weapon_Guard` renders it at 1.45. Rotation and position - the things being tuned - are
exact. Called out rather than silently accepted.

**Not verified:** how it looks. No screenshot this pass; the numbers are unambiguous and the arena's
four `BP_GSPlayerCharacter` pawns made the last four capture attempts useless.

**No backup asset left in the repo.** The mesh is re-importable from
`C:\Users\Michael\Downloads\medieval sword 3d model.zip` if the bake ever needs undoing - noted here
rather than leaving a `_orig` duplicate lying in `/Game/_Import`.

## Refine

**Used the measured bound, not my constant.** The first pass translated by the 99.8 I had been
carrying since #100 and left the mesh at `z -0.0047..99.8000` - pivot fractionally inside the grip.
The real length is **99.8046875**. Re-ran from the actual bounding box and it lands on 0.0000.

**Left the scale in the data asset rather than moving it to the socket.** Putting 1.45 on
`hand_r_weapon` would have made the preview perfect - and would have scaled every weapon that ever
attaches to that socket, which is a trap for whoever adds the second one.

**Deliberately left undone:** the preview attachment itself. `PreviewAttachedAssetContainer` is not
exposed to Python, so it is a UI action for Michael - Skeleton Tree, right-click the socket, Add
Preview Asset. Also undone: `DA_Weapon_HordeGoblin` and `DA_Weapon_Scout` need nothing, since
`GS_Sword` already had this convention.
