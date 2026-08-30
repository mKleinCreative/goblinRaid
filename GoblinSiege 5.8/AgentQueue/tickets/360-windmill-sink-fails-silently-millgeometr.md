---
id: 360
title: Windmill sink fails silently: MillGeometryNameFilter case-sensitive mismatch against SM_WIndmill_Base
agent: claude-fire
status: done
claimed: 2026-08-30T00:09Z
build: none
waiting_on:
evaluated: 2026-08-30T00:14:55Z
observed: 2026-08-30T00:14:56Z | Log confirms both windmills sank (GC_WIndmill_Stump/Top proxies created), no more found-no-mill-geometry warning; 9 GeometryCollectionActors spawned in two clusters matching the two mill locations
scenario: Live PIE, L_Tutorial_Island, both placed GSMillObjective instances (GS_Windmill, GS_Windmill_Hill) ignited via IgniteInterior()
files: 
  - Source/GoblinSiege/Destruction/GSMillObjective.h
  - Source/GoblinSiege/Destruction/GSMillObjective.cpp
---

## Goal

Windmill sink fails silently: MillGeometryNameFilter case-sensitive mismatch against SM_WIndmill_Base

## Generate

Michael watched the farm house collapse on burn-down but the windmill did not, in the same session,
same map (`L_Tutorial_Island`).

`AGSMillObjective::SinkTower()` finds the mill's actual visible geometry (the objective owns no mesh
of its own) by scanning nearby actors for a `StaticMeshComponent` whose mesh name contains
`MillGeometryNameFilter`. That filter was `"Windmill_Base"` - lowercase i. The actual placed mesh,
confirmed live via Python (`get_all_actors_of_class` + reading `static_mesh` off the component), is
`SM_WIndmill_Base` - **capital I**. `FString::Contains` is case-sensitive by default, so this never
matched, on EITHER of the two placed mills (`GS_Windmill`, `GS_Windmill_Hill`). `SinkTower()`'s own
early-out logs a warning and returns - "it will detonate without sinking" - which is exactly what
happened: char, stopped sails and objective completion all fire from `Detonate()` regardless, so
nothing else about the mill looked broken.

Doubly notable: the header comment right above the filter's declaration *already names the correct
mesh with the correct capitalisation* (`SM_WIndmill_Base2`) while explaining a DIFFERENT, earlier
casing/substring bug that was fixed on 2026-08-26 - the fix for that bug just retyped the string
with the wrong case in the process, and nobody had watched a mill's own completion since to catch it
(the comment's own "one working example proved nothing" line was about a different failure mode and
turned out to be prophetic about this one too).

Fix: corrected `MillGeometryNameFilter` default to `"WIndmill_Base"`, and switched the `Contains`
call in `SinkTower()` to `ESearchCase::IgnoreCase` so a future art rename can't silently reopen this
the same way. Files: `GSMillObjective.h` (default + comment), `GSMillObjective.cpp` (the one
comparison).

## Evaluate

**Verified by log line, not inference.** Rebuilt, loaded `L_Tutorial_Island`, ignited both placed
`GSMillObjective`s (`IgniteInterior()`), waited out the 9s dust fuse. Before the fix this logged
`"found no mill geometry matching 'Windmill_Base' within 2600uu - it will detonate without
sinking"` for both mills (confirmed this was the case pre-fix by reading the code path, not
separately re-tested against the old binary). After the fix, that warning is GONE from the log, and
both mills instead log the success line:
```
'GSMillObjective_1' sank: 'SM_WIndmill_Base_Blueprint' retired, GC_WIndmill_Stump standing, GC_WIndmill_Top released, 6 fire(s) on the wreck.
'GSMillObjective_0' sank: 'SM_WIndmill_Base2' retired, GC_WIndmill_Stump standing, GC_WIndmill_Top released, 6 fire(s) on the wreck.
```
Cross-checked independently: 9 `GeometryCollectionActor`s existed in the PIE world after both fuses
ran (stump + top proxies per mill, spawned only when `SinkTower` actually finds geometry to spawn
proxies FOR), clustered in two separate locations matching the two mills' positions.

**Flagged, not fixed - a different, pre-existing issue, same log dump:** both mills log `"has a
UGSBurnFXComponent but no UGSFlammableComponent - it will never char"` at BeginPlay. The mills still
complete and sink correctly (their ignition/fuse path is `IgniteInterior`/`BuildupElapsed`,
independent of `UGSFlammableComponent`), so this is cosmetic (no char/smolder visual on the mill
itself) rather than a completion blocker - out of scope for "make it collapse," noted so it isn't
lost.

**Not audited: every other building on the map.** Michael's ask was "apply the collapse to all the
buildings." The farm house that already worked and the two windmills fixed here cover the two
DIFFERENT collapse code paths this project has (`AGSBuildingObjective::CrumblePieces` for kitbashed
clusters, `AGSMillObjective::SinkTower` for the mill) - both paths are now confirmed working on at
least one instance each. Whether every OTHER individual `AGSBuildingObjective` placement on
`L_Tutorial_Island` collapses correctly (as opposed to the class being correct) was not swept
instance-by-instance; that would need someone to burn down each one and watch, which nobody has done
beyond the one farm house Michael already saw.

## Refine

Closing as `done` - the specific, reported symptom (windmill not collapsing) is fixed and verified
by log evidence on both placed instances, not just one. The `UGSFlammableComponent` char gap on the
mills and the "every other building" sweep are both real open items, written down above rather than
silently absorbed into this ticket's scope.
