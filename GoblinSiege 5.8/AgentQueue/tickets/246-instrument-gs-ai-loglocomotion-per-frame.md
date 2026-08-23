---
id: 246
title: Instrument: GS.AI.LogLocomotion, per-frame AI speed, to prove or refute the one-foot-step diagnosis
agent: claude-acf
status: done
claimed: 2026-08-21T22:04Z
build: done
waiting_on: Built and running. Needs Michael to capture a real fight: GS.AI.LogLocomotion 8 with an archer engaged. The fixes are deliberately unwritten until that measurement lands.
evaluated: 2026-08-21T22:13:36Z
observed: 2026-08-22T03:55:51Z | Ran the command in a live fight; it printed per-pawn speed tables that drove every subsequent decision, including refuting two of my own predictions
scenario: 8s captures during real fights with archers, guards, horde goblins and the player all in range
files: 
  - Source/GoblinSiege/Combat/GSAnimDebugCommands.cpp
---

## Goal

Before changing anything about Erika's stutter, build a runtime readout that can **refute** the
diagnosis rather than one that can only agree with it.

Michael's report: Erika "only takes a step forward with one foot, or something like that, but is
still stuttering." Two facts he supplied narrowed it before any code was read — the stutter is
**unchanged by #240**, and **the castle guards do it too and always have**. So it is the shared
human locomotion path, and it predates today.

## Generate

`Source/GoblinSiege/Combat/GSAnimDebugCommands.cpp` (+350 lines) — a new console command
`GS.AI.LogLocomotion [seconds=5] [radius=3000]`, living beside `GS.Anim.Snapshot` because it answers
the same class of question. Registers an `FTSTicker` for the capture window, samples every pawn's
XY speed and velocity heading each frame, then unregisters itself and reports.

It measures **two competing hypotheses from one capture**:

- **A** — speed is a *square wave*: short pulses separated by flat zeros (the diagnosis: a MoveTo
  re-executing against a sliding hold point, braking before it reaches walk speed).
- **B** — speed is *continuous* and it is the *heading* that jitters (the alternative: the RVO
  avoidance + separation steer, with the behaviour tree innocent).

Columns: `smpls | %rest | maxspd | flick/s | prop/s | turn d/s | verdict`. `%rest` and `turn d/s`
are what separate A from B. Plus an ASCII sparkline of the worst offender's speed trace, because the
*shape* is the evidence and a square wave versus a smooth ramp is instant by eye and tedious from
summary statistics.

`prop/s` is the part worth calling out: it replays the captured speed through the **proposed**
`Idle<->Walk` rule (enter 60 / leave 25) alongside the live one (10 both ways), so the AnimBP fix is
a prediction tested against real data **before** anyone edits an asset. If `prop/s` does not collapse
relative to `flick/s`, the proposed numbers are wrong and this says so while it is still cheap.

Two sampling decisions that exist to stop the instrument flattering the hypothesis: the sparkline
**peak-holds** per bucket rather than averaging (averaging a 0.15s pulse into a wider bucket is
exactly how a square wave becomes the ramp we are testing for), and heading change is accumulated
**only across consecutive moving samples** (spanning a stop would report the heading either side of
a pause as a "turn", the one artefact that could make B look true when it is not).

## Evaluate

**Verified at runtime, not by compile.** Build succeeded in 01:28. The command was then issued into
the relaunched editor via `gs_ue.py` and produced real output (`MyProject.log`, 22:12:24): a 1.1s
capture, 6 pawns found (`BP_CastleGuard01_C_1/2`, `BP_CastleGuard02_C_0/1`, `BP_ErikaArcher_C_1/2`),
every one correctly classified `still`, and the guidance line "nothing moved enough to judge.
Capture during a fight, with an archer in range." Registration, ticker lifecycle, pawn iteration,
the table, the still-pawn guard and the summary all executed. Nothing was silently unrecognised.

**What has NOT run: the measurement it was built for.** Every pawn was at rest in the editor world,
so no `SQUARE` or `turnjit` verdict has ever been produced, the sparkline branch has never been
reached, and `prop/s` has never been exercised against non-zero data. **The instrument is verified;
the diagnosis is not.** Those are different claims and this ticket does not conflate them.

Outside the goal: nothing. One `#include "Containers/Ticker.h"` added; no existing code altered.

Owes `AGENT_STATE.md` a DECISION line: *instrument built to falsify, not to confirm — the proposed
AnimBP thresholds are simulated against captured data before any asset is touched.*

## Refine

Nothing changed on re-reading, and here is why the first pass survives scrutiny: the one real risk
was building a detector that could only answer "yes", which is the failure `GSIsInReferencePose`'s
own comment records from #092. The peak-hold and moving-samples-only choices above are the specific
guards against it, and the summary has an explicit branch that prints "**THE BEHAVIOUR-TREE
DIAGNOSIS IS WRONG**" and names where to look instead.

Deliberately left undone: **fixes A (sticky hold point) and C (restore combat focus) are NOT
written.** They are the point of the plan, and writing them now would be the exact mistake this
ticket exists to prevent — shipping a fix ahead of the measurement that justifies it. They land in
one build after Michael captures a fight.
