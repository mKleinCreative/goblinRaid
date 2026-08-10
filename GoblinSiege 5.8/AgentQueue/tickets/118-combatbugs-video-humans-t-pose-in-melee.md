---
id: 118
title: CombatBugs video: humans T-pose in melee, and debug spheres draw in normal play
agent: claude-anim
status: done
claimed: 2026-08-10T21:40Z
build: none
waiting_on: Michael - does the T-pose still happen in a live fight? The video predates the last edit to GA_HU_SwordLight by 24 minutes, so it cannot answer this.
evaluated: 2026-08-10T22:19:37Z
files: 
  - GoblinSiege 5.8/Content/Blueprints/Abilities/Human/GA_HU_SwordLight.uasset
  - GoblinSiege 5.8/Content/Blueprints/Adversaries/BP_CastleGuard01.uasset
---

## Goal

CombatBugs video: humans T-pose in melee, and debug spheres draw in normal play

## Generate

Source: `C:\Users\Michael\Videos\CombatBugs.mp4`, 44.8s, 1614x932, recorded **2026-08-09 22:35
local**. Michael flagged "the animations are still doing the problematic jump". Frames extracted
with the ffmpeg bundled in the `imageio_ffmpeg` python package (no ffmpeg on PATH).

**What the video actually shows, at ~11.8s (game clock 29:04):** two human guards standing in a
**T-pose** - both arms straight out horizontally, legs together and straight, sword hanging limp -
while the goblins beside them animate normally. Held across many frames, not a single-frame
artifact. This is a *reference pose*: the signature of no animation playing at all.

That matters, because #113 read this symptom as an animation playing at the wrong height and fixed
it with `force_root_lock` on 9 retargeted sequences. Root-locking changes the pose of an animation
that plays. It does nothing for one that never plays.

**Also visible, and a separate bug:** cyan melee trace spheres and red hit markers drawn throughout
normal play (29:13, 29:04, 29:02). That is `GS.Combat.Debug` defaulting to `1` -
`GSGA_SwordLight.cpp:25`, the only debug cvar in the project that is on by default. The bloat audit
found it and refuted it as "live tooling, not bloat", which was right about the category and wrong
about the consequence: it means every session, for every player, draws debug geometry unless someone
remembers to type `GS.PlayerView`. This video is the evidence.

## Evaluate

**A theory I formed and then disproved, recorded because I nearly acted on it.**
`AGSCharacterBase::PlayAnimMontage` (`GSCharacterBase.cpp:251`) refuses any montage whose skeleton
differs from the character's mesh skeleton, returning 0 and logging only under
`GS.Combat.LogHitReact`. A refused montage would produce exactly the observed T-pose, silently. I
was ready to call that the cause.

Ran the duel with `GS.Combat.LogHitReact 1`: **zero `REFUSED montage` lines**, and the humans are
demonstrably playing human animations - 16x `AM_HU_HitReact_Front`, 10x `AM_HU_HitReact_Left`,
1x `AM_HU_Block_React`. The gate is not firing. Theory dead.

**What is established:**

- The human montage set is complete on disk: 7 `AM_HU_Atk_*`, `AM_HU_Block_Idle`,
  `AM_HU_Block_React`, 2 `AM_HU_HitReact_*`.
- The wiring exists in current content: `GA_HU_SwordLight` references `AM_HU_Atk_Light`,
  `GA_HU_Block` references `AM_HU_Block_Idle`, and all six adversary Blueprints reference both
  `GA_HU_SwordLight` and `GA_HU_Block` (binary name-table grep - a strong hint, not proof of the
  assigned value).
- Human hit-reacts and block-reacts play correctly in a live fight, proven by log.

**Why the video cannot close this.** `GA_HU_SwordLight.uasset` has mtime **08-09 22:59**, twenty-four
minutes AFTER the recording. Every other human combat asset predates the video. So the recording
shows a state that was subsequently edited, and it is entirely possible the attack animation was
fixed by that edit. Git cannot arbitrate - all of these assets were uncommitted until 688ad92 today,
so their history begins at my checkpoint.

