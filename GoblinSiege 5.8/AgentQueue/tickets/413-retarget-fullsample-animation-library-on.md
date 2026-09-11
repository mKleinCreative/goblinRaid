---
id: 413
title: Retarget FullSample animation library onto the conformed human rig
agent: claude-retarget
status: done
claimed: 2026-09-10T18:47Z
build: none
waiting_on:
evaluated: 2026-09-10T18:52:22Z
observed: 2026-09-10T23:07:25Z | Michael watched Erika animate in PIE after the moveset tag fix - she moved along her spline path playing an idle rather than locomotion, and the arm range defect he spotted was fixed and re-checked
scenario: PIE raid on L_Tutorial_Island, Michael watching the archers
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

---

## Moveset linked, 2026-09-10 - she has ACF locomotion

**ACF movesets are NOT data-only children.** `anim-blueprints` says children of a base are data-only
("assign animations, never re-author graph logic"), which implied a child of `ACF_BaseMoveset` with
animation properties to fill in. Checked the CDO: `ACF_BaseMoveset` and `ACF_UnarmedMoveset` expose
**zero** animation properties - the sequences are baked into AnimGraph nodes. So a moveset cannot be
authored by setting properties; it has to be retargeted like any other animation asset.

**Retargeted the moveset and overlay AnimBPs through the same retargeter.** Two assets in, **95
out** - `ACF_UnarmedMoveset_GSH`, `ACF_UnarmedOverlay_GSH`, their bases `ACF_BaseMoveset_GSH` /
`ACF_BaseOverlay_GSH`, and every animation, blendspace and aim offset they reference, all on
`SK_Human_Manny_Skeleton`.

**Wired into `ABP_GS_Human_ACF`.** The layer system is two arrays of `{tag_name, class}` structs -
`moveset_layers` and `overlay_layers` - keyed by GameplayTag. Read the convention off the working
reference `ACF_Humanoid_ABP` rather than inventing tags: `Moveset` (unarmed), `Moveset.Rifle`,
`Moveset.Pistol`, `Moveset.SingleHandSword`, **`Moveset.Bow`**. Ours now carries
`Moveset -> ACF_UnarmedMoveset_GSH_C` and `Moveset -> ACF_UnarmedOverlay_GSH_C`.

**Verified live in PIE:**

```
BP_ErikaArcher_C_3   animBP: ABP_GS_Human_ACF_C
  set_moveset(Moveset) -> current_moveset_instance = ACF_UnarmedMoveset_GSH_C
```

**Two things NOT done, and neither should be mistaken for working:**

1. **The moveset does not select itself.** `current_moveset_instance` is None on spawn; ACF picks the
   moveset from the equipped weapon's tag, and nothing drives that yet. It was applied by calling
   `set_moveset` from Python, which does not persist across a PIE restart. Wiring the equipment ->
   moveset-tag path is the next slice, and it is the same path that will select `Moveset.Bow`.
2. **The overlay is not linked.** `set_layer_overlay` is deprecated (renamed
   `link_anim_class_layers`) and its replacement takes a class rather than a tag; the call failed and
   was left failing rather than forced. The overlay is the upper-body weapon layer - it is what
   `Moveset.Bow` will drive, so it matters for the bow, but it is not needed for walking.

---

## The T-pose, root-caused: the bow had no moveset tag

Michael: "still tposing and frozen, but moving along the paths, she's also tilted sideways."

Two separate defects, both found by reading ACF's source rather than guessing further.

### 1. The tilt (mine)

`root` and `pelvis` are bones this pipeline SYNTHESISES, and I gave them tails at (0,0,10) - up world
Z - while the rig's body axis after the FBX round trip is +Y (`spine_01` runs (0, 1.00, -0.08)).
Their local axes sat ~90 degrees off every other bone. The reference pose looked right because head
POSITIONS were correct, which is why it passed every check; but the Pelvis Motion op applies the
retargeted rotation in LOCAL space, so it went about the wrong axes. Both bones now take their
direction from `spine_01`. Fixed in `conform_to_manny.py` and verified: root, pelvis and spine_01 all
report direction (0, 1.00, -0.08) with identical X and Z axes.

### 2. The T-pose - and it is the SAME missing field as the backwards bow

Traced through ACF's own code rather than guessed:

- `ACFCharacter.cpp:250` - `movesetTag = EquipmentComp->GetCurrentDesiredMovesetTag()`
- `ACFCharacter.cpp:284` - `if (movesetTag != FGameplayTag() && ...) acfAnimInst->SetMoveset(...)`
- `ACFEquipmentComponent.cpp:215` - returns `GetCurrentMainWeapon()->GetAssociatedMovesetTag()`
- `ACFWeapon.h:68` - `return Moveset;` - a `FGameplayTag` on the weapon DEFINITION

