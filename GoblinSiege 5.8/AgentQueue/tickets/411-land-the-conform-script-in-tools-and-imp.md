---
id: 411
title: Land the conform script in Tools and import the conformed human rig
agent: claude-rig-import
status: review
claimed: 2026-09-10T17:26Z
build: none
waiting_on:
evaluated: 2026-09-10T17:28:12Z
observed:
scenario:
files: 
  - Tools/Rig/conform_to_manny.py
---

## Goal

Michael: "move it and then use it to get the first bit of work done."

## Generate

1. **`Tools/Rig/conform_to_manny.py`** - the Blender conform pass from #410, landed with a header
   explaining why it exists (ACF is authored against `ACF_UE5Manny`; `FAnimNode_StrideWarping::
   IsValidToEvaluate` returns false **with no log** without `pelvis`/`ik_foot_root`), the usage line,
   the two Blender FBX-importer defects it guards against, and the `Hip`/`Pelvis` mapping subtlety.
   Re-run from its new home to confirm it still works before relying on it.
2. **Imported the conformed human rig** as `/Game/Characters/ACFRigs/Human/SK_Human_Manny` with a
   NEW skeleton `SK_Human_Manny_Skeleton` (`FbxImportUI.skeleton = None`), physics asset generated,
   animations off.

## Evaluate

**All 24 checked bones present in the IMPORTED Unreal skeleton**, i.e. the conform survived the
round trip through Blender and the FBX importer:

`root`, `pelvis`, `spine_01/02/03`, `neck_01`, `head`, `clavicle_l`, `upperarm_l`, `lowerarm_l`,
`hand_l`, `thigh_l`, `calf_l`, `foot_l`, `ball_l`, `thumb_01_l`, `index_01_l`, and all seven IK
bones (`ik_foot_root`, `ik_foot_l/r`, `ik_hand_root`, `ik_hand_gun`, `ik_hand_l/r`).

**Geometry is intact** - the new mesh measures identically to its source:

| | extent | origin | materials |
|---|---|---|---|
| `SK_ErikaArcher_baked` (source) | (150.8, 37.0, 150.1) | (-0.1, -3.2, 149.1) | 4 |
| `SK_Human_Manny` (conformed) | (150.8, 37.0, 150.1) | (-0.1, -3.2, 149.1) | 4 |

**NOT VERIFIED - needs eyes, and this is the honest limit of what a bone list proves.** Nobody has
looked at the mesh. Bone NAMES, hierarchy, a single root and geometry bounds are confirmed; skin
weights, bone orientations and the placement of the seven synthesised IK bones are not. The IK bones
were placed at their corresponding foot/hand head positions, which is the convention, but a rig can
have every bone correctly named and still deform wrongly.

No character points at this skeleton yet. Nothing in the shipping game has changed.

**Note on tracking:** `Content/*` is gitignored (`.gitignore:32`), so the imported assets are not in
the repo - which is now a feature rather than a gap, because `Tools/Rig/conform_to_manny.py` plus
the source mesh regenerates them.

## Refine

Next: point ONE castle guard at `SK_Human_Manny_Skeleton`, build an AnimBP from `ACF_Template_ABP`
(valid now that the IK bones exist), and confirm the equipment-to-pose chain grips a weapon
correctly - before the player, the goblins, or FullSample's animation library.
