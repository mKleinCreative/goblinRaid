---
id: 399
title: Chaos ensure: NaN world-space inflated bounds on mass building destruction (completeAllObjectives)
agent: claude-chaos-bounds
status: done
claimed: 2026-09-01T23:47Z
build: none
waiting_on:
evaluated: 2026-09-09T16:04:52Z
observed: 2026-09-09T16:04:47Z | Ran GS.Raid.CompleteAllObjectives in PIE and watched all three Chaos ensures fire in the live log (AccumulatedImpulse NaN, then twice invalid world-space bounds); scanned the running world and found 741 NaN piece transforms across three actors, all GC_MERGED_House_Medium_07. After regenerating that one asset, the identical run forced the same 71 objectives and produced no ensure at all, and the same scan found zero NaN transforms.
scenario: Live PIE on L_Tutorial_Island in the editor, mass destruction via GS.Raid.CompleteAllObjectives - the exact command Michael reported it on - run once before the fix and once after, same map, same command, same session
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

> 2026-09-09T15:26Z Picked up 2026-09-09 by claude-chaos-bounds2 (Michael directed). Investigation continues; no files claimed yet.

---

## Second pass, 2026-09-09 (picked up by claude-chaos-bounds2 on Michael's direction)

### The first report read only ONE of four ensure files. There were four, and the order matters.

`Saved/Crashes/UECC-Windows-386C0D174648F69ABFDE62975604CAF1_000{0,1,2,3}` are all the same editor
session. The original write-up quoted `_0003` only. Read in order:

| file | SecondsSinceStart | ensure |
|---|---|---|
| _0000 | 21806 | `false` - `GameplayTagsManager.cpp:2421` (unrelated, ~33 min earlier) |
| _0001 | 23800 | **`!Constraint.AccumulatedImpulse.ContainsNaN()` - `EventDefaults.cpp:141`** |
| _0002 | 23800 | `MWorldSpaceInflatedBounds[Index].IsValid()` - `GeometryParticles.h:461` |
| _0003 | 23800 | `MWorldSpaceInflatedBounds[Index].IsValid()` - `GeometryParticles.h:472` |

**_0001 is the one to chase.** The three fire in the same second, and the NaN appears in a
*collision constraint's accumulated impulse* first; the invalid world-space bounds in _0002/_0003
are that same NaN having propagated into the particle's bounds. The bounds ensure the first report
led with is the symptom, not the origin. (Callstacks in all four are module names only - no symbols
- so they identify nothing beyond "inside Chaos".)

### The log tail names the frame, and it is not the fracture regeneration

`MyProject.log` inside `_0001`, the seconds before the ensure at `23:02:59.507`:

```
23:02:53.330  6x 'GeometryCollectionActor_12x' collapse ring: 7 shoves of 5000000, inward 0.60
23:02:53.679  Warning: 652 of 652 piece(s) did not move, highest at local z=1137
23:02:53.679  Warning: 623 of 637 piece(s) did not move, highest at local z=144981   <-- 1.45 km
23:02:53.679  'GeometryCollectionActor_126' OUTCOME: DID NOT MOVE - ... mass 100005 kg
23:02:53.679  'GeometryCollectionActor_124' OUTCOME: DID NOT MOVE - ... mass 100003 kg
23:02:53.679  'GeometryCollectionActor_123' OUTCOME: DID NOT MOVE - ... mass 100003 kg
23:02:53.679  'GeometryCollectionActor_125' OUTCOME: DID NOT MOVE - ... mass 100002 kg
23:02:59.154  'GeometryCollectionActor_125' swept 77 straggler(s) of 637 that had not moved.
23:02:59.154  'GeometryCollectionActor_124' swept  7 straggler(s) of 347
23:02:59.154  'GeometryCollectionActor_123' swept 29 straggler(s) of 392
23:02:59.154  'GeometryCollectionActor_126' swept 34 straggler(s) of 652
23:02:59.330  [stack walk begins]
23:02:59.507  Ensure ... AccumulatedImpulse.ContainsNaN()
```

