---
id: 337
title: Stage 4 - UGSCorruptionDataAsset: weights, knees, curves and both grade ends leave C++
agent: claude-acf
status: done
claimed: 2026-08-27T23:46Z
build: required
waiting_on: 
evaluated: 2026-08-27T23:49:43Z
observed: 2026-08-28T03:54:51Z | Ran a PIE session with no tuning asset authored and the subsystem behaved as designed: it looked for DA_Corruption_Default, did not find it, warned loudly naming the path, and carried on driving the world from the C++ defaults instead of silently zeroing every term
scenario: PIE after the 18:15 build, with /Game/Data/World/DA_Corruption_Default deliberately not yet created
files: 
  - Source/GoblinSiege/World/GSCorruptionDataAsset.h
  - Source/GoblinSiege/World/GSCorruptionSubsystem.h
  - Source/GoblinSiege/World/GSCorruptionSubsystem.cpp
  - Source/GoblinSiege/World/GSCorruptionDirector.h
  - Source/GoblinSiege/World/GSCorruptionDirector.cpp
---

## Goal

Stage 4 - UGSCorruptionDataAsset: weights, knees, curves and both grade ends leave C++

## Generate

Tuning leaves C++. **`World/GSCorruptionDataAsset.h`** (new, header-only - the whole asset is data,
so `Evaluate` is inline and there is no .cpp): `UGSCorruptionDataAsset : UPrimaryDataAsset` carrying
the five weights, the per-TYPE objective weight map + its non-zero default, both soft knees, the
razed floor, the civilian multiplier and archetype row name, both `FGSCorruptionGrade` ends, and four
optional `UCurveFloat` response curves.

**`FGSCorruptionGrade` moved** from `GSCorruptionDirector.h` into the data header - it is data, both
the director and the asset need it, and neither owns it.

**`UGSCorruptionSubsystem`** gains `CorruptionDataPath` (Config `FSoftObjectPath`, C++ default
`/Game/Data/World/DA_Corruption_Default`) and `ApplyTuningAsset()`, which copies the asset over the
C++/Config defaults at `OnWorldBeginPlay` **before** the weight-sum check, pushes both grade ends
into the director, and seeds the civilian-weight cvar. `ObjectiveTypeWeight` is no longer static and
consults the asset's map first.

**Two decisions inside that are about failure, not features:**
- **A missing asset is a WARNING and the C++ defaults stand**, never a silent zeroing. A feature that
  cannot be told apart from a broken one is the mistake this whole effort keeps rediscovering.
- **A null curve means LINEAR, not zero.** Authoring the asset and leaving the curves empty is the
  first thing anyone will do; if that switched every output layer off it would look exactly like the
  feature being broken.

## Evaluate

**NOT COMPILED. NO ASSET AUTHORED YET.** This ticket ships the *class*; `DA_Corruption_Default` does
not exist, so the warning path is what will actually run on the next build. That is the correct order
- the asset cannot be created until the class it instances exists - but it means stage 4 is only half
delivered until someone makes the asset in the editor.

**A queue fault, and it is the third variation of the same mistake.** I first claimed #336 including
`Config/DefaultGame.ini`, which **#335 holds** - the queue refused to let me go active, correctly.
I abandoned #336 with nothing written and re-claimed as #337 without the ini, which stage 4 does not
need (the data path defaults in C++). Then I created `GSCorruptionDataAsset.cpp` **outside my claim**
before noticing, and removed it by inlining `Evaluate` into the header - which is a better design
anyway. Twice before I claimed files I never edited; this time I edited a file I never claimed. The
rule I keep half-following: **the claim and the edit set must be the same set, checked in both
directions.**

**Adversarially:**

- **The copy-into-members approach has a real cost.** The subsystem's `UPROPERTY(Config)` members now
  hold values that may have come from an asset instead, so the ini is no longer the truth once an
  asset exists. `ApplyTuningAsset` logs exactly what it overrode, which is the mitigation, but a
  reader who checks only `DefaultGame.ini` will be misled.
- **The curves are declared and NOT consumed.** Four `UCurveFloat` slots exist and nothing evaluates
  them yet - the response remapping is a further change to the director. Shipping the slots without
  the wiring is a half-feature and I would rather say so than let the asset imply more than it does.
- **`CivilianKillWeight` seeds a cvar**, so an asset edit mid-session does nothing until the next
  PIE, while a cvar drag takes effect instantly. Two paths to one number is a smell; it is
  deliberate, because the magnitude is still unfounded and wants dragging.
- **Nothing has run.** Five files touched, one new class, zero runtime evidence.

**Owes `AGENT_STATE.md`:** a NEXT line - *"stage 4 shipped the class; DA_Corruption_Default still
needs authoring, and the four response curves are declared but unconsumed"*.

## Refine

**Changed from my own review:** removed the unclaimed .cpp by inlining; seeded the civilian cvar from
the asset after noticing the asset carried a number the maths never read - which would have been a
field that looks authoritative and does nothing.

**Deliberately left undone:**

- **Authoring `DA_Corruption_Default`** - needs an editor session; the class must exist first.
- **Consuming the response curves** - a director change, and it wants the asset to exist so the shape
  can be judged by eye rather than asserted.
- **The `DefaultGame.ini` entry** for `CorruptionDataPath` - #335 holds that file.
- **Raising the kill log from Verbose, and reporting a failed `Cast<AGSEnemyCharacter>`** - both
  carried from #327's Refine; they belong with the curve work, not bundled here.

> 2026-08-28T01:00Z Adopted by claude-acf (was claude-corruption). Michael reassigned: this session has the 40 ACF skill packs listed, the previous one did not.
