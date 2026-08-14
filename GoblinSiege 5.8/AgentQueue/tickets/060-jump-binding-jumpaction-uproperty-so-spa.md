---
id: 060
title: Jump binding: JumpAction UPROPERTY so SpaceBar actually jumps
agent: claude-input
status: done
claimed: 2026-08-07T05:23Z
build: required
waiting_on:
evaluated: 2026-08-07T05:32:33Z
files: 
  - Source/GoblinSiege/Characters/GSPlayerCharacter.h
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
---

## Goal

Jump binding: JumpAction UPROPERTY so SpaceBar actually jumps

## Generate

#058 mapped SpaceBar to `IA_Jump` and nothing handled it. This binds it.

`AGSPlayerCharacter` gains a `JumpAction` UPROPERTY, bound in `SetupPlayerInputComponent` straight to
`ACharacter::Jump` (Started) and `ACharacter::StopJumping` (Completed AND Canceled). Binding only
Started would make every tap a full-height jump, because `StopJumping` is what ends the
variable-height hold.

No ability, no montage, no stamina cost. A jump you have to afford is a different design and the GDD
only prices the CLIMB jump.

**Measured the jump while Michael was in PIE**, which is the substance of this ticket. Live values
off the running pawn: `JumpZVelocity` 490.5, `AirControl` **0.05**, `GravityScale` 1.0,
`BrakingDecelerationFalling` 0, `FallingLateralFriction` 0.

That yields, at 980 gravity:

| | airtime | height | distance walking (470) | sprinting (818) |
|---|---|---|---|---|
| 490.5 / AirControl 0.05 | 1.00s | 123uu | 470uu | 819uu |
| 620 / AirControl 0.35 | 1.27s | 196uu | 595uu | **1035uu** |

`AirControl 0.05` is the larger cause of "superficial animation": you commit at takeoff and cannot
steer, which reads as an animation rather than a movement choice. Momentum already carries (both
falling-friction values are 0), so **sprint-jumping is what clears a street**.

Set both live on the running pawn so Michael could feel them immediately. They died with PIE, which
was expected - they are recorded in the `JumpAction` header comment so the numbers survive.

## Evaluate

**NOT COMPILED at the time of writing**, and the binding is the whole ticket - so nothing here is
proven until it is.

**`JumpAction` is UNSET on BP_GSPlayerCharacter, so SpaceBar will still do nothing after this
compiles.** Adding the UPROPERTY does not assign it; that is an editor step, and until it is done
this ticket has moved the problem rather than solved it. Same shape as `InteractAction`, which #058
found has been unset since it was written and is why interact was never reachable from any key.

**The tuning CANNOT be done in C++ and I nearly tried.** `JumpZVelocity` 490.5 against an engine
default of 420 proves BP_GSPlayerCharacter holds an override; a C++ default would be silently beaten
by it. 620 / 0.35 must be set on the Blueprint. The numbers are in the header comment rather than in
code precisely because putting them in code would look like they were applied.

**Written but never run:** the binding, obviously. Also unverified - whether `ACharacter::Jump` is
even reachable given the Blueprint may have its own jump handling I have not read, and whether the
AnimBP has falling/jump states at all. The investigator that was going to answer both died on an API
error and I did not re-run it. **If the goblin jumps in a T-pose, that is the unasked question.**

**Not addressed:** jump-into-climb, which is the half of Michael's request with actual design in it.
This ticket only restores a plain jump.

**Owed AGENT_STATE.md** - once verified: that BP_GSPlayerCharacter overrides `JumpZVelocity` and
`AirControl`, so movement feel is not tunable from C++ on this project.

**Touched outside the goal:** none; both files claimed.

## Refine

- **Measured before tuning.** "Covers no meaningful distance" could have been answered by raising
  `JumpZVelocity` and calling it done. Reading the live pawn showed `AirControl` at 0.05, which is
  the bigger cause and would not have been found by guessing at height.
- **Tuned on the live pawn first.** The values were applied to the running PIE character so Michael
  could feel them before anything was committed - a feel change argued from arithmetic is exactly
  what got the aim camera wrong twice earlier today.
- **Bound Completed AND Canceled to `StopJumping`.** Only Completed leaves a jump held forever if
  focus is lost mid-press - the same omission the weapon wheel needed fixing for.
- **Put the measured numbers in the header rather than in the constructor.** Writing
  `JumpZVelocity = 620` in C++ would compile, look correct in review, and do nothing, because the
  Blueprint override wins. A comment that explains why the obvious edit is futile is worth more
  than the futile edit.

**Deliberately left undone:** assigning `JumpAction` on the Blueprint and setting 620 / 0.35 - both
editor work, both required before a single jump happens; jump-into-climb; and checking whether the
AnimBP has any airborne state.
