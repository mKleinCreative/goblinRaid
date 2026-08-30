---
id: 333
title: Human locomotion + attacks move onto CombatMasterBundle PowerfulSword (retarget Manny_UE5 -> SK_Human)
agent: claude-acf
status: done
claimed: 2026-08-27T22:49Z
build: none
waiting_on: 
evaluated: 2026-08-28T23:25:51Z
observed: 2026-08-28T23:25:06Z | The retargeted CombatMasterBundle locomotion ran in PIE: seven guards walked and ran their patrol routes on the rebuilt 27-sample BS_GS_Locomotion_Hu, watched on screen by Michael, with no sliding or snapping of the kind the old hips-baked Mixamo clips produced. The attack montage was also confirmed live - TryLightAttack() on GS_Guard_A1 returned true and AM_HU_Atk_Light played on the UpperBody slot, which is the montage half this ticket had never seen run.
scenario: PIE on L_Tutorial_Island, guards patrolling their roads, plus a direct TryLightAttack() invocation on a live guard with the anim instance queried for the active montage
files: 
  - Content/Characters/Humans/Retarget/RTG_Manny_To_Human.uasset
  - Content/Characters/Humans/Anims/CMB
  - Content/Characters/Humans/Anims/BS_GS_Locomotion_Hu.uasset
  - Content/Characters/Humans/Anims_Combat/AM_HU_Atk_Light.uasset
  - Content/Characters/Humans/Anims_Combat/AM_HU_Atk_Heavy.uasset
  - Content/Characters/Humans/Anims/A_HU_Std_WalkF.uasset
  - Content/Characters/Humans/Anims/A_HU_Std_WalkB.uasset
  - Content/Characters/Humans/Anims/A_HU_Std_WalkL.uasset
  - Content/Characters/Humans/Anims/A_HU_Std_WalkR.uasset
  - Content/Characters/Humans/Anims/A_HU_Std_RunF.uasset
  - Content/Characters/Humans/Anims/A_HU_Std_RunB.uasset
  - Content/Characters/Humans/Anims/A_HU_Std_RunL.uasset
  - Content/Characters/Humans/Anims/A_HU_Std_RunR.uasset
---

## Goal

Human locomotion + attacks move onto CombatMasterBundle PowerfulSword (retarget Manny_UE5 -> SK_Human)

## Generate

Human locomotion and the two guard attacks now come from `/Game/CombatMasterBundle` (PowerfulSword),
retargeted onto `SK_Human_Skeleton`. Michael's call: PowerfulSword for locomotion, `Anim_PS_Combo_A1_RM`
and `Anim_PS_Combo_C1_RM` for the attacks.

**Why the bundle and not the Mixamo clips.** The 8 `A_HU_Std_*` clips have travel BAKED INTO THE HIPS
(223uu on WalkF) and the rig has NO ROOT BONE - bone 0 is `Hips`. So yesterday's `force_root_lock=True`
pinned the hips outright and took the bob and sway with it: Hips Zspan measured 0.00 on all 8 clips
while `A_HU_Std_Idle` (never locked) still had 6.76uu. That is the "little stiff" Michael saw. The
bundle's clips put travel on a real `root` bone and leave the pelvis free - source `Anim_PS_Walk_F_RM`
measures root travel 161uu with **pelvis travel 0.1uu and 4.68uu of bob**.

New assets:
- `Content/Characters/Humans/Retarget/IK_Manny_PS.uasset` - source rig for `SKM_Manny_PS`, 29 chains
  from `apply_auto_generated_retarget_definition()`.
- `Content/Characters/Humans/Retarget/RTG_MannyPS_To_Human.uasset` - all 15 target chains map EXACT.
  Three settings are load-bearing, all found by measurement rather than by reading:
  - **"Root Motion" op DISABLED.** By default it is `SourceRoot=root -> TargetRoot=Hips`,
    `RootMotionSource=CopyFromSourceRoot`, `RootHeightSource=CopyHeightFromSource`. Against a rootless
    target that copies Manny's root travel INTO the human's Hips (measured 280.1uu) and copies the
    root's flat height over the bob (measured 0.00). Disabling it restored bob to 8.14uu.
  - **Pelvis Motion `ScaleHorizontal=0`, `ScaleVertical=1`.** The pelvis op works in GLOBAL space, so it
    still carried root travel scaled by the 1.7398 height ratio. Zeroing horizontal alone removed the
    travel and kept the bob; it costs nothing because the source pelvis's own horizontal travel is 0.1uu.
  - `auto_align_all_bones(TARGET, CHAIN_TO_CHAIN)` - the human rig is T-pose, Manny is A-pose. Stored
    19 rotation offsets (LeftArm 54.8 deg, LeftForeArm 38.7 deg).
- `Content/Characters/Humans/Anims/CMB/` - 19 clips: 8 walk, 8 run, idle, `Combo_A1`, `Combo_C1`.

Changed:
- `BS_GS_Locomotion_Hu` - rebuilt from 15 samples (5 directions x 3 speeds) to **27** (9 x 3).
  Direction grid 4 -> 8, `bWrapInput` on. Speed rows 0 / **280.1** / **606.3**, derived from source root
  travel over clip length times the 1.7398 retarget scale, so stride matches ground speed.
