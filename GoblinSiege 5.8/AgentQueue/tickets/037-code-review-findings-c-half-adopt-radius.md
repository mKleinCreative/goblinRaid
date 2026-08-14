---
id: 037
title: Code-review findings, C++ half: adopt radius double-count, silent completion freeze, two lying diagnostics
agent: claude-fixes
status: done
claimed: 2026-08-06T18:12Z
build: required
waiting_on:
evaluated: 2026-08-06T18:17:22Z
files: 
  - Source/GoblinSiege/Destruction/GSBuildingObjective.cpp
  - Source/GoblinSiege/Destruction/GSBuildingObjective.h
  - Source/GoblinSiege/Raid/GSRaidDebugCommands.cpp
  - GoblinSiege 5.8/AGENT_STATE.md
  - decision-queue-kanban.html
---

## Goal

Code-review findings, C++ half: adopt radius double-count, silent completion freeze, two lying diagnostics

## Generate

The five remaining `HANDOFF.md` code-review findings. All five re-checked against the current tree
before editing, per #036's lesson - all five are still live.

**1. `GSBuildingObjective.cpp` - adopt radius counted the box DIAGONAL.**
`Dist(PieceOrigin, Origin) - PieceExtent.Size()`. `Size()` is `sqrt(x^2+y^2+z^2)`, the radius of the
sphere the box is inscribed in, not the box's reach toward this building. On a roof piece with extent
(700, 700, 200) that is 1005 instead of 700, so every piece was treated as starting ~300 uu closer
than it does, in all directions at once. Replaced with the exact point-to-AABB distance
(`(Origin - PieceOrigin).GetAbs() - PieceExtent`, componentwise clamped at 0, then `Size()`), which
is zero when the centre is inside the box and matches the comment's stated intent - "adopted if ANY
of it is inside the footprint".

**2. `GSBuildingObjective.cpp` - a building could freeze its completion in silence.**
`if (Shell > 0) SetCompletion01(...)` with no else, so a building whose adopted pieces are all
interior-named never scored at all and sat at its last value forever with nothing in the log.
Reachable today: `InteriorNameFilters` contains `"Beam"`, so a detached roof-beam cluster adopts
pieces, counts more than zero, and scores nothing. Now logs an `Error` - once, via `bWarnedNoShell` -
mirroring the existing `InitialPieceCount == 0` Error, which exists because this class of silent dead
objective has burned the project before. BeginPlay reports the same condition up front.

**3. `GSBuildingObjective.cpp` - the BeginPlay diagnostic described a denominator nothing uses.**
It printed `CeilToInt(InitialPieceCount * CompletionThreshold01)` while `RecomputeCompletion` divides
by the SHELL count, over-reporting the requirement by ~40% on a kit that is ~28% interior. Added
`CountShellPieces()`, used by the log, so the startup line and the live score cannot disagree.

**4. `GSRaidDebugCommands.cpp` - `BuildingStatus` re-derived the piece set, wrongly.**
It rebuilt "any actor with a flammable within a flat 2500 uu of `GetActorLocation()`" - pivot to
pivot, which is the exact bug `270d107` fixed inside `AdoptPieces` (this kit offsets meshes up to
671 uu from their pivots), with a radius unrelated to the building's own `AdoptRadius`. It
under-counted a tavern spanning more than 2500 uu and counted the neighbour's pieces between two
close houses. Now iterates `Nearest->GetPieces()` - a new public accessor - so the command describes
the set the building actually scores. An adopted piece with **no** flammable is now counted and
visible in `pieces=` vs `flammable=`; the old sweep used "has a flammable" as its filter for "is a
piece", so it could not see that failure at all.

**5. `GSRaidDebugCommands.cpp` - "nearest" building with no pawn.**
With `Pawn` null, `D` was `0.f` for every building, so `D < BestSq` was true only on the first
iteration and the command reported on an arbitrary iteration-order building while its help text said
"nearest". Now bails out loudly with a pointer to `GS.Raid.GotoBuilding`, matching `GS.Raid.BurnHere`
directly above it.

Also wrote the three FAILED entries owed by #034 and #036 into `AGENT_STATE.md`, and updated the
board's `review-findings` card.

## Evaluate