**The gap.** Nothing logs whether the human ATTACK montage plays; the hit-react channel does not
cover it and the skeleton gate only logs refusals. Attacks definitely *activate* (telegraph SEEN
lines are present), so the open question is narrow: does `AM_HU_Atk_Light` visually play on
`SK_Human_Skeleton`, or does the guard hold ref pose through the swing?

## RESOLVED by #119 (2026-08-10)

The T-pose is fixed and Michael confirmed it in PIE: *"the issue was solved."*

Cause, which none of the eliminations below found and which Michael named himself: the montages
referenced the `A_MX_*_Gob` clips carrying `force_root_lock=True` from #113, pinning the root to the
reference pose while the hips kept 88uu of authored travel. #119 repointed all 11 montages onto the
clean `A_HU_*` import. He had raised root snapping before and it was rejected at the time.

The elimination map below stays as written - it is what proved the fault was NOT in the wiring, and
it is what made the two-parallel-sets discovery possible. The head wiggle he noticed afterwards is
accepted, see #119.

**Still open from this ticket and NOT carried by #119:** `GS.Combat.Debug` defaults to `1`
(`GSGA_SwordLight.cpp:25`), which is why cyan trace spheres and red hit markers are drawn in normal
play throughout the CombatBugs video. One-line C++ change, needs a build, unclaimed.

## Refine

**Not fixed, and deliberately not guessed at.** Twice in this session a reasoned diagnosis of combat
code survived inspection and failed on contact with a live run (#116's punish, and the skeleton-gate
theory above). The remaining candidates here - a null stage montage on the human ability, a
retarget that produces ref pose, or an already-fixed bug - are indistinguishable without either
watching a fight or reading the ability's stage array in the editor. Choosing between them by
argument is the exact move that has failed twice today.

**Michael confirmed 2026-08-10: they still T-pose.** So this is live, not history.

## Editor investigation (2026-08-10, via the in-editor MCP bridge)

Every link in the asset chain was read directly and every one is CORRECT:

| Checked | Result |
|---|---|
| `GA_HU_SwordLight.Stages` | 3 stages, montages `AM_HU_Atk_Light` / `_Spin` / `_Flurry` - none null |
| Guard's granted abilities | `light/heavy/block` all point at the `GA_HU_*` human set |
| Guard mesh + skeleton | `SK_CastleGuard01_baked` on `SK_Human_Skeleton`, anim class `ABP_Human` |
| Montage skeleton | `SK_Human_Skeleton` - matches the mesh, so the `PlayAnimMontage` gate passes |
| Montage slot | `DefaultSlot`, 1 track, 1 section, duration 1.15 |
| `ABP_Human` AnimGraph | HAS `Slot 'DefaultSlot'` (connected) plus a second `Slot 'UpperBody'` |
| Montage segment | resolves to a real sequence, not null |
| Sequence keys | 73 sampled keys, `force_root_lock=True` (from #113), `enable_root_motion=True` |
| **Sequence motion** | **real** - `leftarm` rot (12,-12,-44) -> (-13,35,-48) and `spine` loc (-0.7,-164.5,-35.3) -> (-84.3,-149.3,-17.1) across t=0.0..1.2 |
| Anim tick option | `ALWAYS_TICK_POSE` on guard, knight AND the player goblin - identical, so not the differentiator |

**Conclusion: this is not an asset-wiring bug.** The montage is assigned, skeleton-matched,
correctly slotted, contains genuine per-bone motion for the human rig, and the AnimGraph has the
slot to blend it into. Everything #113 and #112 were meant to establish is in place.

**Two invalid tests, recorded so nobody repeats them:**
1. `SkeletonService.get_bone_transform(skeleton, name)` returns success for ANY name, including
   `mixamorig1:Hips` which certainly does not exist. It cannot be used to test bone presence.
2. A name-table grep of a `.uasset` proves a reference exists somewhere in the package, not that a
   property is assigned. Useful as a hint only; the CDO read above is the real evidence.

## Michael's two discriminating observations (2026-08-10)

- **Distance makes no difference** - close up and across the arena both T-pose. Rules out LOD,
  significance, URO and visibility-based tick (all three character BPs are `ALWAYS_TICK_POSE`
  anyway, including the player goblin that animates correctly).
- **Only when attacking or blocking** - they walk and idle correctly. So the Locomotion state
  machine is healthy and the fault is on the montage path specifically.

## Elimination map - what is PROVEN NOT the cause

Every one of these was read from the live editor, not inferred:

| Ruled out | Evidence |
|---|---|
| Missing montage assignment | 3 stages, all populated |
| Wrong ability granted | guard grants `GA_HU_*`, not the goblin set |
| Skeleton mismatch (the `PlayAnimMontage` gate) | montage and mesh both `SK_Human_Skeleton`; 0 `REFUSED` lines in a live fight |
| Missing slot node | `ABP_Human` has `Slot 'DefaultSlot'`, connected |
| Wrong slot name | montages author into `DefaultSlot`, which is the node present |
| Empty montage / null segment | segment resolves to a real sequence, duration 1.15 |
| No animation data | `leftarm` and `rightarm` rotate; `spine` translates 84uu over the swing |
| Bone-name mismatch | attack and locomotion clips animate the IDENTICAL 43 bone names - 0 differences |
| Additive-type mistake | every clip, human and goblin, reports `additive=None` |
| LOD / anim tick / distance | identical settings to the working player goblin; Michael confirms distance-independent |
| Montage failing to start at runtime | `PlayAnimMontage` returned length 1.600 for the hit-react, so montages DO start |

## AnimGraph topology (read from the graph, connections included)

```
Locomotion ──► Slot 'DefaultSlot' ──► SaveCachedPose 'LocoPose'
UseCachedPose(LocoPose) ──────────────► LayeredBoneBlend.BasePose
UseCachedPose(LocoPose) ──► Slot 'UpperBody' ──► LayeredBoneBlend.BlendPoses_0   [BlendWeights_0 = 1.0]
LayeredBoneBlend ─────────────────────► Output Pose
```

**The one thing I could not read is `LayeredBoneBlend`'s LayerSetup** - the per-bone branch filter,
including the root bone each layer starts from, and whether `bMeshSpaceRotationBlend` is set. That
node is the only remaining element on the path between a montage that demonstrably plays and an
output pose that is demonstrably the reference pose. It is where I would look next, specifically:
does its branch filter name a bone that exists on this rig? This skeleton uses Mixamo naming
(`spine1`, not `spine_01`), and a filter authored against UE-standard names would find nothing.

**Fastest human check, ~10 seconds, worth doing before any of that:** double-click
`AM_HU_Atk_Light` in the Content Browser and scrub it. The asset preview plays the montage on the
human mesh with no AnimGraph involved. If it animates there, the assets are exonerated and the fault
is the graph (LayeredBoneBlend above). If it T-poses there, something in the montage/sequence pairing
is wrong in a way that eight separate property reads all failed to expose, and the next step is a
fresh re-import rather than more inspection.

**One unexplained observation worth keeping:** the human copies animate Mixamo-style bone names
(`hips, spine, spine1, leftshoulder, leftarm`, 43 bones) while the goblin originals animate
`root, hip, pelvis, l_thigh` (41 bones), and `SK_Human_Skeleton` has 72 bones. So the human
sequences are not retargets OF the goblin animations - they are the original Mixamo clips bound to
the human skeleton. That is probably correct and healthy, but it means "retargeted set" in #112/#113
describes something different from what is actually on disk.

**Separate and independently actionable:** `GS.Combat.Debug` should default to 0. That needs no
observation from anyone and is a one-line change to `GSGA_SwordLight.cpp:25`. Left out of this
ticket only because it was not claimed here.
