---
id: 250
title: Archer step 1: reposition speed to 200 and widen the hold band to 300-1400
agent: claude-acf
status: done
claimed: 2026-08-22T01:27Z
build: none
waiting_on: Asset-only, no build. Needs a fight + GS.AI.LogLocomotion 8.
evaluated: 2026-08-22T01:28:35Z
observed: 2026-08-22T03:55:54Z | Both archers went smooth at 200 with the widened band: flick/s 0.2 each, max speed pinned at the new 200 cap
scenario: 8s capture during a fight with both Erikas engaged
files: 
  - Content/AI/BT_Archer.uasset
---

## Goal

The 520 cap from #248 fixed one archer and not the other. Act on which one.

## Generate

Measured after #247+#248 (`MyProject.log` 23:00:35):

| pawn | %rest | maxspd | flick/s | verdict |
|---|---|---|---|---|
| BP_ErikaArcher_C_1 | 73% | **161** | **0.5** | smooth |
| BP_ErikaArcher_C_2 | 24% | **520** | 1.2 | SQUARE |

The archer that WALKED went smooth; the one that hit the cap did not. C_1's 161 was accidental - she
happened to stay in band - but it is the only configuration observed to work.

- `BP_ErikaArcher` `MaxWalkSpeed` 520 -> **200**. `A_HU_Std_WalkF` is authored at 167.5, so ~1.2x
  slide, and at 200 she can never cross `Walk -> Run` (500) - the Idle/Walk/Run thrash is removed by
  construction rather than by tuning against it.
- `BT_Archer`'s `BTService_AcquireTarget` band 400/1000 -> **300/1400**. The player sprints at 818
  uu/s and covers the old +/-300 tolerance in under half a second, so C_2 was legitimately out of
  position constantly. Standoff stays 700.

Both verified by read-back after save; `BT_Archer.uasset` mtime moved.

## Evaluate

Written, never watched. **This reverses #248's reasoning**, which argued 520 to keep her above the
Run threshold. The measurement says the Run state is the problem, not the goal - so #248 is
superseded, not built on.

Risk accepted: at 200 an archer cannot retreat quickly from a charge. If she now reads as passive
under pressure that is the trade, and the band widening (1400) is what should stop her needing to.

## Refine

Nothing further. Next step is a capture, not another change - the last two changes both half-worked
and guessing a third on top would lose track of which did what.
