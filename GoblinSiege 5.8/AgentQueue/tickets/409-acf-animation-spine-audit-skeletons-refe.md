---
id: 409
title: ACF animation spine: audit skeletons, references and AnimBP parents before migrating
agent: claude-anim-spine
status: review
claimed: 2026-09-10T17:00Z
build: none
waiting_on:
evaluated: 2026-09-10T17:06:28Z
observed:
scenario:
files: 
  - none-investigation-only
---

## Goal

Michael's ruling of 2026-09-10: ACF is the system, GS code is the exception. First slice is the
animation spine, because it is the dependency root AND the cause of the visible bow/grip bugs.

Audit before migrating. No assets changed.

## Generate

Measurement only. Four things established, each of which changes the plan.

### 1. Root motion exists. I was wrong, and Michael caught it.

I claimed ACF montages "expect root motion we deliberately stripped", repeated from
`gs-anim-adoption-gaps` without opening an asset. Measured:

- **25 `_RM` sequences** in `/Game/CombatMasterBundle/Animations/DynamicAxe/Manny_UE5/RootMotion/`.
- All 25 have **`enable_root_motion = False`**.
- The root track **genuinely carries motion**: `Anim_DA_CommonAttack_A_RM` moves the root
  **364.56 uu** over 1.60 s; `Anim_DA_Combo_A1_RM` 94.56 uu; `Anim_DA_Idle_A_RM` 2.44 uu (correctly
  static).

The data was never removed. A flag was. See the AGENT_STATE correction.

### 2. But the `_RM` set is on a THIRD skeleton, so it is source material, not drop-in

| | skeleton |
|---|---|
| the 25 `_RM` sequences | **SKEL_Manny_DA** |
| goblins (`BP_GSPlayerCharacter`, `BP_HordeGoblin`) | GOB_Scout_v2_Skeleton |
| humans (guards, archer, knight, peasant) | SK_Human_Skeleton |

No GS character can play them directly. Flipping `enable_root_motion` on them alone achieves
nothing until they are retargeted, or until equivalent motion exists on the GS rigs.

### 3. Neither GS AnimBP is a `UACFAnimInstance` - confirmed by runtime type test, not by reading

`ABP_Human_C` and `ThirdPerson_AnimBP_Gob_C` both return `isACFAnimInstance = False`. This is the
spine gap, and it is why ACF's equipment-to-pose chain never runs - the backwards bow and the odd
grip are downstream of exactly this.

### 4. THE KEY FINDING: ACF's templates are SKELETON-AGNOSTIC

`ACF_Template_ABP`, `ACF_SimpleTemplate_ABP`, `ACF_BaseMoveset`, `ACF_BaseOverlay`,
`ACF_BaseBranchOverlay`, `ACF_MaskOverlay`, `ACF_QuadrupedBaseMoveset` and the `ACF_*_ALI` layer
interfaces (`Moveset`, `Overlay`, `Climbing`, `Riding`, `IK`) all report **`skel=None`** - they are
Animation Blueprint Templates plus Animation Layer Interfaces. Only the concrete instances
(`ACF_Humanoid_ABP`, `ACF_UnarmedMoveset`, `ACF_UnarmedOverlay`, `ACF_IKLayer_ABP`) bind to
`ACF_UE5Manny`.

**So adopting ACF's animation architecture does NOT require retargeting its ~1000-sequence library
onto the goblin and human rigs.** The templates supply the structure - the `UACFAnimInstance` base,
the locomotion state machine, the moveset/overlay layering driven by equipment - and GS supplies
per-skeleton animations through the layer interfaces.

What GS already has to feed them: **177 sequences and 55 montages on GOB_Scout_v2_Skeleton**, and
17 montages on SK_Human_Skeleton.

## Evaluate

Every claim above is a measurement. The two that matter most were both things I had previously
asserted from prose:

- "we have no root motion" - **false**, 25 sequences with up to 364 uu of root travel.
- "no GS AnimBP is a UACFAnimInstance" - **true**, confirmed by `isinstance` against
  `unreal.ACFAnimInstance` rather than by reading a header.

**Not established, and needed before authoring:**
- Whether `SK_Human_Skeleton` and `GOB_Scout_v2_Skeleton` bone names satisfy ACF's template
  expectations. `gs-anim-rig-compatibility` exists for exactly this pre-check and has not been run.
- Whether the 55 goblin / 17 human montages are shaped the way ACF's Actions System expects
  (notify windows, sections), or only the way GS's own ability code expects.
- Which of the 25 `_RM` animations have a GS-rig equivalent already, versus needing retarget.

## Refine

Proposed order, smallest risk first, nothing authored yet:

1. Run the `gs-anim-rig-compatibility` bone pre-check on both GS skeletons against ACF's template
   requirements. It is cheap and it gates everything else.
2. Build ONE `UACFAnimInstance`-derived AnimBP from `ACF_Template_ABP` for the human rig, with a
   single moveset and overlay, on ONE test character. Prove the equipment-to-pose chain drives a
   weapon grip correctly before touching the player or the goblins.
3. Only then migrate the rest, and only then revisit root motion - it is a per-asset boolean once
   the motion is on the right rig, and it will change spacing and AI engagement distances when it
   lands.
