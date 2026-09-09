---
id: 400
title: Smoke by proximity, and field fire stops at water/grass/props
agent: claude-smoke-water
status: review
claimed: 2026-09-09T15:40Z
build: none
waiting_on:
evaluated: 2026-09-09T16:28:43Z
observed:
scenario:
files: 
  - Source/GoblinSiege/Destruction/GSBurnFXComponent.h
  - Source/GoblinSiege/Destruction/GSBurnFXComponent.cpp
  - Source/GoblinSiege/Destruction/GSFieldFireObjective.h
  - Source/GoblinSiege/Destruction/GSFieldFireObjective.cpp
  - Source/GoblinSiege/Destruction/GSBurnMaskSubsystem.h
  - Source/GoblinSiege/Destruction/GSBurnMaskSubsystem.cpp
---

## Goal

Michael, 2026-09-09, two asks that turned into four changes:

1. "The smoke effect for burning buildings is fairly hefty. One other problem is the Inn spawns
   essentially one of those per wall panel or prop it feels like. Can we somehow limit the amount of
   smoke being produced by proximity so it's never one giant cloud of smoke?"
2. "We also need to make sure that only the wheat burns from the field fire" - grass and props
   (fences, carts, haystacks) named as things he watched catch - plus, unprompted: "it goes through
   the river. fire should stop on contact with rivers/water in general."

## Generate

