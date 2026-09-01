---
name: gs-anim-adoption-gaps
description: Goblin Siege — every ACF montage type needs root motion we deliberately removed, no GS AnimBP is a UACFAnimInstance so the equipment-to-pose chain is dead, there is no aim offset, and baked retargets have no provenance.
globs: []
alwaysApply: false
---

# GS — what ACF's animation side is already doing for us, and getting back nothing

`AGSCharacterBase` has been an `AACFCharacter` since #223 (`GSCharacterBase.h:47`), so ACF's animation
calls all fire — into nothing. This pack is the list of what is silently inert and what it would cost to
turn on. Rig-level bone compatibility is a separate pack (`gs-anim-rig-compatibility`).

---

## 1 — Every `EMontageReproductionType` is a root-motion type

`ActionsSystem/Public/ACFActionTypes.h:48-53` — the enum has exactly three values: **`ERootMotion`,
`ERootMotionScaled`, `EMotionWarped`** ("Root Motion Warped"). There is **no in-place option**. (The one
other value the codebase mentions, `ECurveOverrideSpeed`, survives only inside commented-out lines of a
dead stub: `ACFOverrideSpeedNotifyState.cpp:19-21` and `:27-29`, both notify bodies entirely commented
out.) `actions-system:52` lists a `Normal` value that does not exist — see its addendum.

`FActionConfig` defaults to `ERootMotion` (`ACFActionTypes.h:478`), and every shipped combat action
upgrades that to warped root motion in its constructor: `ACFAttackAction.cpp:25`,
`ACFDirectionalDodgeAction.cpp:19`, `ACFHitReactionChooserAction.cpp:16`,
`ACFInteractActionAbility.cpp:14`, `ACFAdvancedHitAction.cpp:20`.

Motion Warping is implemented **only as an edit to the root-motion delta**: `UMotionWarpingComponent`
binds `WarpLocalRootMotionDelegate`
(`Engine/Plugins/Animation/MotionWarping/Source/MotionWarping/Private/MotionWarpingComponent.cpp:321`)
and applies modifiers in `ProcessRootMotionPreConvertToWorld` (`:537-561`). No root motion in the
montage ⇒ that delegate carries nothing, so the warp target is registered
(`ACFGameplayAbility.cpp:415-433`) and later removed (`:240-242`) having moved the character **zero
units**, with no log.

**And Goblin Siege deliberately stripped root motion from its attack clips.**
`AgentQueue/tickets/128-attacking-freezes-the-player-the-goblin.md:37-67` records turning root motion
off on the goblin attack montages because "a montage carrying root motion makes the character
root-motion-driven for its duration", closing with "with root motion off, an attack contributes no
animation-driven step".

So the obvious migration — reparent `GSGA_SwordLight` / `GSGA_DodgeRoll` onto `UACFAttackAction` /
`UACFDirectionalDodgeAction` to pick up combos, hit reactions and target warping — silently inherits
`EMotionWarped` against montages made root-motion-free. The montage plays, the damage window fires, the
warp target is set and torn down, and the character does not translate one unit toward the enemy.
Symptom reads as a targeting or range bug. **If GS adopts ACF's actions, the root motion #128 removed has
to come back, and the movement fight #128 was solving has to be re-solved with `ERootMotionScaled` /
`RootMotionScale` (`ACFActionTypes.h:319-320`, `ACFGameplayAbility.cpp:260-264`) instead.**

ACF ships the warping notify as content: `.../Content/Blueprints/Animation/ACF_WarpToTargetNotify_AN.uasset`.
**NOT VERIFIED:** none of ACF's own attack montages are present in this FullExample install (its plugin
ships 20 anim assets, all Manny locomotion / aim offsets), so no shipped montage's root-motion flag could
be read.

---

## 2 — Nothing here is a `UACFAnimInstance`, and the cast is silent

`anim-blueprints` and `character-controller` already state the *rule* (derive from `UACFAnimInstance`).
What they frame as an animation problem is in fact a whole-chain no-op, and Goblin Siege is currently in
violation:

- `Content/Characters/Humans/ABP_Human.uasset` — `NativeParentClass = /Script/Engine.AnimInstance`;
  references only `BS_GS_Locomotion_Hu`, `A_HU_Std_Idle`, `A_HU_Std_WalkF`, `A_HU_Std_RunF`.
