---
id: 219
title: CrowdStats: report outer-ring claims and mark a breached engagement cap
agent: claude-acf
status: done
claimed: 2026-08-20T23:50Z
build: none
waiting_on:
evaluated: 2026-08-20T23:51:45Z
observed: 2026-08-21T00:35:57Z | The readout printed engaged N/6 with OVER ENGAGED and the inner+outer split, and it caught a real defect on its first run - 6 inner + 6 outer against engaged 10, which became ticket 220.
scenario: Same session; the 00:07 run exposed the double-claim, the 00:34 run confirmed the fix.
files: 
  - Source/GoblinSiege/Combat/GSDebugCommands.cpp
---

## Goal

CrowdStats: report outer-ring claims and mark a breached engagement cap

## Generate

`GS.Combat.CrowdStats` now prints `engaged N/Max` with an `<-- OVER ENGAGED` marker, and splits the
slot count into `N inner + M outer`.

Before: `engaged 8  swinging 2  weight 2/4  slots 6`. The `8` had nothing to compare it against, so
establishing that it exceeded `MaxEngagedAttackers = 6` took a subagent reading the component source
and correlating 26 log samples against order-issue timestamps by hand. `weight` already had its
`OVER BUDGET` marker; `engaged` did not.

## Evaluate

**NOT COMPILED, NOT RUN.** Editor open, gate shut.

`OVER ENGAGED` deliberately does **not** read as a fault. Ruling 34 keeps the Attack order uncapped
on purpose (`GSHordeSubsystem.cpp:549-555`), so the marker means "surplus exists" - which is exactly
the condition under which #218's outer ring is supposed to be doing something, and therefore the
thing you want to see next to `+ M outer`. The comment in the code says so, because a future reader
finding `OVER ENGAGED` in a log will otherwise file a bug against intended behaviour.

## Refine

This is the instrument for #218 and should be watched in the same session: a `GS.Horde.Order Attack`
on a defender with more goblins than ring slots should read `engaged 9/6 <-- OVER ENGAGED` and
`slots 6 inner + 3 outer`. If the outer count stays 0 while engaged exceeds 6, #218 did not take
effect and the readout has earned its keep immediately.
