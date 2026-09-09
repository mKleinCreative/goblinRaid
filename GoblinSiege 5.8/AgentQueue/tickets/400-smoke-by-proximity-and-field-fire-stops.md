---
id: 400
title: Smoke by proximity, and field fire stops at water/grass/props
agent: claude-smoke-water
status: done
claimed: 2026-09-09T15:40Z
build: none
waiting_on:
evaluated: 2026-09-09T19:08:18Z
observed: 2026-09-09T18:05:14Z | Michael played the rebuilt game and reported the wheat field is gold again before burning, confirming the black-field regression is gone; he also reported the river stops the fire
scenario: Live PIE on L_Tutorial_Island, played by Michael, walking the village and field before torching
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

---

## Correction, 2026-09-09 - the grass exclusion was wrong and is reverted

Michael, after playing the build: *"river works great, the collapse settles, which is nice, grass
stays green, but the wheat field is black before it got burnt."*

**Two mistakes in one three-line change, both mine.**

1. **`SM_VillageWheat_Grass` is not lawn grass.** It is the wheat field's own dense ground layer -
   part of the crop, and it is supposed to char with it. I read the mesh name as "grass" and
   excluded the thing the player actually looks at when they look at the field.

2. **Excluding a mesh does not leave it alone - it turns it BLACK.** The bind loop skips excluded
   components, but the component still carried a `UMaterialInstanceDynamic` from an earlier bind
   pass, so the exclusion only stopped `GS_BurnMask` being reassigned. A MID with no mask texture
   samples the engine `DefaultTexture`, which is WHITE, and the crop material reads white as fully
   burnt. Measured directly in PIE rather than reasoned:

   ```
   SM_VillageWheat_01     GS_BurnMask = TextureRenderTarget2D_0
   SM_VillageWheat_02     GS_BurnMask = TextureRenderTarget2D_0
   SM_VillageWheat_Grass  GS_BurnMask = DefaultTexture       <- the black field
   ```

Reverted: `CropMeshNameExclusions` now defaults EMPTY. The property and its mechanism stay, with the
hazard documented on the property itself, because an entry there currently means "permanently burnt"
rather than "never marked" for any mesh that was ever bound - a trap worth leaving a sign on rather
than deleting silently.

Re-verified live after the rebuild: all three wheat foliage components bind `GS_BurnMask` to
`TextureRenderTarget2D_0` again.

**The original complaint is therefore NOT addressed.** "Only the wheat burns" still stands as a
request; what has been established is only that `SM_VillageWheat_Grass` is the wrong target for it.
Which mesh Michael actually saw charring that should not have is an open question for him, and
guessing at a second mesh name would repeat exactly this mistake.

**Still standing from the same session, verified by Michael watching:** the river stops the fire, and
the collapse settles.

---

## Ruling, 2026-09-09 - smoke is reduced by COUNT, not by density

Asked directly whether he wanted fewer pillars or the same number of thinner ones, Michael:
*"fewer pillars of smokes works better."*

So the count levers stand and the Niagara asset is left alone:

- `MaxGlobalSmolderFX` = 12 (was 40)
- `MinSmolderSpacing` = 3500uu (was 1500)
- `P_SmolderSmoke_Converted`'s `Constants.Smoke.SpawnRate.SpawnRate` stays at **15.0** - deliberately
  NOT lowered. Each pillar keeps its authored density; there are simply fewer of them.

**Do not reach for plume scale.** `SetWorldScale3D` is inert on this system - its single emitter is
`LocalSpace: No`, so particles are spawned and sized in absolute world units and the component
transform only moves them. The `SmolderScale` / `SmolderAuthoredRadius` / `MaxSmolderScale`
mechanism has no visible effect here, and an earlier entry in this ticket claiming a 481x -> 6x
clamp fixed the sky-filling columns is **withdrawn**: those columns were several normal-sized
plumes, not one giant one.

The asset also has **no User parameters**, so there is no per-instance density override without
authoring one in the Niagara editor. If fewer than 12 is wanted, drop `MaxGlobalSmolderFX` - that is
the dial.

---

## Closing status, 2026-09-09 - what this ticket actually shipped

The Evaluate above was written before anything was compiled and is now stale in one direction only:
everything in it has since been built and run. Superseding it rather than leaving it to be read as
a to-do list.

**Shipped and confirmed by Michael watching it in a live raid:**
- Fire stops at water. "river works great." The probe reports `37 of 1089 cell(s) are water`.
- The wheat field is gold before it burns, and chars when it does.

**Shipped and measured, but NOT yet judged by eye:**
- Smoke by proximity. Final values after three rounds of his feedback: `MaxGlobalSmolderFX` 12 (from
  40), `MinSmolderSpacing` 3500uu (from 1500). Measured: active plumes 5-6 -> 3 at the same point in
  an identical burn; closest pair 636uu -> 15,722uu.
- Plume position: sourced from the intact footprint's BASE, so smoke rises from the rubble rather
  than from where the roof used to be. Measured: median height above ground -50uu, five of six
  within 200uu.
- Props no longer catch from the field edge (`bCanJumpToAdjacentFlammables` defaults false).
  **Nobody has watched a fence at the field edge fail to catch.** This is the one behavioural claim
  in this ticket with no observation behind it.

**Reverted, and the reason is worth more than the change was:**
- The grass exclusion. `SM_VillageWheat_Grass` is the wheat field's own ground layer, not lawn
  grass, and excluding a mesh does not spare it - it leaves a stale MID sampling the white
  DefaultTexture, which the crop material reads as fully burnt. The field went black.
  `CropMeshNameExclusions` now defaults empty with the hazard documented on the property.
  **"Only the wheat burns" therefore remains an OPEN request** - all that was settled is which mesh
  is the wrong target for it.

**Withdrawn claim:** an earlier entry here credited a 481x -> 6x plume-scale clamp with fixing the
sky-filling columns. Plume scale is inert on this system (`P_SmolderSmoke_Converted`'s single
emitter is `LocalSpace: No`, so particles are sized in absolute world units and the component
transform only moves them). Those columns were several normal-sized plumes, not one giant one, and
the fix was always going to be fewer rather than smaller. Michael, asked directly: "fewer pillars of
smokes works better."

---

## Observed and accepted by Michael, 2026-09-09 (after close)

The two items this ticket left flagged as unwatched, and the one it left open, are all settled by
Michael's own play rather than by further agent work. His words: *"they were observed by me."*

- **Props no longer catching at the field edge** - the one behavioural claim in this ticket that had
  no observation behind it. He has now watched it.
- **The smoke amount** - 12 plumes at 3500uu spacing reads right to him.
- **"Only the wheat burns"** - dropped as a request. `SM_VillageWheat_Grass` charring with the crop
  is correct behaviour (it IS the crop's ground layer), and no other mesh is charring that should
  not be. `CropMeshNameExclusions` stays empty.

Nothing outstanding on this ticket.
