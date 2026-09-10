---
id: 410
title: Conform GS rigs to ACF Manny bone standard in Blender
agent: claude-rig-conform
status: review
claimed: 2026-09-10T17:15Z
build: none
waiting_on:
evaluated: 2026-09-10T17:21:45Z
observed:
scenario:
files: 
  - none-investigation-only
---

## Goal

Michael: *"Can you just rename the fucking bones on the rig?"* then *"we have access to blender as
well, you're making it much more complicated than it needs to be and trying to work around processes
which is what got us to this hole in the first place."*

He was right. I was reaching for in-editor workarounds - virtual bones, Compatible Skeletons, IK
Retargeters, downgrading to `ACF_SimpleTemplate_ABP` - when the actual answer is to conform the rigs
to what ACF expects.

## Generate

`scratchpad/rig/conform.py`, run headless through Blender 5.1.2
(`blender.exe --background --python`). Exports both GS skeletal meshes from Unreal
(`unreal.Exporter.run_asset_export_task`), renames every bone to the Manny convention, adds the
bones that were missing entirely, and writes a conformed FBX.

**Mapping, human (Mixamo -> Manny):** `Hips`->`pelvis`, `Spine/1/2`->`spine_01/02/03`,
`Neck`->`neck_01`, `LeftShoulder/Arm/ForeArm/Hand`->`clavicle_l/upperarm_l/lowerarm_l/hand_l`,
`LeftUpLeg/Leg/Foot/ToeBase`->`thigh_l/calf_l/foot_l/ball_l`, all five finger chains ->
`thumb_0N_l`/`index_0N_l`/`middle_0N_l`/`ring_0N_l`/`pinky_0N_l`, mirrored for `_r`.

**Mapping, goblin:** `Root`->`root`, `Hip`->`pelvis`, `Waist/Spine01/Spine02`->`spine_01/02/03`,
`NeckTwist01/02`->`neck_01/02`, `L_Clavicle/Upperarm/Forearm/Hand`->`clavicle_l/...`,
`L_Thigh/Calf/Foot/ToeBase`->`thigh_l/calf_l/foot_l/ball_l`, all eight twist chains ->
`upperarm_twist_0N_l` etc.

**Added to both:** `root`, `ik_foot_root`, `ik_foot_l/r`, `ik_hand_root`, `ik_hand_gun`,
`ik_hand_l/r`. Neither rig had a single `ik_*` bone - renaming could never have produced them.

## Evaluate

**Result, verified by re-importing the written FBX and re-running the gate:**

| rig | bones | root | pelvis | spine_01 | ik_foot_root | ik_foot_l/r | foot_l | thigh_l | root count |
|---|---|---|---|---|---|---|---|---|---|
| goblin | 48 | Y | Y | Y | Y | Y | Y | Y | 1 |
| human | 75 | Y | Y | Y | Y | Y | Y | Y | 1 |

Both previously failed on `pelvis`/`ik_foot_root`/`ik_foot_l`/`foot_l`/`thigh_l`. **This retires the
`ACF_SimpleTemplate_ABP` compromise** - `ACF_Template_ABP` + `ACF_BaseMoveset` and its 10 warping
nodes are now valid on both rigs.

**Two defects caught by measuring the output rather than trusting the run**, both of which would have
imported as broken rigs:

1. **`MISSING: ['Hips']` / `['Root']`.** Blender's FBX importer absorbs a parentless root bone into
   the armature OBJECT. The human arrived as three separate roots (`spine_01`, `thigh_l`, `thigh_r`)
   with no pelvis at all. Fixed by synthesising `pelvis` at `spine_01`'s head and reparenting the
   three.
2. **`HAS ROOT: False` on the goblin**, whose source rig *does* author `Root` - same absorption. The
   IK roots then had no parent and became two more skeleton roots. Fixed by creating `root`
   unconditionally and parenting `pelvis` and both IK roots beneath it.

**Corrected Michael's instruction on one point, deliberately.** He approved `Pelvis`->`spine_01`, but
the hierarchy read from the actual rig is `Hip -> {Pelvis -> thighs, Waist -> Spine01}`: `Pelvis`
parents the LEGS, not the spine. Renaming it `spine_01` would have put both legs under the spine
chain. Mapped `Hip`->`pelvis` instead and left `Pelvis` as an extra bone between `pelvis` and the
thighs; ACF ignores bones it does not reference. Verified after the run: `pelvis children:
['Pelvis', 'spine_01']`.

**NOT DONE - nothing has entered the project yet.** The conformed FBXs are in the session scratchpad
(`rig/SK_Human_Manny.fbx`, `rig/GOB_Scout_Manny.fbx`). They have not been imported, no character
points at them, and nothing has been looked at in the editor. Skinning, bone orientations and the
IK bone transforms are unverified beyond names and hierarchy - the IK bones were placed at their
corresponding foot/hand head positions, which is the convention, but no one has seen the mesh
deform.

## Refine

Next, in order: import both as new skeletons -> point ONE character (a castle guard) at the human
one -> build an AnimBP from `ACF_Template_ABP` -> confirm the equipment-to-pose chain grips a weapon
correctly -> only then the player and the goblins, and FullSample's ~1000 animations.

Michael has ruled the existing animations expendable ("I don't care about the current animations,
I'd rather we have the complete fresh start"), so the 177 goblin sequences / 55 goblin montages / 17
human montages bound to the old skeletons are NOT being migrated.

**`conform.py` is worth keeping** - it is re-runnable whenever the art changes, and it encodes both
importer defects above. It currently lives in the session scratchpad and will be lost. Suggest
`Tools/Rig/conform_to_manny.py`; Michael's call.