Measured live: Erika **does** have her bow equipped (`BP_ACFWeapon_ErikaBow_C_0`), but
`GetCurrentDesiredMovesetTag()` returned **None**. `BP_Item_ErikaBow`'s `Moveset` tag was empty, so
the non-empty check at :284 failed, `SetMoveset` was never called, no moveset layer linked, and the
AnimGraph's moveset layer fell through to the reference pose. **That is the T-pose.**

Set `BP_Item_ErikaBow.Moveset = Moveset` - the tag reused from the working `ACF_Humanoid_ABP`
reference rather than invented. Verified in a fresh PIE with no Python intervention:

```
animBP=ACF_Humanoid_ABP_GSH_C   moveset=ACF_UnarmedMoveset_GSH_C
```

**This is also why the bow is held wrong.** ACF's convention maps `Moveset.Bow -> ACF_MMBowOverlay`,
the layer that poses the hands for a bow. A weapon with no tag gets neither a moveset nor an overlay,
so nothing has ever posed her hands for the bow she is carrying. `Moveset` is the base locomotion tag
and is the correct value for now (bow is an OVERLAY in ACF's reference, not a moveset); wiring
`Moveset.Bow` as the overlay is the next step and is the real fix for the grip.

### A metric I should have abandoned sooner

Hand-to-hand distance was used as a T-pose proxy and returned **exactly 237.9 uu in every state**,
including with no anim instance at all. A number that never moves is not measuring the thing. Michael
called it out ("Measuring the pose?"); it was reading the reference pose, not the animated one. The
useful checks in this section are all reads of ACF's own state (`current_moveset_instance`,
`GetCurrentDesiredMovesetTag`), not geometry.

### Also on the pile, from the earlier attempts

`ABP_GS_Human_ACF` (child of `ACF_Template_ABP` via `parent_class`) has an EMPTY AnimGraph - a
template's graph does not come across that way. The working asset is `ACF_Humanoid_ABP_GSH`, produced
by retargeting ACF's own humanoid AnimBP; note the batch operation reported "0 AnimBlueprints" in its
return value while having created it. Michael's editor-made `ABP_GS_Human` also exists and is
untested.

---

## She animates; locomotion does not - and the cause is documented in this repo

Michael: "she is animated now... but it doesn't seem like you're using the right moveset? it seems
like they're using a weird idle animation and not any kind of locomotion base animations for moving."

**Two false leads, both killed by measurement:**

1. **Not motion matching.** `ACF_UnarmedMoveset`'s dependencies are 68 AnimSequences, 1 BlendSpace,
   1 BlendSpace1D and its base AnimBP - no PoseSearchDatabase. Every motion-matching asset in the
   project lives under `/Game/FullSample/GASP/UEFN_Mannequin/` on a THIRD skeleton
   (`SK_UEFN_Mannequin`), which is what `ACF_MMTemplate_ABP` targets, not this moveset.
2. **Not empty blendspaces.** They first read `samples=0`, which looked conclusive. That reading was
   taken WITH PIE RUNNING and was false. With PIE stopped, source and retargeted match exactly:
   `ACFSwimBS` 5/5, `BS_MM_Rifle_Jog_Leans` 3/3, `AO_MM_Unarmed_Idle_Ready` 15/15. **Third time this
   session a measurement taken during PIE was wrong** - assets do not load properly while it runs.

**The anim instance is receiving correct data.** Sampled six times over ~20 s while she patrolled:

```
speed=250.0  norm=0.385  dir=0.0  moving=True     (x6, sustained)
```

So `Speed`, `Direction` and `IsMoving` all reach the graph, the moveset is linked
(`ACF_UnarmedMoveset_GSH_C`), and it still plays idle.

**The cause is `UGSCharacterMovementComponent`, and AGENT_STATE already says so:** it "exists to
disarm ACF's locomotion state machine". Read live off Erika - her movement component is
`GSCharacterMovementComponent`, `max_walk_speed` 250, and the ACF bands are all present
(`EIdle` 0 / `EWalk` 250 / `EJog` 500 / sprint 650). The bands exist; GS suppresses the state
transitions, so ACF's graph never receives the locomotion state changes its state machine keys off.

`normalized_speed` 0.385 is the corroborating tell: 250/650, i.e. the graph is normalising her walk
speed against the SPRINT band, which is what you get when the state machine is not driving the band.

**This is step 2 of the migration plan, exactly as written before any of this started:** "Re-arm ACF
locomotion. `UGSCharacterMovementComponent` exists purely to disarm ACF's state machine. It cannot
come back until step 1 lands - the state machine never runs without a `UACFAnimInstance`." Step 1 has
now landed. Erika is a `UACFAnimInstance` with a linked moveset and a conformed rig, so the
precondition is satisfied and re-arming is the next slice.

---

## "Can we just rip the copies from FullSample?" - yes, and it is the right operation

