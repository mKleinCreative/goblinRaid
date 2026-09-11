---
id: 411
title: Land the conform script in Tools and import the conformed human rig
agent: claude-rig-import
status: done
claimed: 2026-09-10T17:26Z
build: none
waiting_on:
evaluated: 2026-09-10T23:08:21Z
observed: 2026-09-10T23:07:23Z | Michael looked at the imported SK_Human_Manny in the editor; mesh present and skinned, geometry extent matching SK_ErikaArcher_baked
scenario: Editor viewport after import
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

---

## Evaluate, re-read 2026-09-10T22:4x (the script changed after the above was written)

`Tools/Rig/conform_to_manny.py` was edited twice after this Evaluate was stamped, and the
"NOT VERIFIED - needs eyes" caveat above turned out to be the important sentence in this ticket.
What eyes then found, in order:

1. **The sideways tilt** (#413): `root` and `pelvis` were created with tails up world Z, ~90 degrees
   off every other bone. The Pelvis Motion retarget op applies pelvis rotation in LOCAL space, so it
   went about the wrong axes. Fixed in commit `1f96013` by orienting both along `spine_01`.
2. **Orphan vertices**: the weight-restore block did `continue` on `total <= 0.0`, which silently
   abandoned the three vertices (1233, 2235, 2293) that had no weights at all. They are now rigged
   to the nearest deform bone, and the mesh re-imports with zero unweighted vertices.

**A causal claim made while fixing #2 is RETRACTED.** Those three vertices were reported as the
cause of the long triangle spikes Michael saw in the animation preview. They were not. This
ticket's own commit `692d748` documents the real mechanism - ~1,092 hip vertices left holding a
median weight sum of 0.492 after Blender deleted the `Hips` vertex group - and Unreal does not leave
zero-influence vertices undefined anyway: `MeshUtilities.cpp:4043` pins them to the root bone and
logs `"Missing influence on vert N"`, a warning that appears **zero** times in `MyProject.log`. The
orphan fix is correct hygiene and stays; the diagnosis attached to it was wrong.

**What this ticket ultimately proved, and it is the opposite of what it set out to prove:**
conforming by RENAME cannot produce a correct rig. Measured on the shipped output afterwards -

| defect | measurement |
|---|---|
| phantom `Hips` at bone index 0 | Blender's armature OBJECT re-exported as a bone; no parent, so `USkeleton::IsCompatibleMesh` returns false at `Skeleton.cpp:707` before reaching anything else. This is why ACF's own AnimBP silently refused to attach to this rig. |
| `root` at Z=173.81, not 0 | same artifact - the armature object's location. Root motion and ground plane off by 1.74m. |
| `pelvis` coincident with `spine_01` | the synthesised pelvis landed on the Mixamo *Spine* joint. Legs 2.01x Manny, torso 1.14x. |
| 90-degree bone-roll mismatch on the arm chain | our arm bones aim down local -Y; Manny's aim down local +X. |

Superseded by **#415**, which builds the skeleton FROM Manny and fits it to the mesh, so names,
hierarchy, rolls, twists and IK bones are correct by construction. `conform_to_manny.py` is retired
there; its Mixamo->Manny name map and its four documented Blender-importer defects carry over,
because that knowledge was expensive and is still true.
