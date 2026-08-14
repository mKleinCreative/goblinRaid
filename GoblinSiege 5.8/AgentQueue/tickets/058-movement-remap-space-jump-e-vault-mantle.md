---
id: 058
title: Movement remap: Space=jump, E=vault/mantle/climb, F=interact, G=block, LeftAlt=dodge; jump-into-climb
agent: claude-input
status: done
claimed: 2026-08-07T03:05Z
build: none
waiting_on:
evaluated: 2026-08-07T05:32:32Z
files: 
  - Content/Input/IMC_Default.uasset
  - Content/Input/IA_Jump.uasset
  - Content/Input/IA_Climb.uasset
  - Content/Input/IA_Dodge.uasset
  - Content/Input/IA_Interact.uasset
  - Content/Input/IA_Block.uasset
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
---

## Goal

Movement remap: Space=jump, E=vault/mantle/climb, F=interact, G=block, LeftAlt=dodge; jump-into-climb

## Generate

Michael's control-scheme change, applied as DATA only (no C++, no EventGraph surgery). Run through a
7-agent workflow: 3 parallel investigators, 1 planner, 1 serial applier, 2 adversarial verifiers.

Created `IA_Jump` and `IA_Climb`; remapped `IMC_Default` from 16 rows to 17:

| key | now | was |
|---|---|---|
| SpaceBar | IA_Jump | IA_Traverse |
| E | IA_Climb | IA_Interact |
| F | IA_Interact | IA_Block |
| G | IA_Block | IA_Dodge |
| LeftAlt | IA_Dodge | (new row) |
| RightMouseButton | IA_Aim | unchanged |

`IA_Traverse` keeps its asset and now has zero mappings. Verified from a fresh load: all 17 rows
correct, nothing dropped.

**Two findings from the investigation that matter more than the remap.**

1. **`InteractAction` on the character is UNSET, and always was.** The interact framework was never
   bound in C++ at all, which squares with AGENT_STATE calling it "still untested, nothing has run".
   Moving interact to F changes nothing, because interact was never reachable from any key.
2. **There are TWO dodges.** The `G` roll is `UGSGA_DodgeRoll` - no montage, a `LaunchCharacter`
   impulse. But BP_GSPlayerCharacter also holds a full motion-warped directional dive
   (`DodgeMontage_Fwd/Back/Left/Right`, `DodgeDistance=650`, skew-warp) wired to **SpaceBar** via
   `DoTraverse`. All four montage slots point at the SAME asset, `AM_GS_Dive_RM`, and the four
   authored directional montages have zero referencers. The better-looking dodge is the one this
   remap unplugged.

## Evaluate

**THIS TICKET LEFT THE GAME BROKEN, and that was foreseen rather than discovered.** Repointing an IMC
row does not change what the character's CDO holds. After this ticket alone:

| verb | state |
|---|---|
| dodge / block / aim / move / look / sprint / attack / crouch / wheel / swap / guard-break | reachable - their C++ `*Action` properties already point at the right assets |
| **jump** | UNREACHABLE - `IA_Jump` is on Space but there is no `JumpAction` property |
| **climb, vault, mantle** | UNREACHABLE - they hang off `IA_Traverse`, which now has zero mappings |
| **the SpaceBar dive** | UNREACHABLE - same cause |

Verified by reading every `*Action` property off the CDO: there is no `JumpAction`, no `ClimbAction`,
no `TraverseAction`. I told the verifier that "climbing unreachable" MUST fail the check, precisely
so a green tick could not hide it.

**`IA_Climb` was a mistake.** The Blueprint already routes tap-vault / hold-climb off `IA_Traverse`.
Pointing E at `IA_Traverse` restores all three verbs with zero code; a new action asset needs a new
C++ binding to do the same job worse. Left in place rather than deleted mid-session, but the E row
should be repointed to `IA_Traverse` and `IA_Climb` retired.

**My workflow script was defective and got a correct result by luck.** Two of three investigators died
on API errors (`investigate:jump` and `investigate:input` both returned null). I guarded against
TOTAL discovery failure - `if (!climbVar?.found && !tracePins?.found)` in the previous run - but not
partial, so the plan and apply phases ran on nulls for two thirds of their input. The applier did the
right thing anyway. Next workflow of this shape needs a per-input guard, not an all-inputs one.

**Not verified:** nothing was played. The remap is proven by reading the table back, not by pressing
a key.

**Touched outside the goal:** none. All seven files were claimed before editing.

## Refine

- **Asked before building.** The first draft of this work would have moved climb to E and dodge to
  RMB as literally requested - straight over `IA_Interact` and `IA_Aim`, the latter being the entire
  ranged pass built earlier the same day. Surfacing both collisions produced a different and better
  layout, and cost one question.
- **Scoped the applier to assets only.** It was explicitly forbidden from touching C++ or the
  556-node EventGraph. That is why a run with two dead investigators still produced a clean,
  revertible result: `git checkout -- "GoblinSiege 5.8/Content/Input"` undoes all of it.
- **Told the verifier what MUST fail it.** "If climbing is currently unreachable, holds MUST be
  false" - otherwise an adversarial checker confirms the mappings are right and misses that the
  player can no longer climb. Verification has to be pointed at the risk, not at the change.
- **Did not delete `IA_Traverse`** despite it having no mappings. The Blueprint still listens for it,
  and it is the asset the E row should point at.

**Deliberately left undone:** the `JumpAction` C++ binding (#060); repointing E to `IA_Traverse`;
retiring `IA_Climb`; the hold-LeftAlt-plus-direction dodge, which needs edge detection because
`IA_Move` fires `Triggered` every frame and only fires `Started` on the 0-to-non-zero transition -
pressing D while already holding W produces no new `Started`; and jump-into-climb.
