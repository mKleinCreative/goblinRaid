---
id: 062
title: Jump feels floaty; climb cannot pass a rooftop lip
agent: claude-move
status: done
claimed: 2026-08-07T06:04Z
build: none
waiting_on:
evaluated: 2026-08-07T10:05Z
files: 
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
---

## Goal

Jump feels floaty; climb cannot pass a rooftop lip

## Generate

**One pin literal changed, and it was the wrong one.** `ClimbStaminaExits` top-out accept gate, node
`55D17294436DD7E1D7188EA67F48C1EB` pin B: `ImpactNormal.Z >= 0.47` -> `>= 0.35`. Compiled, saved,
verified off a fresh `collect_garbage()` + reload. EventGraph still 556 nodes - no structural edits.

The reasoning at the time: a 7-agent workflow sampled roof-lip pieces and found normal-Z values of
0.39 / 0.43 / 0.44 / 0.45 being rejected by the 0.47 cutoff, so lowering it "rescues 4 of 5 measured
failures".

**The floaty-jump half of this ticket was never addressed.** #061 had already set `JumpZVelocity` 620
and `AirControl` 0.35; Michael's follow-up ("should land quicker, feels floaty") wanted `GravityScale`
or a shorter descent, and nothing here touched it.

## Evaluate

**This ticket's only change was wrong, and #067 proved it by measurement.** Two independent errors:

1. **The threshold was never what stopped Michael.** #067 swept a capsule up 11 real village facades
   and found `ClimbWallOffset` was **45** against a capsule radius of **52** - the capsule was born
   **inside the wall**, `initial_overlap = True` on **100%** of them. A swept move starting in
   penetration travels zero distance. The top-out was *available* the whole time and physically
   unreachable. Nothing about the normal gate mattered.
2. **0.35 is not a standable surface.** `Z = 0.35` is a **70 degree slope**. Warping a player onto one
   produces a top-out that lands nowhere - which is a direct contributor to the "falls off a
   mid-wall ledge" bug Michael later recorded on video. The correct value is `0.71` (walkable). The
   measured roofs at 0.44-0.69 are genuinely *not standable*: they need the climb to continue onto
   them as a new surface plane, not a mantle onto them.

**How I got it wrong, because it is the reusable part:** I wrote my hypothesis into a workflow's
CONVENTIONS block as established fact - "so topping out onto a roof whose edge overhangs slightly
FAILS" - and offered only candidate causes that presupposed it. Seven agents investigated rigorously
and none could contradict me. I also never looked for Michael's screenshot, and I demoted my own
workflow's finding about the no-sweep teleport to "a second problem I didn't ask about". Michael's
response was "You're not understanding the issue... clear context and try again fresh."

**The single question that resolved it** - "does he sink in, fall off, or stick?" - took one turn.
"Sticks but can't go higher" instantly ruled out the entire branch I had been working on.

**The 0.35 value is still live in the Blueprint as this ticket closes.** It is scheduled for revert
to 0.71 in Stage 1a of the approved climbing-rebuild plan. Closing this ticket does not mean the
change was kept.

**Touched outside the goal:** nothing. One pin.

**Owed AGENT_STATE.md** - FAILED: a top-out normal-Z gate of 0.35 accepts 70-degree surfaces as
standable; walkable is 0.71. And: when a player reports a movement bug, ask which symptom they see
before forming a theory - a blocked capsule and a rejected acceptance test look nothing alike once
you know which one you have.

## Refine

- **Reverted nothing here, deliberately.** The 0.35 -> 0.71 revert belongs in Stage 1a of the rebuild
  plan alongside the capsule-fit check that makes the gate meaningful. Flipping the literal back on
  its own would restore the *old* failure mode without adding the new protection, and this file has
  been edited enough times today by enough tickets.
- **Recorded the failure analysis rather than just the fix.** The mechanical error (wrong pin) is
  worth one line. The process error - stating a hypothesis as fact inside a subagent prompt, so that
  no amount of downstream rigour could catch it - is the part that would otherwise repeat.
- **Did not close this quietly.** #067 superseded it, and it would have been easy to mark this `done`
  and let the superseding ticket carry the story. A ticket that shipped a wrong change should say so
  in its own Evaluate.

**Deliberately left undone:** the floaty-jump half (wants `GravityScale` / descent tuning, still
open); the revert of 0.35 -> 0.71 (Stage 1a); everything else in the rebuild plan.
