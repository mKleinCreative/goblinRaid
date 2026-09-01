---
name: gs-anim-rig-compatibility
description: Goblin Siege — bone-name pre-checks before adopting any ACF animation template, why force_root_lock flattened the human walk, and the blind spot in GS.Anim.Snapshot.
globs: []
alwaysApply: false
---

# GS — our rigs are not Manny, and the failures are silent

Both Goblin Siege skeletons fail ACF's and the engine's bone-name assumptions, and **every one of these
failures is silent**: no log, no compile error, an animating character, and a symptom three steps
removed from the cause ("stiff", "sliding", "wrong pose"). Read this before reparenting an ABP,
ticking a root-lock box, or trusting `GS.Anim.Snapshot`.

The bone lists, read from the assets' name tables:

| Skeleton | Bone 0 | Spine / pelvis | Limbs | IK bones |
|---|---|---|---|---|
| `Content/Characters/Humans/SK_Human_Skeleton.uasset` (Mixamo) | **`Hips`** | `Spine`, `Spine1`, `Spine2`, `Neck`, `Head` | `LeftUpLeg`, `LeftLeg`, `LeftFoot`, `LeftToeBase`, `LeftShoulder`, `LeftArm`, `LeftForeArm`, `LeftHand` (+ mirror) | **none** — no `root`, no `pelvis`, no `ik_*` |
| `Content/Characters/ScoutV3/GOB_Scout_v3_baked_Skeleton.uasset` | **`Root`** | `Pelvis`, `Spine01`, `Spine02` | `L_Thigh`, `L_Calf`, `L_Foot`, `L_ToeBase`, `L_Clavicle`, `L_Upperarm`, `L_Forearm`, `L_Hand` (+ twists, + mirror) | **none** — no `ik_foot_root`, no `foot_l`, no `thigh_l`; `Spine01` ≠ `spine_01` |
| ACF's `ACF_UE5Manny` (what everything ACF ships is authored against) | `root` | `pelvis`, `spine_01..05` | `thigh_l/r`, `calf_l/r`, `foot_l/r`, `upperarm_l/r`, `lowerarm_l/r`, `hand_l/r` | `ik_foot_root`, `ik_foot_l/r`, `ik_hand_root`, `ik_hand_l/r` |

---

## 1 — Pick the Simple template, not the Lyra one — and here is the test

`ACF_BaseMoveset` (the full-body base for `ACF_Template_ABP`) is built out of warping nodes: a node dump
of `.../AscentCombatFramework/Content/CharacterController/ACF_BaseMoveset.uasset` gives **5 ×
`AnimGraphNode_StrideWarping` and 5 × `AnimGraphNode_OrientationWarping`** (93 / 85 runtime-struct name
hits), and its name table carries exactly one set of bone refs: `pelvis`, `ik_foot_root`, `ik_foot_l/r`,
`foot_l/r`, `thigh_l/r`, `ik_hand_root`, `spine_01..spine_05`.

`FAnimNode_StrideWarping::IsValidToEvaluate`
(`Engine/Plugins/Animation/AnimationWarping/Source/Runtime/Private/BoneControllers/AnimNode_StrideWarping.cpp:424-458`)
returns **false with no log at all** if `PelvisBone` or `IKFootRootBone` are not in the bone container,
or if any foot's IK / FK / Thigh index is `INDEX_NONE`.

By contrast `.../CharacterController/Simple/ACF_SimpleMoveset.uasset` dumps as 61 BlendSpacePlayer +
37 SequencePlayer + 13 SequenceEvaluator, with **zero bone references and zero warping nodes**. ACF's
own sample enemy takes that family: `Blueprints/Actors/ACF_Enemy_BP.uasset` references `SKM_Manny` and
`ACF_SimpleTemplate_ABP_C`.

**Trap:** set `ABP_Human`'s parent to `ACF_Template_ABP`, assign an `ACF_BaseMoveset` child, and the
editor compiles clean and the character animates. What you lose is stride scaled to ground speed and
lower-body strafe orientation — uniform foot-sliding at every speed, which reads as a blendspace-tuning
problem and can eat a session.

**Pre-check, one line, before adopting any ACF anim template:** does the target skeleton contain
`pelvis` AND `ik_foot_root` AND (`ik_foot_l`, `foot_l`, `thigh_l`)? Per the table above, neither GS rig
does. Take `ACF_SimpleTemplate_ABP` + `ACF_SimpleMoveset`.

(Both warping nodes are also **graph-driven** — they read the root-motion delta attribute,
`AnimNode_StrideWarping.cpp:117`, `:199-210` — which only exists when the sequence has
`bEnableRootMotion` (`AnimSequence.cpp:1704-1713`, gated on `HasRootMotion()`, literally
`return bEnableRootMotion`, `AnimSequence.h:407`). See `gs-anim-adoption-gaps` §1.)

