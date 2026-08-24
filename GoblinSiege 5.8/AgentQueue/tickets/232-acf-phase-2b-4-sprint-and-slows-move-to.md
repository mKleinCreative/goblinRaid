---
id: 232
title: ACF Phase 2b-4: sprint and slows move to ACF locomotion states
agent: claude-acf
status: abandoned
claimed: 2026-08-21T03:56Z
build: none
waiting_on:
evaluated: 2026-08-21T03:58:04Z
observed: UNOBSERVED 2026-08-21T03:58:05Z - Abandoned before any code was written - nothing consumes ACF locomotion state yet, so the only effect today would be risking sprint.
scenario: none - never run
files: 
  - Source/GoblinSiege/Characters/GSCharacterMovementComponent.h
  - Source/GoblinSiege/Characters/GSCharacterMovementComponent.cpp
  - Source/GoblinSiege/Characters/GSCharacterBase.cpp
  - Source/GoblinSiege/Characters/GSCharacterBase.h
---

## Goal

ACF Phase 2b-4: sprint and slows move to ACF locomotion states

## Generate

**Nothing. Abandoned before any code was written**, on evidence gathered while scoping it.

## Evaluate

Ruling 27 says move speed becomes ACF locomotion states. Two greps decide whether that is worth doing
NOW:

- **Nothing in this project reads an ACF locomotion state.** `GetCurrentLocomotionState`,
  `ELocomotionState`, `IsSprinting()` and `SetLocomotionState` appear in our source exactly once, in
  a comment explaining why we disarmed the state machine.
- **We are not on ACF's animation stack.** No `.uasset` references `ACFAnimInstance`, which is the
  thing that would consume the state to pick movesets and overlays.

So re-arming the state machine today buys **nothing observable** and risks the one system Michael has
had broken twice this session and just confirmed working: *"sprinting worked, drained stamina and so
did dodging."* Sprint currently works precisely BECAUSE `UGSCharacterMovementComponent` empties the
bands - re-authoring them puts `MaxWalkSpeed` back under ACF's control, and sprint is a band the
player cannot enter by accelerating into it.

The cost is also not small. Sprint lives in `BP_GSPlayerCharacter`'s Event Tick as a `Select Float`
into `Set MaxWalkSpeed`; under ACF it has to become a `SetLocomotionState(ESprint)` call, which is
Blueprint graph surgery on a working system.

**This is a deferral with a trigger, not a rejection.** Ruling 27 becomes worth doing the moment ACF's
anim instance is adopted - at that point the locomotion state stops being bookkeeping and starts
driving what the character looks like, and `UGSCharacterMovementComponent` should be deleted in the
same change.

## Refine

The ledger entry for ruling 27 already records how the slows must work when this does land: the
multiplicative composition stays ours - carry x block x swing, which discrete states cannot express -
and only the product crosses over, pushed into `SetLocomotionStateSpeed` per state. Nothing here
changes that plan; it only moves when it happens.

> 2026-08-23T23:53Z Marked abandoned on Michaels instruction 2026-08-23. Its own closing note says it was abandoned before any code was written, so done was the wrong state - nothing was delivered and nothing needs reverting. If sprint and slows on ACF locomotion states are still wanted, they need a fresh ticket.
