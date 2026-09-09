---
id: 404
title: Validate generated fracture assets against the source mesh so #399 cannot recur
agent: claude-fracture-guard
status: done
claimed: 2026-09-09T19:12Z
build: none
waiting_on:
evaluated: 2026-09-09T19:26:15Z
observed: 2026-09-09T19:26:16Z | Ran the generator twice against a real house mesh and watched the gate decide: at a limit of 1x it refused to save and wrote nothing, at 4x the same asset saved, with the measured ratio (1.25x) logged both times. Two earlier versions of the same check were caught by that log line reporting worst piece 0 uu while the collection held 704 pieces
scenario: Editor Python calling UGSFractureToolsLibrary.generate_fracture_asset on SM_MERGED_House_Medium_15, once per limit, reading the result from the live log
files: 
  - Source/GoblinSiegeEditor/GSFractureToolsLibrary.cpp
  - Source/GoblinSiegeEditor/GSFractureToolsLibrary.h
---

## Goal

Stop #399 recurring. `GenerateFractureAsset` produced a geometrically corrupt collection
(`GC_MERGED_House_Medium_07`, pieces 270,634uu from a house whose source extent is 1,171uu), Chaos
NaN'd on it eight days later, and nothing pointed back at the tool. The fracture is
non-deterministic - the same mesh and parameters produced a corrupt asset on 09-01 and a clean one
on 09-09 - and `Content/Destruction` is gitignored, so every future regeneration is another roll
with no committed copy to fall back on.

## Generate

`UGSFractureToolsLibrary::GenerateFractureAsset` now measures its own output before saving and
refuses to write a result whose geometry sits absurdly far from the source mesh.

- New trailing parameter `MaxPieceReachRatio` (default 4.0), threaded through
  `BulkGenerateMissingBuildingFractures` so the batch path is covered too.
- The check runs after fracturing and clustering but BEFORE `SavePackage`, so a refusal writes
  nothing and leaves any existing asset untouched.
- On refusal: `Error` naming the offending vertex, its distance, the source reach, the limit, and
  the fact that a re-run usually succeeds.
- The measurement is logged at `Log` on EVERY run, pass or fail.

## Evaluate

**Verified both directions on a real asset**, which is the only reason this ticket is not shipping a
third broken version of the same check:

```
ratio 1.25x, limit 1x  ->  REFUSED to save GC_MERGED_House_Medium_15, nothing written
ratio 1.25x, limit 4x  ->  saved
```

A healthy result measures **1.25x** the source reach; the corrupt asset would have been **~231x**.
The gate catches it by two orders of magnitude and cannot plausibly fire on a good asset.

**Two earlier versions of this gate reported success while measuring nothing, and both would have
shipped as "done" without the always-on log line.**

1. `GeometryCollectionAlgo::GlobalMatrices(Collection->Transform, Collection->Parent, Out)` returned
   an **empty array** - `worst piece 0 uu (index -1)` while the collection held 704 pieces.
2. Walking the raw `Collection->Transform` array instead gave 740 entries whose translations are all
   **exactly zero**. A freshly fractured collection keeps every bone at the origin and puts the
   geometry in the VERTEX positions; the per-piece transforms seen at runtime are derived later by
   the physics proxy.

Both passed a deliberately impossible `MaxPieceReachRatio` of 1.0 without a word. The fix was to
measure `Collection->Vertex`, and the lesson is the one this whole area keeps teaching: a validator
that cannot be seen working is indistinguishable from one that does nothing. Hence the unconditional
log line - it is what caught both mistakes, in one build each.

**Not covered:**
- This validates GEOMETRIC REACH only. A collection could still be wrong in ways this does not see
  (degenerate faces, bad convex hulls, zero-volume pieces). It closes the failure mode that actually
  bit, not the class.
- The already-corrupt asset in the SHIPPED itch build is unaffected - this prevents recurrence, it
  does not reach a build that is already out.
- No retry loop. A refusal needs a human to re-run. Given the ~1-in-40 rate an automatic retry would
  be reasonable, and it is deliberately not here: a silent retry hides how often this fires, and
  nobody yet knows whether the real rate is 1-in-40 or worse on other meshes.

## Refine

`GC_MERGED_House_Medium_15` was regenerated twice by the test and now carries a fresh (validated)
fracture. That is a content change to a gitignored, regenerable asset, and it passed the gate at
1.25x.
