---
id: 203
title: Combo link speed: the contact fix parked its reclaimed time in RecoverySeconds, which is the gap between swings
agent: claude-combofeel
status: done
claimed: 2026-08-20T17:03Z
build: none
waiting_on:
evaluated: 2026-08-20T17:18:06Z
observed: 2026-08-20T17:18:06Z | Michael swung the light combo across two tuning passes and signed it off: swing 2 now comes out of swing 1's follow-through instead of out of the goblin settling, and at 0.160 the remaining hitch between 1 and 2 is gone
scenario: PIE with the player goblin on the light attack chain, attack pressed twice to link stage 0 into stage 1; recovery changed 0.455 to 0.200, watched, then 0.200 to 0.160, watched again
files: 
  - Content/Blueprints/Abilities/GA_GS_SwordLight.uasset
---

## Goal

Combo link speed: the contact fix parked its reclaimed time in RecoverySeconds, which is the gap between swings

## Generate

Data change only, on `GA_GS_SwordLight`'s stage array. No C++, no build.

| stage | recovery | next swing | into the clip |
|---|---|---|---|
| 0 | 0.455 -> **0.160** | 0.969s -> **0.674s** | 71% -> **49%** |
| 1 | 0.333 -> **0.200** | 0.893s -> **0.760s** | 71% -> **60%** |
| 2 | untouched | | 89% |

Applied in the running editor and **verified against a reloaded asset**, not against the object still
in memory - `save_loaded_asset` was passed `only_if_is_dirty=False`, because `set_editor_property`
does not reliably dirty the package (AGENT_STATE, 2026-08-08). Stage edits were assigned back **by
index** and the whole array written back, because iterating a Python-exposed array of structs yields
copies.

Stage 2 is deliberately untouched: it is the last stage, so its recovery is the window before a NEW
combo can start, not a gap between swings.

## Evaluate

**Watched and signed off by Michael, 2026-08-20.** Two passes: 0.455 -> 0.200 fixed the reset but
left a small hitch; 0.200 -> 0.160 closed it. His words on the result: *"looks good to me."*

**The diagnosis was measured, not reasoned, and my first two guesses were both wrong.** I suspected
the clip was finishing early and blending to idle, and that `MontagePlayRate` was the shipped 1.5
default while the timing had been checked at 1.0. Measuring in the live editor killed both: the rate
is 1.0, and every clip is *longer* than its stage, so no montage ever blends out mid-combo. What the
player sees during recovery is the clip's own return-to-neutral, which looks identical to a reset and
is not one. **The instrument settled in one run what two readings of the source could not** - the
same lesson as `GS.Anim.Snapshot` (#137).

**Root cause sits with #184, and its own reasoning is why this was cheap.** Holding stage totals
constant while shortening windup parked the reclaimed time in `RecoverySeconds`. #184 explicitly
refused to touch chain speed in the same pass so the two variables could be told apart - which is
exactly what made this a one-line diagnosis rather than a search.

**What is NOT covered:** only the player pawn. Nothing here was watched on a horn-summoned goblin,
and `UGSGA_SwordLight` is the shared light-attack ability, so every AI that swings runs these same
stages. `GS.Combat.Duel` provably cannot see that class of difference (#135).

## Refine

Went in two steps rather than one so the second could be judged against the first. Left stage 1 at
0.200 rather than matching stage 0's 0.160 - Michael called out 1->2 specifically, and moving 2->3 in
the same pass would have made his verdict unattributable, which is the mistake #184 avoided.

**Deliberately left undone, and each wants its own ticket:**

- **The finisher swallows a queued input.** A press during stage 2's recovery sets `bComboQueued`,
  then `FinishRecovery` finds no stage 3 and simply ends - so mashing after the third swing eats the
  first swing of the next combo. Cheap to fix and it needs no timing change.
- **Stage 2's damage window is 0.567s**, a third of its clip. Wide windows land hits at times that do
  not match the blade - the same class of problem #184 fixed at the front of the swing.
- **The guard-break stage** was repointed by #184 and has never been measured or watched.
- **AI melee feel is unverified** against these new numbers.