**1. Smoke by proximity** (`GSBurnFXComponent.h/.cpp`). New `MinSmolderSpacing` (default 1500uu):
a piece that burns down within that distance of a live smolder plume chars normally and adds no
second column. `MaxGlobalSmolderFX` (#395's 40-plume budget) stays as the ceiling behind it.

The two answer different halves of one complaint. A count-only budget has no idea where anything
is, so the Inn's 440 kitbashed pieces claimed all 40 slots inside their own footprint - one opaque
cloud there AND nothing left in the budget for the rest of the village. To do spacing the code has
to know plume POSITIONS, so the file-scope `static int32 GActiveSmolderCount` became
`static TArray<TWeakObjectPtr<UGSBurnFXComponent>> GActiveSmolderFX` and the count is now derived
from `.Num()` - deliberately one structure rather than a counter plus a position array that have to
agree. #395's `bCountedTowardGlobalSmolderCap` guard is kept and now means "I am in the registry".
The scan is linear, bounded by the cap at 40, on a burn-down event rather than a tick, and it
self-cleans stale entries and skips other worlds (PIE vs editor).

**2. Only wheat chars, not grass** (`GSBurnMaskSubsystem.h/.cpp`). New `CropMeshNameExclusions`
(default `{"Grass"}`), checked BEFORE the include filter so an exclusion cannot be defeated by a
broader include. `CropMeshNameFilters` is the single substring `"Wheat"`, and the art names the
field's ground cover `SM_VillageWheat_Grass` - so "Wheat" matched the grass too. That mesh is
painted well beyond the fields, which is why the burn read as spilling across ground that was never
alight. The crop itself (`SM_VillageWheat_01` / `_02`) is untouched.

**3. Props stop catching** (`GSFieldFireObjective.h`). `bCanJumpToAdjacentFlammables` already
existed and its own comment named the exact things Michael listed - "fences, haycarts, a granary
built too close". Defaulted to `false`. The machinery (decision 26) is left intact, not deleted, so
a designer can turn it back on per field; what changes is that it is no longer every field's
default. **Open risk, see Evaluate.**

**4. Fire stops at water** (`GSFieldFireObjective.h/.cpp`). New per-cell `bIsWater`, probed once at
grid allocation by `ProbeWaterCells()`:

- any `APhysicsVolume` with `bWaterVolume` containing the cell (this is what `AGSWaterVolume`
  already is - the sea uses it), then
- a downward Visibility trace, matched against `WaterActorNameFilters` (default `{"River"}`) or the
  `GS_Water` actor tag.

`IgniteCell` refuses a water cell above the state checks, so a torch thrown into the river fails
exactly as spread across it does - `bDirectIgnition` is not an override, because that flag exists to
beat a DOUSED firebreak (wet crop that dries out) and a river is not that.

`GetBurntFraction`'s denominator changed from `Cells.Num()` to a new `BurnableCellCount`. Gating
ignition without this is half a change: water cells can never reach Burnt, so leaving them in the
divisor caps the achievable fraction. GS_MillField is only 3.4% river so 70% is still reachable
today - but a field crossing a wider stream would simply never complete, however long it burned.

Water cells draw deep blue in `DrawDebugState` (above state, since a water cell's state is always
Unburnt and drawing it as dry crop is the picture that would hide a mis-probed river), and
`ProbeWaterCells` logs its count at `Log`, with a `Warning` if a field probes as entirely water.
The file also gained its own `LogGSField` category - it was logging through `LogTemp` while every
other burn system had one.

## Evaluate

**Measured, not assumed.** The water design was settled against the live level, not from the map
name. `GS_MillField` is the only field on L_Tutorial_Island: 33x33 cells at 640uu. Tracing down at
all 1089 cell centres in the editor returned 995 Landscape, **37 `BP_RiverSpline_C`**, 34
StaticMeshActor, 18 InstancedFoliageActor, 3 windmill base, 1 `BP_WaterWheel_C`, 1 RoadFence, 0
misses. That is what establishes both that the river really does run through the crop and that a
Visibility trace can see it - the Water plugin is not enabled in this project at all, so there are
no Epic water bodies to query.

That same enumeration is why the default filter is `{"River"}` and not `{"Water"}`: eight
`BP_RiverSpline_C` are the river, but `SM_WaterWheel_Blueprint` and six `P_WaterFog` emitters also
contain "Water", and the water wheel genuinely sits under one of this field's cells. A "Water"
substring would have marked dry ground as river on the strength of a prop's name.

**NOTHING HERE HAS BEEN COMPILED OR RUN.** The build gate is closed and the editor is open; see
Refine. Everything above is a code and level-data claim, not a behavioural one. In particular:

- The 1500uu spacing default is reasoned (the Inn's own AdoptRadius is 2523uu) but has never been
  looked at. It is a look number and wants Michael's eye, not a rebuild-per-guess loop.
- **Open risk on change 3.** `bCanJumpToAdjacentFlammables` is `EditAnywhere`, so the placed
  `GS_MillField` may carry a serialized `true` from when the default was `true`, in which case
  flipping the C++ default does not reach it. Queried live and it reads `True` - but the running
  editor was compiled with the old default, so that reading cannot distinguish "serialized
  override" from "inherits the old default". **Re-check after the rebuild; if it still reads
  `True`, set it false on the instance and save the level.** Deliberately not done pre-emptively:
  that would mean claiming and re-committing the 189MB `.umap` for a change that may be unnecessary.
- Spacing is first-come, so a wall panel can claim the spot a whole-building plume would have
  wanted. Acceptable at 1500uu (they are within a plume of each other anyway); worth revisiting only
  if the smoke visibly attaches to the wrong part of a building.
- `StopSmolder()` deactivates a plume without releasing its registry slot. Pre-existing, only
  reachable when `bSmolderForever` is false (not the default), left alone as out of scope.

## Refine

Left deliberately undone: the build, and therefore every behavioural verification. This ticket
cannot compile without emptying the queue, and #399 - which this session picked up on Michael's
direction - is the other thing holding it. That is his call, not mine to close. The editor is also
currently open, which `Build-GoblinSiege.ps1` refuses on independently.

What a verification run should watch, in one PIE session on L_Tutorial_Island:

1. The field debug draw - a blue strip should trace the river through the field, and the
   `water probe: N of 1089` line should read 37 or thereabouts.
2. Torch the river directly - nothing should light.
3. Burn the field to completion - it should still reach 70% and complete, which is the check that
   the denominator change works.
4. Burn the Inn - a few separated plumes rather than one wall of smoke.
5. Watch a fence at the field edge NOT catch.