- `AM_HU_Atk_Light` -> `A_HU_PS_Combo_A1_RM`; `AM_HU_Atk_Heavy` -> `A_HU_PS_Combo_C1_RM`. `UpperBody`
  slot preserved on both. Neither montage had notifies to lose.

## Evaluate

**Verified by measurement on all 19 clips** (composed bone chain from asset data, no editor ticking):
Hips travel **0.00uu** on every clip; bob 2.76 (idle) to 14.68 (runs); `LeftArm` rotation present on all;
hand lateral offset a uniform **1.15-1.22x** the scaled source - consistent across every clip, which is
rig proportion, not error. Files verified on disk by mtime + `git status`, never by a save return value.

**Verified in PIE** (`GS.Anim.Snapshot`): *"17 pawn(s), 0 in REFERENCE POSE"*, `ABP_Human_C` on every
guard. `GS.AI.LogLocomotion 8`: guards travel a flat **450uu/s, 0% rest**, which sits between the 280/606
rows - about 1.5% foot slide, so no rate scaling is needed. NOTE this PIE run happened BEFORE the blend
space repoint, so it proves the retarget is sane, NOT that the new blend space works.

**Two of my own errors, both caught by measurement, both worth recording:**
1. I ran `auto_align_all_bones` AFTER the batch of 19, then re-retargeted only `Walk_F` to test it. The
   other 18 kept the unaligned pose. Michael opened `A_HU_PS_Idle_RM` and saw a T-pose - hands lateral
   131.3 and **+61.6 above the hips**. Re-running the batch fixed it to 64.2 / -7.1 (expected 55.6 / -0.9).
   **A retargeter setting proves nothing until every clip has been re-run through it.**
2. **My live probe died mid-session and I read three poses off a corpse.** A `SkeletalMeshActor` in
   single-node mode stops ticking when its anim asset is overwritten underneath it; `get_position()`
   then returns 0.000 and `get_bone_transform` hands back the REF POSE - which for this rig IS a T-pose.
   Two conclusions drawn from it were wrong and I stated one to Michael before catching it. **Print
   `get_position()` beside every probe reading and treat an unchanged value as a dead instrument.** The
   composed-bone-chain method (`get_bone_pose_for_frame` + `MathLibrary.compose_transforms`, root to
   leaf) needs no ticking and is what every number above rests on.

**Written but NOT observed running:** the 27-sample blend space and both attack montages have never been
seen in PIE. Nobody has watched a guard walk or swing with these assets.

**Known cosmetic defect, self-correcting.** Both montages carry a stale `SequenceLength` (Light 1.15 vs a
1.3167 clip, Heavy 2.7333 vs 1.5) because `SequenceLength` is read-only from Python and no reachable path
recalculates it - reload, a `rate_scale` nudge and a notify add/remove all failed (3 attempts, then
stopped per rule 3). It does not need fixing: `UAnimMontage::PostLoad`
(`Engine/Private/Animation/AnimMontage.cpp:464-470`) detects the mismatch, logs *"Please resave the
asset"* and calls `SetCompositeLength(CalculateSequenceLength())`. Resave after an editor restart to
clear the log line.


**UPDATE 2026-08-28 (claude-acf) - the two gaps above are now closed.** The caveat in this section
("this PIE run happened BEFORE the blend space repoint, so it proves the retarget is sane, NOT that
the new blend space works") no longer stands:

- **The rebuilt 27-sample `BS_GS_Locomotion_Hu` has now run.** Seven guards patrolled their roads in
  PIE on `L_Tutorial_Island`, watched on screen by Michael, walking and running the retargeted
  CombatMasterBundle clips. No sliding or snap-back of the kind the hips-baked Mixamo clips produced.
- **The montage half has now run too.** `TryLightAttack()` on a live `GS_Guard_A1` returned true and
  the anim instance reported `AM_HU_Atk_Light` playing on the `UpperBody` slot.

## Refine

- Dropped the plan to re-download the 8 clips from Mixamo with IN PLACE checked. It would have worked -
  `_MixamoStaging/_DOWNLOAD_SETTINGS.txt` already mandates it for travelling clips - but the bundle was
  already on disk, needs no downloads, and brings 8 directions instead of 4 plus a matching sword stance.
  **I asserted twice that no combat pack was installed. I had searched only `/Plugins` and
  `/Game/Characters` and missed `/Game/CombatMasterBundle` entirely - 1305 clips. Michael corrected me.**
  The lesson is the same one #330 paid for: search the whole content root before concluding ACF or a
  vendor pack does not ship something.
- Kept the old `A_HU_Std_*` clips and their `force_root_lock=True` rather than reverting. They are now
  unreferenced by the blend space; whether to delete them is a separate decision.
- Left `MaxWalkSpeed` alone - patrol-walks/combat-runs is #334.

> 2026-08-28T01:00Z Adopted by claude-acf (was claude-anims). Same body of work as 334/339; its blend space is unobserved and will be watched in the same PIE session.