- `Content/Characters/ScoutV2/Animations/ThirdPerson_AnimBP_Gob.uasset` — same parent; 121
  SequencePlayer + 25 BlendSpacePlayer + 13 StateMachine nodes, with **zero
  `AnimNode_LinkedAnimLayer` and zero `AnimNode_LinkedAnimGraph`**: no moveset, no overlay, no layer
  swapping of any kind.
- A binary grep across `Content` for `ACFAnimInstance`, `ACF_Template_ABP`, `ACF_BaseMoveset`,
  `ACF_BaseOverlay`, `ACF_SimpleTemplate` returns nothing.
- `GSAnimDebugCommands.cpp:10` states it outright: "There is no UAnimInstance subclass in the module."

The failure signature — this is the part worth memorising:

| Call | Guard | What you get |
|---|---|---|
| `AACFCharacter::GetACFAnimInstance()` = `Cast<UACFAnimInstance>(GetMesh()->GetAnimInstance())` | `ACFCharacter.cpp:501-504` | nullptr |
| combat-type update → `SetMoveset(movesetTag)` / `SetAnimationOverlay(overlayTag)` | `ACFCharacter.cpp:282-290`, wrapped in `if (acfAnimInst)` | **silent no-op** |
| `UACFAnimsetFragment::ApplyFragment_Implementation` | `ACFAnimsetFragment.cpp:27-32`, same failed cast | returns silently |
| `GetCurrentMoveset` | `ACFCharacter.cpp:488-499` | the **only** thing that logs — one Warning |

So every equip in the project already calls `SetMoveset` / `SetAnimationOverlay` into a null cast:
equipping a weapon changes nothing about the pose, and an `ACFAnimsetFragment` on a character data asset
applies nothing. Free climbing is lost the same way — `UACFAnimInstance` caches the
`UACFClimbingComponent` in `SetReferences` (`ACFAnimInstance.cpp:194`), sets `bIsClimbing` from
`ClimbingComponent->IsClimbing()` every thread-safe update (`:414-416`), and links `ACF_ClimbingLayer`
via `ActivateClimbingLayer` / `DeactivateClimbingLayer` (`:128-150`). GS hand-rolled climbing instead
(MOVE_Flying + `AddMovementInput` + a hand-written state machine,
`AgentQueue/tickets/067-climb-polish-stop-the-lip-jam-wire-the-4.md:30-70`) and GS Source contains no
reference to `UACFClimbingComponent` at all.

**Expected failure mode:** somebody concludes "ACF's moveset tags don't work" and re-implements
per-weapon stances as more states in the 13-state-machine goblin graph — a third copy of logic ACF owns.
The cheap correct move for these rigs is `ACF_SimpleTemplate_ABP` + `ACF_SimpleMoveset` (no bone refs,
no warping nodes — see `gs-anim-rig-compatibility` §1). ACF ships the layer bases and their interfaces
as content: `ACF_BaseMoveset`, `ACF_BaseOverlay`, `ACF_ClimbingLayer`, `ACF_Moveset_ALI`,
`ACF_Overlay_ALI`, `ACF_Climbing_ALI`, `Simple/ACF_SimpleMoveset` (all under
`.../AscentCombatFramework/Content/CharacterController/`).

See also `gs-locomotion-bands` §2: the same missing cast is what freezes the movement component's
locomotion classifier.

---

## 3 — There is no aim offset in this project

ACF already computes the value. `UACFAnimInstance::UpdateAimData` (`ACFAnimInstance.cpp:345-377`) takes
the delta of `GetBaseAimRotation()` against actor rotation, subtracts the turn-in-place `YawOffset`,
applies the **±135° wrap-flip** that stops the pose snapping when aim crosses behind (`:352-370`), then
`FInterpTo`s both axes at `AimOffsetInterpSpeed` (`ACFAnimInstance.h:235`, default 10). The result is
`FVector2D AimOffset`, BlueprintReadOnly (`ACFAnimInstance.h:407-408`), updated on the worker thread
every frame when `bUpdateAimData` (`:291`, default true; `cpp:232-234`). The header includes
`<Animation/AimOffsetBlendSpace1D.h>` (`cpp:20`).

