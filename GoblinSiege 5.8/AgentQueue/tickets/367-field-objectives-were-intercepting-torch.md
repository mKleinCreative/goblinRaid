---
id: 367
title: Field objectives were intercepting torch hits meant for structures - fix FindObjectiveAtLocation priority
agent: claude-fire
status: done
claimed: 2026-08-30T04:52Z
build: none
waiting_on:
evaluated: 2026-08-30T04:55:58Z
observed: 2026-08-30T04:55:59Z | Torched the mill while a field was still actively burning (not complete) and it ignited correctly - the field no longer intercepts the hit
scenario: Live PIE, L_Tutorial_Island, field lit and actively burning, torch thrown at mill exterior
files: 
  - Source/GoblinSiege/Destruction/GSBurnObjectiveBase.cpp
---

## Goal

Field objectives were intercepting torch hits meant for structures - fix FindObjectiveAtLocation priority

## Generate

Root cause found via #366's diagnostic logging (see that ticket for the log lines). Every torch hit
during a live test - including ones that visibly landed on the hill mill's own tower mesh, and
separately one that hit a house - was claimed by `GSFieldFireObjective_0`, not the actual object hit.
`AGSBurnObjectiveBase::FindObjectiveAtLocation` iterates all burn objectives in `TActorIterator`
order (arbitrary) and returns the FIRST whose `ContainsWorldLocation` matches. A field's version of
that check is a coarse axis-aligned rectangle over its whole grid footprint - it has no concept of
"a mill or house happens to sit inside this rectangle." Confirmed independently by Michael before
the fix even built: torching the mill only worked once the field objective had already completed
(`FindObjectiveAtLocation` already skips completed objectives via `!Objective->IsComplete()`) -
exactly consistent with the field winning the lookup race while active.

Fix: `FindObjectiveAtLocation` now runs two passes. Structures (anything whose
`GetObjectiveType() != EGSBurnObjectiveType::Field`) are checked first and returned immediately on
match. Fields are held as a fallback and only returned if no structure claimed the point - so a
torch landing on a mill or building's own geometry always resolves to that structure, regardless of
whether it also happens to sit inside an active field's bounding rectangle. `AGSFieldFireObjective`
and `AGSMillObjective`/`AGSBuildingObjective` themselves are untouched - this is purely a dispatch-
priority fix in the shared base class.

Rebuilt (`Build-GoblinSiege.ps1 -IgnoreQueue`, solo, only tickets in queue were #366/#367 mine) -
18s, no new warnings.

## Evaluate

**Verified live, the exact previously-broken scenario.** Michael lit the field, then - while it was
still actively burning, not yet complete - threw a torch at the mill: "it burns now before the
Field." This is the precise repro that failed all session (mill exterior ignition was effectively
gated on the field finishing first, an accidental and confusing dependency nobody intended).

**This was the actual root cause of the whole mill-ignition difficulty this session was chasing** -
not a reach/height problem (the user's own first theory), not a collision/geometry miss (the second
theory the diagnostic logging in #366 was built to test), but an objective-priority bug that made
the mill's exterior route genuinely unreachable whenever an active field happened to overlap it.
#361/#362 (exterior-fire-immune retirement, `ContainsWorldLocation` override) were both real,
necessary fixes and are NOT wasted work - they were required for the mill to be reachable AT ALL via
this path - but neither could have worked reliably while this dispatch bug stood, since the field
would keep winning the race regardless of what the mill's own `ContainsWorldLocation` said.

## Refine

Closing `done`. The `#366` diagnostic logging (`TORCH HIT` lines in `GSTorchProjectile.cpp`) is
still live and marked temporary in its own comments - worth a follow-up pass to strip it once nobody
needs it, but harmless to leave for now (Warning-level, low volume, one line per throw).

## Generate

<!-- REPLACE: what you produced. Files touched, what each change does, the calls
you made. Delete this comment when you write the section. -->

## Evaluate

<!-- REPLACE: judge your own output against the goal, adversarially. What is
verified and by what evidence (a log line, a PIE observation, a compile result -
not "should work"); what is written but has never run; what you touched outside
the goal; the DECISION or FAILED line this owes AGENT_STATE.md. -->

## Refine

<!-- REPLACE: what you changed in response to your own evaluation, and what you
are deliberately leaving undone. "Nothing changed, and here is why the first pass
survives scrutiny" is a valid answer; silence is not. -->