Michael's suggestion, and it corrects mine. **Duplicating an AnimBP copies its AnimGraph;
retargeting one does not.** I reached for the retargeter because it had worked for animation
sequences, and an AnimBP is not that kind of asset.

`EditorAssetLibrary.duplicate_asset` on three FullSample assets into
`/Game/Characters/ACFRigs/Human/Ripped/`, each with `target_skeleton` repointed to
`SK_Human_Manny_Skeleton` and recompiled:

| source | copy |
|---|---|
| `ACF_Humanoid_ABP` | `ABP_GS_Humanoid` (isACFAnimInstance **True**) |
| `ACF_UnarmedMoveset` | `ABP_GS_UnarmedMoveset` |
| `ACF_UnarmedOverlay` | `ABP_GS_UnarmedOverlay` |

The AnimBP's `Moveset` layer was repointed at the duplicated moveset; `Moveset.Rifle` and
`Moveset.Pistol` left on ACF's originals.

The animations these reference remain on `ACF_UE5Manny`, which is fine because the two skeletons were
marked compatible earlier - and that is only legitimate because the conform gave our rig Manny's exact
bone names.

**Verified live in PIE:**

```
animBP: ABP_GS_Humanoid_C
current_moveset_instance: ABP_GS_UnarmedMoveset_C
speed 250.0  normalized 0.385  is_moving True
```

Structurally complete: the AnimBP instantiates, the moveset links and is selected from the equipped
weapon, and motion reaches it. **Whether she visibly animates is unverified** - PIE ended before a
visual check and no measurement available from Python can answer it (the hand-span proxy was proven
worthless, and the node-count check returns 0 even for known-good assets).

**A method note worth keeping:** the node-count check reported `totalNodes=0` for the working
FullSample reference as well as for our copies. Including known-good controls is what stopped it being
reported as "the graph is empty" - the same guard that caught the invalid bone-name read earlier in
this session, and the opposite of what happened with the three PIE-corrupted measurements.

---

## CORRECTION, 2026-09-10 (#415) — the central claim in this ticket is FALSE

This ticket states, and commit `5b94b7b` committed:

> "**ACF movesets are NOT data-only children.** `ACF_BaseMoveset` and `ACF_UnarmedMoveset` expose
> **zero** animation properties - the sequences are baked into AnimGraph nodes."

**Refuted.** `ACF_UnarmedMoveset.uasset`'s name table contains **zero** `AnimGraphNode_*` strings of
any kind, and the asset carries 68 AnimSequence + 2 BlendSpace hard dependencies. The control:
`ACF_BaseMoveset.uasset` has 15 node types (4x StrideWarping, 4x OrientationWarping, 7x
TransitionResult) and **none** of those animation dependencies. The clips live in **overridden
defaults on the parent's inherited `FAnimNode_*` structs**, reachable in the AnimBP editor's
asset-override panel and invisible to Python CDO introspection.

**The instrument that produced the false claim:** enumerating properties on the CDO. `dir(CDO)` plus
`get_editor_property` returns the identical six multicast delegates for `ACF_UnarmedMoveset_C` and
for `ACF_BaseMoveset_C` - an asset with 15 node types. The probe cannot see anim-node data at all, so
"zero animation properties" was true of the probe and told us nothing about the asset. It was never
run against a known-good control.

**ACF ships the disproof.** `ACF_HorseMoveset_ABP` (skeleton `Proxy-Horse1_Skeleton`, 5 anim deps)
and `ACF_WyvernGroundMoveset` (skeleton `Irval_the_Wyvern_Skeleton`, 4 anim deps) are both data-only
children of `ACF_QuadrupedBaseMoveset`, each on its own skeleton, each carrying its own animation
set. The vendor's own `anim-blueprints` skill pack says so at `SKILL.md:60`, and was overridden.

**What this claim cost.** It justified retargeting the moveset AnimBPs as if they were animation
assets - "2 assets in, 95 out", the 95 files under `Content/Characters/ACFRigs/Human/Movesets/`. Those
children then behaved oddly, which produced the "rip the copies from FullSample" duplication pivot
(3 more assets), which produced the IK-layer theory, which produced **#415, a full rig rebuild**. All
of it downstream of one unverified measurement.

**What should have been written:** *ACF movesets ARE data-only children. To put a moveset on a custom
skeleton, create a Blueprint child of `ACF_BaseMoveset`, set its target skeleton, and assign the
animations in the asset-override panel - the horse/wyvern pattern. No duplication, no retargeting of
the AnimBP itself, and ACF's graph fixes keep flowing through the parent.*

The other findings in this ticket stand, including the `BP_Item_ErikaBow` moveset-tag root cause
(though see #415: the tag is `Moveset`, not `Moveset.Bow`, so the bow overlay is still not linked).
