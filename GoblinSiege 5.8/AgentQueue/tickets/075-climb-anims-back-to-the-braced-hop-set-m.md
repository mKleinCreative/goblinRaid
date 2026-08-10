---
id: 075
title: "Climb anims: back to the braced hop set (Michael's call - better feel for goblins)"
agent: claude-climbrebuild
status: done
claimed: 2026-08-07T21:40Z
build: none
waiting_on:
evaluated: 2026-08-07T21:37:01Z
files: 
  - Content/Characters/ScoutV2/Animations/ThirdPerson_AnimBP_Gob.uasset
---

## Goal

Michael: *"take a look at the climb animations and see if we can move to using just the braced
jumping, it's a better feel for the goblins anyways."* This reverts the swap I made in #067.

## Generate

`ThirdPerson_AnimBP_Gob`, `Climb` state machine:

| state | #067 put | now |
|---|---|---|
| `ClimbIdle` | `A_MX_Climb_Idle_Gob` (0.57s) | **`A_MX_Braced_Hang_Gob`** (1.30s, both feet on the wall) |
| `ClimbUp` | `A_MX_Climbing_Gob` (3.83s continuous cycle) | **`A_MX_Braced_Hop_Up_Gob`** (1.67s) |
| `ShimmyL` | `A_MX_Left_Shimmy_Gob` | **`A_MX_Braced_Hop_Left_Gob`** (1.50s) |
| `ShimmyR` | `A_MX_Right_Shimmy_Gob` | **`A_MX_Braced_Hop_Right_Gob`** (1.67s) |

`ClimbDown` unchanged (`A_MX_Climbing_Down_Wall_Gob`). Every candidate verified for skeleton
(`GOB_Scout_v2_Skeleton`) and `enable_root_motion = False` before assignment - the climb is
velocity-driven, so a root-motion clip in a state would fight it. `validate_state_machine("Climb")`
returns valid, no errors or warnings.

## Evaluate

**#067's reasoning was wrong, and it was mine.** I swapped the braced hops out because a single
discrete hop played on loop "reads as stuttering", and replaced them with continuous cycles. That is
a defensible general animation principle and it was not what this character wants. Michael's read -
a goblin scrambling up a wall in braced hops - is the better one, and he had the original.

**A hop-based climb is also structurally better here, which I did not appreciate until now.** If
climbing is a series of hops, then getting over a lip is *just another hop* - no mantle montage, no
motion warp, no movement-mode change, no state to get stuck in. It sidesteps the entire top-out path
that has been failing all day.

**NOT PLAYED.** Compile and validation only.

**Related finding, not fixed here:** Michael reports the top-out montage plays while the camera and
position stay put. It is not a root-motion problem - `AM_GS_ClimbTopOut` has
`enable_root_motion_translation = True` and the character does have a `MotionWarpingComponent`. The
likely cause is the **warp target resolving to where he already is**: `ClimbTopOut` warps to
`ClimbLedgeLocation + 120`, and when he is jammed against a ledge the probe resolves to roughly his
own position, so the montage faithfully warps him nowhere. Needs its own ticket.

**Owed AGENT_STATE.md** - DECISION (2026-08-07, Michael): goblin climbing uses the BRACED HOP set,
not continuous climb cycles. Do not "improve" this to a smooth cycle again.

## Refine

- **Reverted my own change on the owner's say-so without re-arguing it.** #067 has a paragraph
  justifying the continuous cycles. It was a reasonable argument about a decision that was not mine
  to make.
- **Checked root motion on all four before assigning.** #067 found `ClimbDown` had been playing a
  `_GobRM` asset inside a velocity-driven climb; that class of mistake is easy to repeat.
- **Left the 0.18s transition blends alone.** They govern how snappy the hops read and are worth
  tuning, but only against Michael actually playing it - guessing at feel is what caused this ticket.

**Deliberately left undone:** the top-out warp-target bug above; making the climb MOVEMENT itself
hop-cadenced (discrete impulses rather than continuous velocity), which is the natural next step if
the hop animations feel right but the motion under them feels too smooth.
