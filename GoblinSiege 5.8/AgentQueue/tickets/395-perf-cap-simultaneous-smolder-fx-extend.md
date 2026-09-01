---
id: 395
title: Perf: cap simultaneous smolder FX, extend real fracture to kitbashed wall pieces, fix Inn's PieceNameFilters
agent: claude-grapple
status: done
claimed: 2026-09-01T01:39Z
build: succeeded
waiting_on:
evaluated: 2026-09-01T01:45Z
observed: 2026-09-01T01:41:49Z | Frame rate recovered to normal after smolder-FX cap ('the framerate was amazing'); Inn walls now break with real fracture debris instead of vanishing, interior furniture no longer floats after supporting walls are destroyed ('i watched it and the collapse looked great')
scenario: Live PIE on L_Tutorial_Island, player character, burned the Inn (GSBuildingObjective_293) repeatedly across three fix iterations
files: 
  - Source/GoblinSiege/Destruction/GSBurnFXComponent.h
  - Source/GoblinSiege/Destruction/GSBurnFXComponent.cpp
  - Content/Maps/L_Tutorial_Island.umap
---

## Goal

Follow-up to #393: real fracture assets fixed the merged houses, but a live playtest surfaced three
more problems in the same area - a genuine game-thread perf regression (not the collapse-piece-count
#393 already capped), a kitbashed (never mesh-merged) Inn whose walls only ever vanished instead of
breaking, and its interior furniture never being adopted at all.

## Generate

1. **Global smolder-FX cap** (`GSBurnFXComponent.h/.cpp`) - `MaxGlobalSmolderFX = 40`, same reasoning
   as `AGSBuildingObjective::MaxFireFX` but map-wide: this component has no per-building coordinator,
   so the budget is a file-scope static counter (`GActiveSmolderCount`) incremented on a successful
   `SpawnSmolder()` and decremented in `EndPlay` via a new `bCountedTowardGlobalSmolderCap` guard (so
   a refused spawn can never decrement someone else's count, and EndPlay running twice can't
   double-decrement). Root cause, found via `stat dumpframe` after #393's clustering fix did NOT move
   the frame time at all: the Inn (a genuinely kitbashed building, 440 individually-adopted pieces,
   never mesh-merged like the rest of the village) had every one of those pieces independently
   spawning a permanent (`bSmolderForever=true`) Niagara smolder system on burn-down via
   `SpawnGenericRubbleFallback` - measured at 577 simultaneous `N_PebbleDust`/smolder instances and
   ~70ms of game-thread time from particle-collision checks alone.
2. **Real fracture assets extended to kitbashed wall pieces** - re-ran the existing #393 tool
   (`UGSFractureToolsLibrary::BulkGenerateMissingBuildingFractures`, no code change) with
   `NamePrefix="SM_House_Wall"` instead of `"SM_MERGED_House_"`. Generated 39 new `GC_House_Wall_*`
   assets in `/Game/Destruction`. This is exactly the reusable-process property #393 was built for -
   the same tool, pointed at a different naming pattern, no new engineering.
3. **Inn's interior furniture was never adopted at all** - not a bug in `CrumblePieces`, a level-data
   gap: `AGSBuildingObjective::PieceNameFilters` only matches structural substrings (House, Roof,
   Wall, Window, Door, Foundation, Barn, Tavern), and the Inn's actual furniture/decor
   (`SM_Bar_Main`, `SM_Fireplace_Base`, `SM_RoundTable`, dozens more - confirmed live via VibeUE
   Python query, not guessed) matches none of them, so those pieces were never in `Pieces` at all and
   were left floating once their supporting walls were destroyed. Per this class's own header
   comment, an EMPTY `PieceNameFilters` adopts everything within `AdoptRadius` with no name
   filtering - cleared it on this ONE placed instance (`GSBuildingObjective_293`, whose `AdoptRadius`
   is already unusually large at 2523uu, clearly sized for its sprawling footprint) via
   `set_editor_property` + `save_current_level()`, not the C++ default, so no other building's
   adoption sweep is affected.

## Evaluate

- **Watched live in PIE by Michael, all three fixes**: "the framerate was amazing" (smolder cap),
  "i watched it and the collapse looked great" (wall fracture extension + PieceNameFilters fix
  together - walls now break like every other building, interior furniture no longer floats).
- Diagnosis discipline worth recording: the smolder-FX cost was found by actually re-measuring after
  the clustering fix (`stat dumpframe`) rather than assuming the first hypothesis (piece count) was
  the whole story - it measured as basically unchanged (182ms -> 202ms, still ~5 FPS), which is what
  sent this to the real culprit instead of stopping at a plausible-but-wrong fix.
- **Not independently re-verified**: whether the Inn's furniture adoption is pulling in anything
  UNINTENDED from outside its own footprint (the empty-filter risk the class's own header already
  documents: "Empty adopts anything with a static mesh inside the radius, which drags in barrels and
  market tables"). Michael's live watch covered the collapse looking right, not an exhaustive check
  of the adopted-piece list against the Inn's actual footprint boundary.
- Touched outside the original claim: none new - `GSBurnFXComponent.h/.cpp` were the intended files;
  the level-data change is a single per-instance property edit + save, not new source files.
- **AGENT_STATE.md** gets a DECISION entry: the smolder-FX global cap pattern (worth reusing if any
  other per-piece-forever Niagara system shows the same scaling problem), and a NEXT flagging the
  unverified adoption-radius risk on the Inn specifically.

## Refine

- Left deliberately undone: fracturing the REST of the kitbashed kit (roofs, floors, foundations,
  corners - only walls were done, matching the specific complaint). Same tool, different prefix,
  whenever the next piece type shows the same "walls disappear instead of breaking" report.
- Left deliberately undone: auditing `GSBuildingObjective_293`'s adopted-piece list for over-adoption
  from clearing its `PieceNameFilters` - flagged above, not silently assumed clean.
- Nothing changed in the smolder cap or wall-fracture generation after the first pass - both were
  confirmed correct on the first live watch.
