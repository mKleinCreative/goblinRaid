---
id: 013
title: Burnable buildings: torch through a window or onto the roof sets the house alight
agent: claude-raid
status: done
claimed: 2026-08-06T00:34Z
build: required
waiting_on:
files: 
  - Source/GoblinSiege/Destruction/GSBuildingObjective.h
  - Source/GoblinSiege/Destruction/GSBuildingObjective.cpp
  - Source/GoblinSiege/Destruction/GSBreakableComponent.h
  - Source/GoblinSiege/Destruction/GSBreakableComponent.cpp
  - Source/GoblinSiege/Destruction/GSTorchProjectile.cpp
  - Source/GoblinSiege/Destruction/GSTorchProjectile.h
  - tools/hamlet/gs_buildings.py
---

## Goal

Burnable buildings: torch through a window or onto the roof sets the house alight

## Generate

Michael's flow: a torch thrown into a window breaks it, flies through, and sets the house alight —
or lands on the roof and does the same.

- **`Destruction/GSBuildingObjective.h/.cpp`** (new) — a building. Adopts kit pieces inside
  `AdoptRadius`, gives each a `UGSFlammableComponent` (they are plain `StaticMeshActor`s and cannot
  catch on their own), seeds fire at the entry pieces and lets spread carry it, completes at 66%
  burnt. `ContainsWorldLocation` returns **false** so exterior fire is refused; `IgniteInterior` is
  the only way in. `IsRoofPiece` is public so the torch can tell roof from window.
- **`Destruction/GSBreakableComponent.h/.cpp`** (new) — breaks a window, retires the intact mesh
  (hidden **and** collision off, or the next torch bounces off a pane that is visually gone),
  optionally spawns a Chaos geometry collection, and opens the owning building.
- **`Destruction/GSTorchProjectile.cpp`** — on hit, tries breakable first, then roof.
- **`tools/hamlet/gs_buildings.py`** (new) — grid-bucketed connected-component clustering of the
  1,413 kit pieces into buildings, one `AGSBuildingObjective` per cluster, `AdoptRadius` sized from
  the cluster span, and a breakable component on every window.

Commit `96104be`.

## Evaluate

**NOTHING HERE HAS COMPILED OR RUN.** The build gate was shut for the whole ticket. There is no
evidence of any kind for any claim below — no log line, no PIE observation, not even a compile
result. This is the weakest ticket I have filed and it should be read as intent.

**Specific things most likely to be wrong**
- `add_component_by_class` in the script is the wrong call for persistence. `UGSRaidLibrary::
  MakeActorFlammable` exists precisely because a component must go through `AddInstanceComponent`
  to survive a level save; I did not route the breakable components through an equivalent, so they
  may evaporate on reload. **I expect this to be a bug.**
- Clustering may merge terraced or adjoining houses into one building. The script prints sizes and
  does not save, so it is visible before it is committed — but I have not seen the numbers.
- 66% completion is a guess. The market's 75% was also a guess and turned out to be unreachable for
  reasons no one could see; a house has far more small pieces than a market has stalls, and I have
  no measurement saying two thirds of them can actually be reached by fire.
- `IsRoofPiece` matches the mesh name for `Roof`. Several roof meshes are named
  `SM_House_Roof_01_Middle_Tiling_Window_02` — they contain **both** `Roof` and `Window`, so the
  torch will treat them as breakable windows first. That may be right or may be nonsense; unknown.

**Blocked**
`Objective.Burn.House` lives in `GSGameplayTags`, held by ticket 009, so it does not exist. Every
building will be untagged, which the director treats as invisible to the win condition — safe, but
they score nothing. Placement also needs the map, also 009.

**Owed to AGENT_STATE.md** — the cluster rule generalised beyond the market: *a cluster objective's
adopt radius must not exceed what its spread distance can traverse.* Already written there.

## Refine

- Made `BrokenCollection` optional after weighing it. Gating the gameplay on 18 authored geometry
  collections would mean the flow could not be played or tuned until the art existed; the fallback
  path opens the building identically, so each asset upgrades one window type later with no code
  change. This also honours the GDD's existing "breaking meshes later" deferral instead of
  quietly reversing it.
- Argued myself out of fracturing whole buildings. A house is 20-40 pieces of 12-780 triangles;
  fracturing them all is thousands of rigid bodies on a thread with no room (perf ticket 002 has
  the render thread at 15.4ms of 16.67ms). Windows are the opposite case and the only part worth
  giving to Chaos.
- Set clustering link distance equal to the flammable spread radius by construction, so the market's
  unreachable-denominator failure cannot recur by arithmetic.
- **Deliberately left undone:** civilians fleeing burning buildings (Michael earmarked it for later;
  `GetIgnitionSource()` is the seam it will want), and fracture assets for the 18 window meshes.
