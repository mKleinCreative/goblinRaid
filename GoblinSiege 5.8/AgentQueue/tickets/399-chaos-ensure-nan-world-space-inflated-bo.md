---
id: 399
title: Chaos ensure: NaN world-space inflated bounds on mass building destruction (completeAllObjectives)
agent: claude-chaos-bounds
status: queued
claimed: 2026-09-01T23:47Z
build: none
waiting_on:
evaluated:
observed:
scenario:
files: 
  - none-investigation-only
---

## Goal

Chaos ensure: NaN world-space inflated bounds on mass building destruction (completeAllObjectives)

## Generate

**No fix attempted - this is a found-and-flagged report, not a fix.** Michael reported "the editor
crashed after I did completeAllObjectives" while in `L_Tutorial_Island` with the editor open. Found
the actual crash report at `Saved/Crashes/UECC-Windows-386C0D174648F69ABFDE62975604CAF1_0003`
(`CrashContext.runtime-xml`) and confirmed via `tasklist` that `UnrealEditor.exe` (PID 49092,
matching the crash report's own `ProcessId`) was still running immediately after - this was an
**Ensure**, not a fatal crash (`IsEnsure: true`, `CrashType: Ensure`). No data was lost; the editor
stayed responsive to a live Python round-trip immediately afterward.

The actual error:

```
Ensure condition failed: MWorldSpaceInflatedBounds[Index].IsValid()
[File: Chaos/Public/Chaos/GeometryParticles.h] [Line: 472]
Invalid world space inflated bounds. AABB: Min: [X=nan Y=nan Z=nan], Max: [X=nan Y=nan Z=nan],
BoundsExpansion(X=3.000 Y=3.000 Z=3.000)
```

Some Chaos rigid body ended up with NaN world-space bounds. `completeAllObjectives` is exactly the
kind of debug command that would mass-destroy multiple buildings at once, releasing many Chaos
fracture pieces as live rigid bodies simultaneously - a real stress case that a single manual burn
would rarely exercise.

**Working hypothesis, not confirmed:** this session had just finished regenerating all 84
`Content/Destruction/GC_*` fracture assets at `NumVoronoiCells=3` (down from 8, tickets #397/#398,
for build-size reasons) shortly before this was observed. A lower Voronoi cell count changes piece
geometry, and it's plausible - not verified - that one or more regenerated pieces has degenerate
(zero-size, or otherwise NaN-producing) geometry that only manifests once Chaos computes its
physics bounds at runtime. This is a real hypothesis worth checking, not an established root cause.

## Evaluate

**What's actually verified:** the crash report's contents (Ensure, not fatal, same PID as the live
process), and that the editor was confirmed alive and responsive via a real Python call afterward.

**What's NOT verified, and is exactly the next investigation's job:**
- Whether this is actually caused by the low-Voronoi-cell fracture regeneration, versus a
  pre-existing Chaos issue that mass-destruction would have surfaced regardless of piece count.
- Which specific building(s)/fracture asset(s) triggered it - the crash report doesn't identify
  the specific actor/asset, only the Chaos-internal callstack.
- Whether this recurs on a normal (non-mass, single-building-at-a-time) burn, or only under the
  `completeAllObjectives` stress case.

## Refine

Deliberately left uninvestigated further tonight - time-boxed against an assignment deadline, and
this doesn't block the immediate work (the video recording avoided `completeAllObjectives` as a
workaround). Left as `queued`, not `blocked` or `done` - there's no fix here to evaluate, just a
report with a real but unconfirmed hypothesis, honestly flagged as such rather than closed out
prematurely. Whoever picks this up next: start by reproducing with `completeAllObjectives` again
and checking the NEW crash report for which specific `GC_` asset's fracture pieces are involved,
then compare that asset's post-regeneration piece geometry (piece count, bounds) against its
pre-regeneration values.