**NOT COMPILED, NOT RUN at the time of writing.** The gate is closed by this ticket.

**Finding 1 CHANGES GAMEPLAY NUMBERS on a system Michael called working this morning, and that is
the real risk in this ticket.** The old measure was over-permissive by up to sqrt(3)x, so correcting
it means every building adopts FEWER pieces. Two consequences, neither observed:

- Pieces near the edge of a footprint may now be adopted by nobody. That is precisely the failure
  #025 fixed ("76 of 113 windows owned nothing"), arrived at from the opposite direction.
- `gs_buildings.py` computes `adopt_radius = span * 1.1` and was tuned by watching results produced
  WITH the over-permissive C++ measure. The two are coupled; I changed one of them. If buildings now
  under-adopt, the honest fix is that multiplier, not reverting the geometry.

The placement data on the map is unchanged, so this is recoverable, but **it must be re-checked in
PIE before it is trusted**: `GS.Raid.GotoBuilding <n>` then `GS.Raid.BuildingStatus`, and compare
`pieces=` against the `adopted N piece(s) ... M of them shell` line each building now logs at
BeginPlay. A building that drops to a handful of pieces, or logs the new ZERO-shell Error, is this
change biting.

**Finding 4 makes the diagnostic's numbers change too - that is the point, but it means old
observations do not compare.** Any earlier `BuildingStatus` reading was of a different actor set.

**Verified only by reading.** No compile, no PIE. `GetPieces()` returns a reference to a member
`TArray<TWeakObjectPtr<AActor>>`; the caller null-checks each `.Get()`. `CountShellPieces()` walks
`PieceFlammables`, which `EnsurePiecesFlammable()` fills before the BeginPlay log runs - I checked
the call order rather than assuming it.

**Written but never run:** all five. In particular the new ZERO-shell `Error` has never fired, and
the "no player pawn" bail-out in `BuildingStatus` has never been hit.

**Not addressed from the finding text:** the reviewer's note that the proximity test runs BEFORE the
mesh-name filter, so any actor whose bounds envelope the building (landscape, foliage, blockout
volumes) passes the distance gate unconditionally. Still true - with the exact distance, a box
containing the centre gives 0. Adoption still rests entirely on `PieceNameFilters`. Left alone
because reordering the checks is a performance change dressed as a correctness one, and the name
filter does hold.

**Owed AGENT_STATE.md** - written, three FAILED entries covering the one-root path bug, the
re-check-a-finding lesson, and the unread-UPROPERTY class of bug.

**Touched outside the goal:** none. `AGENT_STATE.md` and the board were both claimed.

## Refine

- **Re-checked all five findings against the tree before touching anything**, because #036's finding
  1 turned out to have been fixed days earlier by an unrelated rewrite. All five here are live; that
  is now a fact rather than an assumption inherited from a document.
- **Used the exact point-to-AABB distance rather than swapping `Size()` for `GetMax()`.** The quick
  fix - subtract the largest extent component instead of the diagonal - is closer than the diagonal
  but still wrong in every direction except the one the longest axis points along. The exact form is
  three lines and has no residual error to reason about later.
- **Counted adopted-but-not-flammable pieces in `BuildingStatus` instead of skipping them.** The old
  loop used "has a flammable component" as its test for "is a piece of this building", which made one
  of the three failure modes the command exists to report structurally invisible. Now `pieces=` and
  `flammable=` differing IS the report.
- **Warn-once for the zero-shell case is a MEMBER, deliberately, not a file-scope static.** #030 and
  #034 moved three such latches to file scope - but those were on projectiles spawned fresh per shot,
  where a member can never latch. A building is a level-placed actor that lives for the raid, so per
  instance is right here, and a static would silence every OTHER broken building after the first.
  Same-looking bug, opposite correct answer; the header says so at the declaration.
- **Wrote the risk in Evaluate rather than softening the fix.** Compensating the geometry to preserve
  today's piece counts would be keeping a bug to avoid testing its removal.

**Deliberately left undone:** re-tuning `gs_buildings.py`'s `span * 1.1` - it may need to go up now,
but guessing before a PIE reading would be replacing one untested constant with another. Reordering
the name filter ahead of the distance test. Re-running the building placement script.
