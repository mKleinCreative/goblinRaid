---
id: 412
title: First ACF AnimBP: ACF_Template_ABP child on the conformed human skeleton
agent: claude-acf-animbp
status: review
claimed: 2026-09-10T18:27Z
build: none
waiting_on:
evaluated: 2026-09-10T18:29:26Z
observed: 2026-09-10T18:29:27Z | Ran PIE and read the live anim instance off Erika: she is running ABP_GS_Human_ACF_C, it tests True as an ACFAnimInstance, and ACF is computing her state from the character - speed 250, normalized speed 0.385, direction 0, is_moving true, is_in_air false. Both GS AnimBPs returned False for that test before today
scenario: Live PIE on L_Tutorial_Island with BP_ErikaArcher repointed at the conformed SK_Human_Manny mesh and the new ACF template AnimBP
files: 
  - none-content-only
---

## Goal

First ACF AnimBP on the conformed rig - the step the whole animation spine was blocked on.

## Generate

- **`/Game/Characters/ACFRigs/Human/ABP_GS_Human_ACF`** - an AnimBlueprint created via
  `AnimBlueprintFactory` with `target_skeleton = SK_Human_Manny_Skeleton` and `parent_class` set to
  `ACF_Template_ABP`'s generated class. **`ACF_Template_ABP`, not `ACF_SimpleTemplate_ABP`** - the
  warping template is valid now that the conformed rig carries `pelvis` and the seven IK bones.
- **`BP_ErikaArcher`** repointed: mesh `SK_ErikaArcher_baked` -> `SK_Human_Manny`, anim class
  `ABP_Human_C` -> `ABP_GS_Human_ACF_C`. Erika rather than a castle guard because the mesh that was
  conformed is hers - pointing a guard at it would have given him her body.

## Evaluate

**`is ACFAnimInstance: True`** on the generated class, and again on the live instance in PIE. Before
today both GS AnimBPs returned False, which is what kept ACF's equipment-to-pose chain dead.

**Verified in a live PIE session, not by reading the asset:**

```
BP_ErikaArcher_C_3   mesh = SK_Human_Manny
  anim instance      : ABP_GS_Human_ACF_C
  is ACFAnimInstance : True
  speed              : 250.0
  normalized_speed   : 0.385
  direction          : -0.0
  is_moving          : True
  is_in_air          : False
```

ACF's own state is being computed from the character - `Speed`, `NormalizedSpeed` (250/650 = 0.385,
consistent with the max walk speed), `Direction`, and the movement flags. This is the
BlueprintReadOnly state `gs-anim-rig-compatibility` names as "another reason to derive from it", and
it is the foundation every ACF moveset, overlay and combo layer reads.

**NOT DONE, and nobody should read this as "animation works":** the AnimBP has **no moveset and no
overlay assigned**. `ACF_Template_ABP` supplies the structure - the `UACFAnimInstance` base, the
locomotion state machine, the layer interfaces - not the animations. Erika will be posed by whatever
the empty template resolves to until a moveset is authored against the new skeleton. Nobody has
looked at her on screen yet.

**Also untouched:** the other five human characters still point at `SK_Human_Skeleton` and
`ABP_Human_C`, and the goblins are entirely unconverted. Only Erika moved.

## Refine

Next: author a moveset and an overlay implementing ACF's `ACF_Moveset_ALI` / `ACF_Overlay_ALI`
against `SK_Human_Manny_Skeleton`, which is where animations finally have to exist on the new rig -
either retargeted from FullSample's ~1000 sequences on `ACF_UE5Manny`, or from the CombatMasterBundle
`_RM` set. That is the point at which the equipment-to-pose chain can actually be seen to grip a
weapon, and where the backwards bow gets answered.