**NOT VERIFIED:** FullExample's own Content has no character content (1616 uassets: StylizedIsland + 4
Mannequin meshes) and `ACF_Enemy_BP`'s `/Game/FullSample/...` references dangle in this install, so the
GASP / Motion-Matching half of `anim-blueprints` could not be checked.

---

## 2 — `force_root_lock` on a rootless rig overwrites the Hips

Engine mechanism, not ACF, and nothing in the 40 packs mentions it.
`FRootMotionReset::ResetRootBoneForRootMotion`
(`Engine/Source/Runtime/Engine/Public/Animation/AnimCompressionTypes.h:878-893`) replaces the **entire
transform** of the bone it is given with the ref pose (default `ERootMotionRootLock::RefPose`) —
translation, rotation *and* scale, not just travel. It is called on
`OutPose[FCompactPoseBoneIndex(0)]` — **bone index 0, by position, with no name check** —
(`AnimationDecompression.cpp:274-277`, `AnimSequence.cpp:1862-1864`), and the condition is
`(bExtractRootMotion && bEnableRootMotion) || bForceRootLock`, so the flag fires regardless of
root-motion mode.

On Manny that costs travel and nothing else (bone 0 is `root`, the pelvis is separate). ACF's own clips
never need it: `ThirdPersonWalk.uasset`, `ThirdPersonRun.uasset` and `Idle.uasset`
(`.../AscentCombatFramework/Content/Mannequin/Animations/`) each serialize `bEnableRootMotion` and none
serializes `bForceRootLock`.

On `SK_Human_Skeleton`, bone 0 is **`Hips`** — the bone that carries the pose. Measured in
`AgentQueue/tickets/333-human-locomotion-attacks-move-onto-comba.md:38-44`:

| Clip set | Hips Z-span |
|---|---|
| 8 locked travelling clips (`A_HU_Std_WalkF.uasset` serializes both `bEnableRootMotion` and `bForceRootLock`) | **0.00** |
| never-locked idle (`A_HU_Std_Idle.uasset`, `bEnableRootMotion` only) | **6.76uu** |

plus 223uu of travel baked into the hips on WalkF. The bob, the sway and the hip rotation were deleted
along with the travel, and the report that reached Michael was "a little stiff". The fix taken
(same ticket, `:47-56`) was to abandon those clips for CombatMasterBundle clips retargeted with the
retargeter's Root Motion op disabled and Pelvis Motion `ScaleHorizontal = 0`.

**The measurement that separates the two cases before you touch anything:** bone-0 name, plus bone-0
(and pelvis) Z-span across the clip. A rig correct for ACF has bone 0 named `root`, travel on it, and a
non-zero pelvis Z-span. A broken one has bone 0 carrying both travel and bob, and a zero Z-span the
moment it is locked.

---

## 3 — `GS.Anim.Snapshot` is blind at exactly the bone these rigs break on

The project's only automated animation check skips bone 0 by design:

```
// Source/GoblinSiege/Combat/GSAnimDebugCommands.cpp:111-112
// Start at 1: bone 0 is the root, which a great many clips legitimately leave at identity.
for (int32 i = 1; i < Live.Num(); ++i)
```

Its REFPOSE column exists to catch "a pawn STANDING STILL IN THE WRONG POSE" (file header, `:16-20`),
and it compares **rotation only** (`:86-89`: "the human clips animate translation on the Hips alone").
But bone 0 is `Hips` on the human — the bone that carries the pose — and `Root` on the goblin — the bone
that carries the travel.

So a re-introduced `force_root_lock`, a retargeter whose Root Motion op zeroes the target root, or a
clip imported without its hip track all express themselves at bone 0 **and only bone 0**, and all of
them read `REFPOSE = no` and pass. The instrument built after #137 to stop animation defects being found
only by a human looking at the screen is blind in the exact direction this rig fails.

**Two extra columns close it and cost nothing:** the *name* of bone 0, and the Z-span of bone 0 (and of
`Hips` / `Pelvis`) over the last N frames. A live walk with a bone-0 Z-span of 0.00 is a locked root,
stated as a number instead of as "stiff". Run any pre-check from §1 or §2 through this instrument, not
by eye.

(ACF needs no equivalent because its own state is readable off `UACFAnimInstance`: `Speed` /
`NormalizedSpeed` from `Speed / GetCharacterMaxSpeed()` at `ACFAnimInstance.cpp:281`, `Direction` from
`CalculateDirection` at `:277`, all BlueprintReadOnly — which is another reason to derive from it.)
