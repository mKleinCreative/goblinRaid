---
id: 350
title: Attacking in mid-air freezes the goblin: root-motion montages override gravity - ignore root motion while falling so jump-attacks work
agent: claude-combat
status: done
claimed: 2026-08-29T04:36Z
build: none
waiting_on:
evaluated: 2026-08-29T04:41:38Z
observed: 2026-08-29T04:40:51Z | Measured the root-motion mode across a swing on the live possessed pawn: ROOT_MOTION_FROM_MONTAGES_ONLY before, IGNORE_ROOT_MOTION while swinging with IsFalling() true, and back to ROOT_MOTION_FROM_MONTAGES_ONLY once the ability ended, with no leftover gameplay tags. So an airborne swing no longer hands movement to root motion (which is what pinned the goblin in the air) and a grounded swing keeps its authored lunge.
scenario: PIE on L_CombatArena with the possessed BP_GSPlayerCharacter_C_0 alive; movement mode forced to MOVE_FALLING so the airborne branch could be caught, since a real jump completes faster than an MCP round trip.
files: 
  - Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.h
---

## Goal

Attacking in mid-air freezes the goblin: root-motion montages override gravity - ignore root motion while falling so jump-attacks work

## Generate

Michael: *"Being able to jump and attack at the same time would be lovely, but right now, when you
jump, it freezes you in mid air. This would be a way to emphasize the mobility of the goblins and
allow us to attack in vulnerable spots."*

**Cause, measured not guessed.** The player's attack montages play full-body on `DefaultSlot`, and
their source sequences carry root motion:

    AM_GOB_DA_Atk_Light1 -> A_GOB_DA_Combo_C1_RM   enable_root_motion = True
    AM_GOB_DA_Atk_Light2 -> A_GOB_DA_Combo_C2_RM   enable_root_motion = True
    AM_GOB_DA_Atk_Light3 -> A_GOB_DA_Combo_C3_RM   enable_root_motion = True

A montage carrying root motion makes the character root-motion-driven for its whole duration, which
**overrides gravity**: mid-air the goblin stops falling and hangs. This is the exact failure ticket
#128 recorded ("attacking freezes the player, the goblin") and fixed by stripping root motion; the
CombatMasterBundle retargets reintroduced it on the new clips.

Nothing was blocking the verb - `ActivationBlockedTags` covers Dead/Dodging/Carrying/Interacting/
Recoil, not falling. Attacking airborne was always permitted; it just froze you.

**The fix.** `SuppressRootMotionIfAirborne()` / `RestoreRootMotionMode()` in `UGSGA_SwordLight`. On
stage start, if `CharacterMovement->IsFalling()`, the anim instance's root motion mode is cached and
set to `ERootMotionMode::IgnoreRootMotion`; `EndAbility` restores it unconditionally.

**Why conditional rather than stripping root motion again (what #128 did).** Grounded swings SHOULD
be root-motion-driven - that is the authored lunge and weight shift the CombatMasterBundle
animations exist for. Removing it globally would flatten every grounded attack to fix an air case.
Suppressing only while falling keeps the ground exactly as authored and turns the air attack into a
real verb.

## Evaluate

**Measured on the live possessed pawn:**

| Moment | Root motion mode |
|---|---|
| Before the swing | `ROOT_MOTION_FROM_MONTAGES_ONLY` |
| Swinging with `IsFalling()` true | **`IGNORE_ROOT_MOTION`** |
| After the ability ended | `ROOT_MOTION_FROM_MONTAGES_ONLY` |

No leftover gameplay tags, character walking normally afterwards. Both the suppression and the
restore are confirmed.

**What this does NOT prove, stated plainly.** I forced `MOVE_FALLING` to catch the airborne branch,
because a real jump finishes faster than an MCP round trip - an earlier attempt using
`LaunchCharacter` sampled the pawn already landed. So the MECHANISM is verified; the FELT result of
jumping and swinging through a real arc is not, and needs Michael at the controls.

**The restore is the risky half and is placed accordingly.** It sits in `EndAbility` beside the
timer and move-speed cleanup, which every exit path runs through including cancellation by death or
a dodge. A leaked mode would be worse than the original bug: the character would ignore root motion
for *every* animation afterwards, so grounded attacks would quietly lose their lunge for the rest of
the raid - a fault that surfaces an hour later with no obvious cause.

**AGENT_STATE.md owes:** DECISION - *attack montages keep their root motion on the ground and ignore
it in the air; the goblin can attack mid-jump without leaving gravity.*

## Refine

**Changed in response to my own evaluation:** the first sampling approach (`LaunchCharacter` then
read) was worthless - it measured a landed pawn twice and would have "proved" the fix worked for the
wrong reason. Replaced with a forced `MOVE_FALLING`, which actually exercises the branch under test.

**Deliberately left undone:**

1. **The felt result is unverified** - see above. Michael to jump, swing, and confirm he keeps
   falling and lands normally, and that a grounded swing still steps forward.
2. **The damage window is unchanged in the air**, so an aerial swing hits on the same timing as a
   grounded one. A dedicated dive or plunge attack that reads differently would be its own stage
   with its own animation, and is a design decision rather than a bug fix.
3. **Defender attacks get the same treatment for free** - `A_HU_PS_Combo_*_RM` are equally
   root-motion, so a knight knocked airborne mid-swing will no longer hang either. Not observed.
4. **No air-specific tuning.** Aerial swings use the same arc, reach and sweep height as grounded
   ones, and the sweep already resolves low (#349 measured a `RightLeg` contact). Attacking downward
   from above may want its own sweep offset.
