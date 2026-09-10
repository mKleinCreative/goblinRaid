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

---

## Michael caught a real defect: "I'm not sure the arm animations went through"

He was right, and the cause was mine.

**The target IK Rig had no goals and no solver.** I mirrored ACF's 19 retarget chains including their
`ik_goal_name` fields (`hand_l_Goal`, `hand_r_Goal`, `foot_l_Goal`, `foot_r_Goal`) but never created
the goals themselves or an IK solver, so the retargeter's "Run IK Rig" op had nothing to run:

```
SOURCE IK_ACF_UE5Manny     goals=[hand_l_Goal, hand_r_Goal, foot_l_Goal, foot_r_Goal]  solvers=1
TARGET IK_SK_Human_Manny   goals=[]                                                     solvers=0
```

Measured against the source animation, sampling local bone rotation range over the clip:

| bone | source | before fix | after fix |
|---|---|---|---|
| `upperarm_l` | 111.2 deg | **57.4 deg** | **109.2 deg** |
| `calf_l` | 110.8 | 110.2 | 101.5 |
| `spine_01` | 9.8 | 9.5 | 16.6 |

Arm motion was arriving at roughly HALF its source range. `apply_auto_fbik()` on the target rig
generated exactly the four goals ACF uses, with the same names, one solver, all four connected - then
re-running the batch restored `upperarm_l` to 109.2 against the source's 111.2.

**All 66 sequences were re-retargeted with the fixed rig.**

## Unresolved, and deliberately not chased further on a bad instrument

`pelvis` still measures 2.4 deg of local rotation range against the source's 84.5. But the Pelvis
Motion op is at defaults with nothing damped - `rotation_alpha 1.0`, `translation_alpha 1.0`,
`scale_horizontal 1.0`, `scale_vertical 1.0` - so there is no obvious cause in configuration.

**The metric is crude and known-noisy:** it is a max euler-component spread, and it reports ~350 deg
for `lowerarm_l`, `thigh_l` and `clavicle_l`, which is angle wrap rather than real motion. A reading
it produces should not be trusted on its own. The pelvis may be genuinely flat, or the difference may
be that the synthesised pelvis has a different local orientation so the same world motion reads
differently. Handing this to Michael's eye rather than tuning against a number I do not trust.
