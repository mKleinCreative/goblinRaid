---
id: 030
title: Code-review fixes: arc pool truncates the ribbon, decal depth reverted, flame warn-latch is per-instance
agent: claude-ranged
status: done
claimed: 2026-08-06T09:49Z
build: required
waiting_on:
evaluated: 2026-08-06T09:52Z
files: 
  - Source/GoblinSiege/Combat/GSAimComponent.h
  - Source/GoblinSiege/Destruction/GSTorchProjectile.cpp
---

## Goal

Code-review fixes: arc pool truncates the ribbon, decal depth reverted, flame warn-latch is per-instance

## Generate

Acting on the three `/code-review` findings that belong to my work. The other nine belong to
claude-raid (`gs_buildings.py`, `GSBuildingObjective`, `GSRaidDebugCommands`) and claude-queue
(`gsqueue.ps1`) and are reported to Michael, not touched here.

**1. The arc pool truncated the ribbon (the real bug of the three).** `MaxArcSegments` was a
fixed 40 while the pool needed `MaxSimSeconds * SimFrequency` = 5 * 15 = ~75 after #023 raised
the sim window. `SegmentCount = Min(Path.Num()-1, ArcSegments.Num())` then silently dropped the
tail, so the ribbon stopped roughly three-quarters along and **visibly failed to reach the
landing decal it exists to lead the eye to**. The pool is now DERIVED in `EnsureArcVisual`:
`Clamp(CeilToInt(MaxSimSeconds * SimFrequency) + 2, 8, MaxArcSegments)`. `MaxArcSegments`
becomes a hard ceiling (160) rather than the size, which removes the third number that had to
be kept in step with the other two.

**2. `LandingDecalSize.X` reverted 250 -> 40.** X is a HALF-depth about the origin and the decal
sits on the impact point, so 250 projects a quarter-metre both in front of and behind the
surface - smearing the ring up any nearby wall and painting the player's own mesh on a close
throw. I raised it in #023 on the theory that shallow depth was why no reticle appeared. #026
proved that theory wrong: the cause was `ArcMaterial` and `LandingDecalMaterial` being assigned
to each other's slots. The justification is gone, so the change goes with it.

**3. Warn-once latches were per-instance on a per-throw actor.** `bFlameSystemResolveFailed`
(mine) and `bTorchMeshResolveFailed` (pre-existing, identical shape) were members on
`AGSTorchProjectile`, which is spawned fresh for every throw - so each new torch started false,
retried the failed synchronous package load and warned again. Both are now file-scope statics.
I fixed the pre-existing twin as well rather than leave an identical bug sitting beside mine.

Also corrected a comment in `UpdateArcVisual` that still described `MaxArcSegments` as the
budget knob.

## Evaluate

**NOT COMPILED, NOT RUN.** The gate is closed (#024, #025, this). Verified only by grep: no
surviving references to the two removed members, and the statics are read and written where the
members used to be.

**The pool bug is the one that matters and I should have caught it in #023.** I explicitly
reasoned about the second-order effect of raising `MaxSimSeconds` - I caught that it would
break the decal's hit test and raised the window for it - and then did not carry the same
reasoning one step further to the thing that consumes the path. Same change, same paragraph,
same five minutes. The review caught what my own adversarial pass did not, which is worth
recording plainly rather than filing as "fixed".

**Cost of the fix:** the pool is now ~77 `USplineMeshComponent`s instead of 40, created lazily
on first aim and reused for the component's life. They are collision-free, shadow-free and
hidden when unused, so the cost is registration and memory, not per-frame work. Not measured.

**Not addressed from the review, deliberately:** the finding that raising `MaxSimSeconds` also
raised per-frame swept traces from 45 to ~75 while aiming. That is real and I have not measured
it. If aiming shows up in a frame-time capture, `SimFrequency` is the dial - it trades arc
smoothness for traces and is the honest place to pay.

**Owes AGENT_STATE.md** - FAILED: a fixed-size pool consuming a variable-length prediction is a
silent truncation. Derive the size or assert; do not leave two constants that must agree.

## Refine

- Fixed the pre-existing `bTorchMeshResolveFailed` twin rather than only my own. Leaving an
  identical bug adjacent to a fixed one guarantees someone re-reports it.
- Reverted rather than tuned the decal depth. With the real cause known, keeping a smaller
  version of a change made for a wrong reason would just be a slower way to be wrong.
- Updated the stale `MaxArcSegments is the knob` comment. Out-of-date comments have misled two
  of my diagnoses today (the skeleton-sockets comment in `GSWeaponComponent.h`, and this),
  which is enough evidence to treat them as defects rather than untidiness.