**147 stragglers were swept across four collections in one frame, and the NaN ensure is the very
next thing in the log.** `UGSCrumbleComponent::SweepStragglers` (`GSCrumbleComponent.cpp:800-817`)
does three things per straggler with no magnitude guard:

```cpp
Collection->CrumbleCluster(i);
Collection->ApplyExternalStrain(i, At, 300.f, 3, 1.f, 1000000.f);
Collection->ApplyBreakingLinearVelocity(i, FVector(0.f, 0.f, -300.f));
```

Three other numbers in the same window are worth holding onto, none of them yet explained:
- **mass ~100,000 kg** on the four big collections, and all four are the ones that DID NOT MOVE.
  Reads like a clamp or a density-derived value, not anything authored. The two that moved are
  7641 kg and 9086 kg.
- **`collapse ring: 7 shoves of 5000000`** applied to those same ~100t bodies.
- **a piece at local z=144981** (1.45 km) on `GeometryCollectionActor_125`, counted as "did not
  move". Note the log's "highest" is read off the CURRENT transforms, so this is either degenerate
  authored geometry or a piece already flung - the log as written cannot tell those apart. Worth
  resolving, because the first case is a direct NaN source.

### What this does to the original hypothesis

The first pass blamed the `NumVoronoiCells=8 -> 3` fracture regeneration (#397/#398). Testing that
against the crash history: **all 466 crash reports under `Saved/Crashes/` were scanned for these
three ensure signatures, and only this one session has any of them.** That session is after the
regeneration, so the hypothesis survives - but so does "this was the first mass-destruction at this
scale", and one sample cannot separate them. The regeneration is no longer the leading explanation;
the straggler sweep is, on temporal evidence the first pass did not have.

### Next

Reproduce in PIE with `completeAllObjectives` and see whether the ensure recurs at all. That is a
machine-checkable outcome (a new folder under `Saved/Crashes/` or not) and does not need a human
watching. If it reproduces, the bisect is the sweep: there is no cvar for it today
(`SweepTimer`, 6s repeating, set at `GSCrumbleComponent.cpp:317`), so gating it would need a code
change and the build gate is closed by this very ticket.

---

## Third pass, 2026-09-09 — REPRODUCED, ROOT-CAUSED, FIXED

### Reproduced on demand

Fresh editor, `L_Tutorial_Island`, PIE, `GS.Raid.CompleteAllObjectives` (71 burn objectives forced,
1 monument toppled). All three ensures came back in the same order and the same second-ish window:

```
15:56:15.021  !Constraint.AccumulatedImpulse.ContainsNaN()   EventDefaults.cpp:141
15:56:16.965  MWorldSpaceInflatedBounds[Index].IsValid()     GeometryParticles.h:461
15:56:18.373  MWorldSpaceInflatedBounds[Index].IsValid()     GeometryParticles.h:472
```

It is fully reproducible, not a one-off, and it is still an Ensure - the editor stayed up
throughout and kept answering Python round-trips.

### The second pass's hypothesis was WRONG, and the repro is what killed it

The straggler sweep was not involved at all. Measured in the repro run: **`swept` log lines = 0**,
`collapse ring` lines = 358, and the last GS activity before the NaN was a crumble OUTCOME report
three seconds earlier, with nothing but PSO hitching in between. The NaN arises inside the Chaos
simulation of already-released collections, not from any call this codebase makes in that frame.

The 2026-09-09 second pass had a tight temporal correlation (147 stragglers swept 0.35 s before the
ensure) and it was a coincidence of that particular run. Recording it because it was convincing and
it was wrong: correlation against ONE log is a hypothesis, and the cheapest way to kill it was to
reproduce and count, which took minutes.

### Root cause: one corrupt fracture asset, provable at rest

Scanning all 366 live `AGeometryCollectionActor` in the PIE world for non-finite or absurd current
transforms returned exactly three, and all three were the same asset:

```
GeometryCollectionActor_51   pieces=637  nan=228  maxAbsCoord=7.5e18  GC_MERGED_House_Medium_07
GeometryCollectionActor_79   pieces=637  nan=252  maxAbsCoord=7.2e18  GC_MERGED_House_Medium_07
GeometryCollectionActor_209  pieces=637  nan=261  maxAbsCoord=7.0e18  GC_MERGED_House_Medium_07
```

Three of three instances of that asset; **zero** across every other asset. Piece count is not the
variable - `_16` (732 pieces), `_15` (726), `_06` (721) and `_12` (652) are all bigger and all
clean. Neither is mass, nor the collapse-ring impulse, nor the ~100,000 kg figure the second pass
flagged: all of those apply identically to the assets that are fine.

The defect is in the asset's own REST transforms, before anything simulates:

| asset | rest pieces | max abs XY | max abs Z |
|---|---|---|---|
| `_15` healthy | 726 | 2,812 | 4,060 |
| `_16` healthy | 732 | 2,731 | 1,055 |
| `_12` healthy | 652 | 1,854 | 1,146 |
| **`_07` BAD** | 637 | **270,634** | **144,992** |

`GC_MERGED_House_Medium_07` carried fracture pieces **2.7 km** from the house. Chaos then has to
solve constraints spanning kilometres between bodies that belong to one building, and the impulse
accumulates to NaN - which is why `AccumulatedImpulse` fails FIRST and the invalid world-space
bounds follow as the NaN propagates. That 144,992 is the same piece as the `local z=144981` in the
2026-09-01 log, so this is one continuous defect across both sessions, not two incidents.

**The source art is innocent.** `SM_MERGED_House_Medium_07` measures 2336 x 2588 x 2128 uu - an
ordinary house, in line with its siblings. The corruption was introduced by fracture GENERATION.

### Fix

Regenerated that one asset from its own source mesh with the parameters the rest of the batch used
(`GenerateFractureAsset`, `NumVoronoiCells=3`, `MaxSimulatedPieces=24`, `bOverwriteExisting=true`).

| | before | after |
|---|---|---|
| rest pieces | 637 | 655 |
| max abs XY | 270,634 | **2,323** |
| max abs Z | 144,992 | **1,017** |

Then re-ran the identical repro - PIE, `CompleteAllObjectives`, 71 objectives forced again:
**no new ensure line of any of the three kinds** (count stayed at the 6 lines from the first run),
and a re-scan of all 366 collections found **zero NaN transforms**, where the same scan minutes
earlier found 741 across three actors.

### The durable finding, which matters more than the one asset

`Content/*` is gitignored (`GoblinSiege 5.8/.gitignore:32`), so `Content/Destruction/GC_*` is NOT in
the repo - by design, since it is tool-regenerable. Two consequences:

1. **This fix is machine-local.** A fresh clone regenerates these via
   `BulkGenerateMissingBuildingFractures`. It is not something that can be committed.
2. **The corruption can come back.** The same source mesh and the same parameters produced a
   corrupt asset on 2026-09-01 and a clean one today, so Voronoi site placement is random and this
   is a non-deterministic failure with roughly a 1-in-40 hit rate on that batch. Every future
   regeneration is another roll.

**Recommended follow-up (NOT done - needs a compile, and the build gate is closed):**
`UGSFractureToolsLibrary::GenerateFractureAsset` should validate its own output before saving -
compare the generated collection's rest-transform extent against the source mesh's bounding box and
refuse (or retry) when a piece lands orders of magnitude outside it. A one-line sanity check would
have caught this at generation time instead of via a Chaos ensure eight days later.

This also revises what #398's verification proved. That ticket confirmed the 84-asset batch by file
size (1.8GB -> 1.2GB), non-crashing, and a successful repackage, and concluded the assets were
"structurally valid, not just present on disk". They were structurally valid - a corrupt-geometry
collection loads, cooks, packages and ships perfectly well. None of those three checks can see a
piece 2.7 km from home. **The shipped itch build contains the corrupt asset.**

### Not verified

Nobody has LOOKED at the three affected houses collapsing since the regeneration. The asset changed,
so the collapse could read differently - piece count went 637 -> 655 and the fracture pattern is
freshly randomised. The physics is proven clean; the look is not, and that is Michael's eye, not a
number.
