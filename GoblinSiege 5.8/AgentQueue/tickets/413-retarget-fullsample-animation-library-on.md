---
id: 413
title: Retarget FullSample animation library onto the conformed human rig
agent: claude-retarget
status: review
claimed: 2026-09-10T18:47Z
build: none
waiting_on:
evaluated: 2026-09-10T18:52:22Z
observed:
scenario:
files: 
  - none-content-only
---

## Goal

Retarget ACF's FullSample animation library onto the conformed human rig, so the ACF AnimBP from
#412 has animations to play.

## Generate

Three assets under `/Game/Characters/ACFRigs/Human`, all built from Python:

1. **`IK_SK_Human_Manny`** - target IK Rig on `SK_Human_Manny`, retarget root `root`, with **19
   retarget chains mirrored one-for-one from ACF's own `IK_ACF_UE5Manny`**.
2. **`RTG_ACFManny_To_Human`** - the retargeter, `IK_ACF_UE5Manny` -> `IK_SK_Human_Manny`, full
   default op stack (Pelvis Motion, FK Chains, Run IK Rig, Root Motion, Remap Curves), **19/19
   chains mapped**.
3. **`Anims/` - 66 retargeted locomotion sequences**, the FullSample Unarmed set, suffixed `_GSH`.

## Evaluate

**The conform paid off exactly where it was supposed to.** ACF's source rig defines 27 chains; every
bone name in them exists on the conformed rig except `spine_05` (we have three spines, so the Spine
chain ends at `spine_03`) and the eight metacarpal chains (our rig has no metacarpals). The
remaining **19 mapped by identical name** - no manual bone pairing, no guesswork. Before the conform
this would have been 27 hand-authored mappings between `Hips`/`LeftUpLeg` and `pelvis`/`thigh_l`.

**Verified after retargeting, not assumed:**

- 66 of 66 assets retargeted, and all 66 report `SK_Human_Manny_Skeleton`.
- `MM_Unarmed_Jog_Fwd_GSH`: 1.70 s, **root travel 1829.7 uu**, `enable_root_motion = True`
- `MM_Unarmed_Walk_Fwd_GSH`: 2.20 s, root travel 1235.6 uu, `enable_root_motion = True`

**Root motion arrived switched ON, with real travel on the root track.** ACF's own animations author
it, and the conformed rig has a genuine `root` bone to receive it. This is the thing I incorrectly
told Michael we did not have; it is now working end to end, from ACF's library onto a GS rig.

**The #333 workaround does not apply and was deliberately NOT used.** That ticket disabled the Root
Motion op and zeroed Pelvis Motion `ScaleHorizontal` because the old human rig was ROOTLESS - bone 0
was `Hips`, so retargeted travel had nowhere to go but the hips. The conformed rig has a real root,
so the default op stack is correct and root motion is kept.

**Two API traps worth recording**, both of which reported success while doing nothing:

- `IKRetargeterController.set_source_chain` returns true and writes nothing when the retargeter has
  **no ops** - `get_num_retarget_ops()` was 0. Chain mappings live inside ops in UE 5.6+, so
  `add_default_ops()` must run first. Nineteen "successful" mappings read back as `None` before this
  was found.
- An earlier script died before `save_asset`, discarding everything it had configured in memory. The
  retargeter looked empty on reload for reasons that had nothing to do with the API.

**NOT DONE:** the AnimBP still has no moveset or overlay assigned, so nothing plays yet - these are
66 sequences sitting on the right skeleton, not a working locomotion set. Nobody has looked at any of
them. Only the Unarmed locomotion folder was taken; the 604 SimpleCombat animations, 69 Rifle, 65
Pistol, ladder and swim sets are untouched.

## Refine

Next: author a moveset implementing `ACF_Moveset_ALI` against these 66, assign it to
`ABP_GS_Human_ACF`, and look at Erika actually walking. That is the first point where any of this is
visible rather than measured.