ACF also ships the asset **shape** you must author per rig:
`.../AscentCombatFramework/Content/Mannequin/Animations/AO_Base.uasset` is an `AimOffsetBlendSpace` fed
by nine single-frame poses in the same folder — `AO_LU, AO_CU, AO_RU / AO_LC, AO_CC, AO_RC / AO_LD,
AO_CD, AO_RD`. A standard 3×3 grid; that is all.

Goblin Siege has none: a search of `Content` for `AO_*` or `*AimOffset*` returns nothing, and the only
blendspaces are `BS_GS_Locomotion_Hu`, `BS_GS_Locomotion_Gob`, `BS_GS_Loco_Pack`, `BS_GS_CrouchMove` —
all ground locomotion. Meanwhile the bow fires along the full control rotation:
`GSGA_BowShot.cpp:250` `const FRotator AimRotation = Avatar->GetControlRotation();`. And per §2 neither
ABP is a `UACFAnimInstance`, so `AimOffset` is not even being computed.

**Symptom:** arrows leave along the aim pitch while the archer's pose stays level — invisible on flat
ground, obvious the moment anyone shoots up at a wall or down from one, which is the premise of a siege.

**Second-order trap — the fix:** the natural move is to add a `Pitch` float to `ABP_Human`'s event graph
off `GetBaseAimRotation`. That creates a second, unsmoothed, wrap-naive source of truth that will
disagree with ACF's `AimOffset` during the interp and at the ±135° boundary the moment any ACF-derived
layer is adopted. Author the nine poses and **read `AimOffset` from the ACF anim instance** instead.

---

## 4 — Baked retargets have no provenance; ACF ships a live alternative

ACF ships `.../AscentCombatFramework/Content/Animation/ACF_UE5ToUE4Retargeted_ABP.uasset`, containing
`AnimNode_RetargetPoseFromMesh` and referencing `/AscentCombatFramework/Animation/RTG_UE5Manny_UE4Manny`
and `UE4_Mannequin_Skeleton` — the pose retargeted at runtime off a leader mesh instead of baked into
per-rig sequences. The supporting set ships with it: `RTG_UE4Manny_UE5Manny`, `RTG_UE5Manny_UE4Manny`,
`IK_Mannequin`, `IK_UE4_Mannequin`, `IK_ACF_UE5Manny`, `ACF_IKLayer_ABP`, `ACF_IKLayerUE4`.

**NOT VERIFIED, and it matters:** no sample character in this install uses `ACF_UE5ToUE4Retargeted_ABP`
as its anim class. The asset exists and is self-consistent, but I found no BP that consumes it, so this
is *an option ACF ships*, **not** evidence of ACF's intent for gameplay characters. Practical limit
either way: the warping nodes (`gs-anim-rig-compatibility` §1) and ACF's overlay blend masks stay keyed
to the **rendered** skeleton, so retargeting the pose does not retarget those.

Goblin Siege bakes, per rig, per clip set, with six retargeters on disk:
`Content/Characters/ScoutV2/RTG_MixamoToGoblin_InPlace`, `RTG_MixamoToGoblin_RootMotion`,
`RTG_MixamoToGoblin_Traversal`, `RTG_MannequinToGoblin_v2`, `Humans/RTG_MixamoToHuman`, and
`Humans/Retarget/RTG_Manny_To_Human` (ticket 333).

The bill is written down at
`AgentQueue/tickets/333-human-locomotion-attacks-move-onto-comba.md:79-84`: `auto_align_all_bones` was
re-run after a batch of 19 clips, only `Walk_F` was re-retargeted to test it, **"the other 18 kept the
unaligned pose"**, and Michael found the T-pose by opening an asset. The ticket's own conclusion — "A
retargeter setting proves nothing until every clip has been re-run through it" — is a cost that exists
only because the pose is baked: a baked clip carries **no link back to the retargeter that produced
it**, so a settings change silently splits the set into current and stale, and the only detector so far
has been a human eyeball.

Before the next rig arrives, and before re-baking the 52 goblin traversal clips, price the live option
against another baked set. If baking stays, the missing instrument is a **per-clip stamp of which
retargeter and which settings hash produced it**, checked in a batch (`gs-anim-rig-compatibility` §3 is
where that check belongs).
