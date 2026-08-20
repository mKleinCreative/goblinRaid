---
id: 182
title: Replace goblin combat+locomotion animation with CombatMasterBundle: retarget DK2 in-place loco (16) and DTA combat (29) onto GOB_Scout_v2_Skeleton
agent: claude-packanim
status: done
claimed: 2026-08-18T01:33Z
build: none
waiting_on:
evaluated: 2026-08-18T06:24:48Z
observed: 2026-08-18T02:51:31Z | Michael hand-authored BS_GS_Loco_Pack in the editor and I wired it into the goblin AnimBP. Watched ten horn-summoned goblins move around the arena and every one of them animated - nothing froze. GS.Anim.Snapshot read 17 pawns with 0 in reference pose, and the goblins were reporting directions right across the axis (-138, -133, -124, -63, -12, 27, 81, 168) rather than all sitting in one column, so the eight-way set is genuinely being sampled and not just the forward clip. Six Python-authored attempts at this asset across the project all produced corpses; the hand-authored one works first time.
scenario: PIE on L_CombatArena, GS.Horde.SpawnTest with ten horde goblins plus the player and six human defenders
files: 
  - Content/Characters/ScoutV2/Anims_Pack
  - Content/Characters/ScoutV2/Animations/BS_GS_Loco_Pack.uasset
---

## Goal

Replace goblin combat+locomotion animation with CombatMasterBundle: retarget DK2 in-place loco (16) and DTA combat (29) onto GOB_Scout_v2_Skeleton

## Generate

**51 pack clips retargeted onto GOB_Scout_v2_Skeleton**, all under
`Content/Characters/ScoutV2/Anims_Pack/`:
- 16 in-place 8-way locomotion from DynamicKatanaV2 -> `A_GOB_DK2_{Walk,Run}_{F,FL,FR,L,R,B,BL,BR}_IP`
- 29 combat from DynamicTwinAxe -> `A_GOB_DTA_*`
- 6 from DynamicAxe -> `A_GOB_DA_*` (Combo_C1/C2/C3, CommonAttack_B, Idle_A, Buff), Michael's picks

**The retarget path, built from nothing:**
- `IK_PackMannequin` - source rig. `IKRigController.apply_auto_generated_retarget_definition()`
  returned **True**, meaning the pack's cloned skeleton matched a known Mannequin template and chains
  built themselves. That return value is also the compatibility proof: bone names could not be
  enumerated any other way (`bone_tree` yields `BoneNode` structs whose `str()` is a memory address,
  and `get_skeleton_info` gives only a count).
- `RTG_PackToGoblin` - `IK_PackMannequin` -> `IK_GoblinScout_v2`. Op stack and chain mapping verified
  **identical** to the project's working `RTG_MannequinToGoblin_v2`.
- `auto_align_all_bones(SOURCE, CHAIN_TO_CHAIN)` after Michael reported hands behind the back. It
  produced real offsets on `upperarm_l/r` and `lowerarm_l/r`; all 51 clips were re-retargeted with the
  aligned pose and the hands moved to chest height. **Re-retargeting resets `enable_root_motion`** -
  the four attack clips had to have it re-applied afterwards.

**Wired into the game:** `ThirdPerson_IdleRun_2D_Gob`'s four samples swapped to pack clips, then
replaced entirely by `BS_GS_Loco_Pack` (Direction x Speed, 21 samples) on the `Idle/Run`
BlendSpacePlayer with `Direction -> X`, `Speed -> Y`.

## Evaluate

**Verified in PIE:** `GS.Anim.Snapshot` reads **17 pawns, 0 in reference pose**, and the goblins
report directions right across the axis (-138, -133, -124, -63, -12, 27, 81, 168) rather than
clustering in one column - so the eight-way set is genuinely sampled, not just the forward clip.
Two clips were also eyeballed as thumbnails and are properly posed, not reference-pose duds.

**The finding that matters most.** I authored a 2D blendspace from Python **twice** and both were
dead (11 of 17 in reference pose). Michael hand-authored one and it worked first time. That is now
six Python-authored corpses against one hand-authored success across this project, and
AGENT_STATE's "author them by hand" ruling should be read as settled rather than provisional.

**A bug of mine worth recording:** `get_editor_property('blend_parameters')` returns **copies**.
Mutating them in place silently does nothing - my axes stayed at the 0..100 default while the samples
sat at -180..180, putting every sample outside the hull. Build fresh `BlendParameter` structs and
assign the array back. The same copy-semantics trap applies to `sample_data` and to `Stages` on the
ability CDOs.

**Also learned:** UE refuses to save assets while PIE is running - `save_asset` returns False with no
dirty package. Stop PIE first or you will report a write that never landed.

**Never verified:** whether the retargeted clips are *good*, only that they are not dead. Michael has
since reported the hands read wrong; that is under investigation separately and is a fidelity
question, not a correctness one.

**Touched outside the claim:** `ThirdPerson_IdleRun_2D_Gob` and `ThirdPerson_AnimBP_Gob` were both
edited under this work; the AnimBP is claimed by my own #174 but the 1D blendspace was not claimed by
anything. Should have been added to the file list.

## Refine

- Switched locomotion source to DynamicKatanaV2 after measuring that **neither axe moveset has any
  locomotion at all** - DA and DTA are combat-only. DK2 is one of only three families shipping
  in-place clips, and the only one with a complete 8-way walk *and* run.
- Swapped all four 1D samples to pack clips as an interim step so pack locomotion was live before the
  2D blendspace existed, rather than leaving the goblin on the old set while blocked.
- **Left undone:** the idle row of `BS_GS_Loco_Pack` mixes two different idle clips
  (`A_GOB_DTA_Idle1_RM` at centre, `A_GOB_DA_Idle_A_RM` at the poles), so standing still and turning
  crossfades between them; the run row sits at Speed 800 which is the axis maximum, so the run clip
  may never fully engage at real goblin speeds. Both are tuning notes handed to Michael, not defects
  I introduced.
