---
id: 414
title: Re-arm ACF locomotion states now that a UACFAnimInstance exists
agent: claude-locomotion
status: abandoned
claimed: 2026-09-10T20:17Z
build: none
waiting_on:
evaluated:
observed:
scenario:
files: 
  - Source/GoblinSiege/Characters/GSCharacterMovementComponent.cpp
  - Source/GoblinSiege/Characters/GSCharacterMovementComponent.h
---

## Goal

Re-arm ACF's locomotion states, on the theory that `UGSCharacterMovementComponent` disarming them was
why Erika plays idle while moving.

## Generate

**Nothing. The premise was wrong and no code was changed.**

## Evaluate

**The hypothesis is withdrawn.** `UGSCharacterMovementComponent`'s own header says the disarm stopped
being global on 2026-08-27 (#334): it is now `bDisarmLocomotionStates`, opt-out per Blueprint, TRUE
for the player and cleared on AI whose bands are authored. Read live in PIE:

| | disarm | bands | maxWalk | animBP |
|---|---|---|---|---|
| `BP_CastleGuard01_C` | **False** | 4 | **280.1** | `ABP_Human_C` |
| `BP_ErikaArcher_C` | (unset on the component) | 4 | 250.0 | `ACF_Humanoid_ABP_GSH_C` |

Erika's bands are populated - `EIdle` 0, `EWalk` 250, `EJog` 500, `ESprint` 650 - and her speed
tracks her patrol correctly across samples (250.0 -> 174.7 -> 0.0). **There is nothing disarmed to
re-arm.** I proposed this fix from AGENT_STATE's summary line without checking the header that
supersedes it, which is the same mistake as the root-motion claim earlier in this session: repeating
a documented statement that a later ticket had already changed.

**One real difference did surface, and it is worth keeping.** The guards' bands are authored to their
animations' actual ground speeds - 280.1 for `EWalk`, matching the retargeted PowerfulSword walk clip
per #334's own measurement - while Erika carries ACF's stock defaults (250/500/650), which have no
relationship to the FullSample clips she is now playing. Bands that do not match clip ground speed
give foot sliding even once the state machine is driving, so hers will need authoring against the
retargeted set. That is tuning, not the cause of an idle-only graph.

## Refine

**The remaining suspect is the retargeted AnimBP graph itself**, and it needs a human looking at it -
this is the limit of what can be established from Python. `ACF_Humanoid_ABP_GSH` was produced by
running ACF's working `ACF_Humanoid_ABP` through the IK retargeter, which copies the asset; whether
its state machine transition rules and the moveset layer's internal state machine survived that copy
intact has not been verified, and cannot be from here.

Concretely, open `ACF_UnarmedMoveset_GSH` and `ACF_Humanoid_ABP_GSH` and check whether the locomotion
state machine has its transition rules and whether the blendspace players are populated. Everything
feeding that graph is confirmed correct: the rig conforms, the animations are on the right skeleton
with root motion, the moveset is linked and selected from the equipped weapon, and Speed / Direction /
IsMoving all arrive with sane values.

> 2026-09-10T20:19Z Premise wrong: locomotion states are not disarmed on Erika (#334 made the disarm opt-out per Blueprint). No code changed.
