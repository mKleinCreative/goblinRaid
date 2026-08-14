---
id: 124
title: Animation pass 2b: the melee facing snap turns 120 degrees in one frame
agent: claude-animsmooth
status: done
claimed: 2026-08-10T23:35Z
build: none
waiting_on:
evaluated: 2026-08-10T23:36:38Z
files: 
  - GoblinSiege 5.8/Source/GoblinSiege/AI/Tasks/BTTask_MeleeAttack.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/AI/Tasks/BTTask_MeleeAttack.h
---

## Goal

Animation pass 2b: the melee facing snap turns 120 degrees in one frame

## Generate

Step 3 of the animation pass, addressing "bodies snap-rotate to face".

Three places in the codebase turn a pawn toward something. Two of them are correct:

| where | how it turned |
|---|---|
| `BTTask_Block:221` | `TurnRateRadPerSec * DeltaSeconds` - rate-limited |
| `BTTask_MenaceOrbit:147` | `TurnRateRadPerSec * DeltaSeconds` - rate-limited |
| `BTTask_MeleeAttack:129` | `FMath::Clamp(NeededYaw, -MaxFacingSnapDegrees, +MaxFacingSnapDegrees)` with **no time term at all** |

`MaxFacingSnapDegrees` is 120, so a defender could whip through 120 degrees in a single frame in the
frame before every swing. That is the snap.

**Fixed by rate-limiting against elapsed WALL CLOCK, not frame delta.** This is the part that needed
care. `ExecuteTask` runs when the behaviour tree re-activates the node, *not* once per frame, so
`* DeltaSeconds` would tie turn speed to how often the tree happens to come back around - and if
that is slower than the frame rate, the agent turns in slow motion and may never face its target.
That is exactly the deadlock the existing comment in this function was written about, where an agent
standing on its stand-off slot failed the facing test forever. Elapsed time between activations
gives the same degrees-per-second regardless of activation rate.

New `FGSMeleeAttackMemory::LastFacingStepTime`; the step budget is
`TurnRateDegPerSec * clamp(Now - LastFacingStepTime, 0, 0.25)`, floored at one 60fps frame so a
first activation still turns, and still ceilinged by `MaxFacingSnapDegrees` so a long gap between
activations cannot re-introduce an arbitrarily large step.

The turn-then-fail structure is untouched: the node still turns as far as it may and returns Failed
if it is not yet facing, so the tree re-runs it. Only the size of each step changed.

## Evaluate

**Verified:** build result in Refine. The three clamps now agree with each other, which was the
point - `Block`, `MenaceOrbit` and `MeleeAttack` all step at the character's own
`TurnRateRadPerSec` (8 rad/s = ~458 deg/s, so a 120-degree correction takes ~0.26s instead of one
frame).

**NOT verified, and this one has a real regression mode:** whether defenders still get their swings
off. The whole reason the original snapped was to guarantee the agent faced its target promptly;
turning ~16x slower per activation means more failed activations before each swing. If the tree
re-activates roughly per frame this is invisible and correct. **If it does not, attacks will come out
noticeably slower or, worst case, an agent could circle without swinging.** The tell in a duel is
`GS.Combat.LogAI 1` and watching for "turning to face" lines repeating without a swing following.

**The dial if it is too slow:** `TurnRateRadPerSec` on the weapon data asset / race archetype, which
also speeds up Block and MenaceOrbit turning. There is deliberately no new melee-only rate dial -
inventing one would put the three turn sites back out of agreement, which is the bug this fixes.

## Refine

**Changed in response to my own evaluation:** the first implementation used
`World->GetDeltaSeconds()`, the obvious reading of "make it like the other two". Rejected on
tracing that `ExecuteTask` is activation-driven rather than per-frame, which would have made turn
speed depend on tree scheduling. Also removed a redundant second `GetWorld()` call once I noticed
`Now` was already resolved at the top of the function.

**Deliberately left undone:** the locomotion blendspace, the last item in the pass.
