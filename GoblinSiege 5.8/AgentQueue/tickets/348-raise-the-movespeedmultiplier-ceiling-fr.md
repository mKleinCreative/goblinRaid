---
id: 348
title: Raise the MoveSpeedMultiplier ceiling from 3 to 6 so the dodge roll can exceed 3x walk speed (supplement to 345)
agent: claude-combat
status: done
claimed: 2026-08-29T02:41Z
build: none
waiting_on:
evaluated: 2026-08-29T02:49:49Z
observed: 2026-08-29T02:50:17Z | Measured the ceiling move directly in PIE: with the clamp at 3 the roll ran at MaxWalkSpeed 1410 for both a requested 6x and 12x and travelled 359uu; after raising the clamp to 6 the same roll ran at 2820 and travelled 417uu, restoring to 470 with zero residual velocity each time. The widened ceiling works - and the 16 percent distance gain against a predicted 100 percent proves the cap was not the real limiter.
scenario: PIE on L_Tutorial_Island, dodge triggered on the possessed live BP_GSPlayerCharacter_C_0 and start/end positions differenced across frames, before and after the clamp build.
files: 
  - Source/GoblinSiege/Attributes/GSAttributeSetBase.cpp
---

## Goal

Raise the MoveSpeedMultiplier ceiling from 3 to 6 so the dodge roll can exceed 3x walk speed (supplement to 345)

## Generate

`Source/GoblinSiege/Attributes/GSAttributeSetBase.cpp` - `PreAttributeChange` clamped
`MoveSpeedMultiplier` to `[0.1, 3]`. The **ceiling** is now 6; the floor is untouched.

The floor is the interesting half of that clamp (stacked slows must not hard-freeze a character); the
ceiling was incidental, and silently capped #345 dodge. `UGSGA_DodgeRoll` lifts walk speed for the
roll through this attribute, and requesting 6 and then 12 both produced `MaxWalkSpeed 470 -> 1410` -
a 3x lift, twice, from two different requested values. That repetition is what gave the clamp away.

## Evaluate

**Measured before and after:**

| Requested multiplier | Clamp | MaxWalkSpeed during roll | Roll distance |
|---|---|---|---|
| 6 | 3 | 1410 | 359uu |
| 12 | 3 | 1410 | unchanged |
| 12 | **6** | **2820** | **417uu** |

The clamp change does exactly what it says: the ceiling moved and `MaxWalkSpeed` followed. Restore is
clean - 470 afterwards every time, zero residual velocity.

**AND THE RESULT REFUTES THE PREMISE, WHICH IS THE POINT OF THIS SECTION.** Doubling the ceiling
(1410 -> 2820 uu/s) moved the roll from 359uu to 417uu: a **16% gain where the arithmetic predicts
~100%**. At 2820uu/s with friction and braking both zeroed, a ~0.9s roll should cover well over
2000uu. So the speed cap was *a* limiter but not *the* limiter - velocity is destroyed within a frame
or two of the launch by something else. `GroundFriction`, `BrakingDecelerationWalking` and
`MaxWalkSpeed` are all measured and excluded.

This ticket delivered a real, verified widening of a real, verified cap - and did **not** deliver the
distance it was opened to get. Both halves are true and the second matters more.

**Risk accepted:** this is a project-wide constant. Nothing else requests a multiplier above 3 today,
so it widens a ceiling nobody is touching. If a future stacking buff exceeds 3x, this is where to
look.

## Refine

**Changed in response to my own evaluation:** nothing in the code. The change is one number and it
was verified to do what it claims.

**Deliberately left undone - the real outstanding work:** the velocity loss above. It needs per-frame
velocity sampling through the roll (a few lines behind the existing `GS.Dodge` cvar) rather than
another guess at a dial. Michael banked 417uu and moved to Stage 2, which is right: the dodge travels,
has working i-frames and restores state cleanly. **Three dials have now been turned at this problem -
DodgeSpeed, friction, and this clamp - and the next step must be a measurement, not a fourth dial.**
