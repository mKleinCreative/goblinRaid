---
id: 022
title: Delete empty gigantic Dreamscape blocking volumes from L_Tutorial_Island
agent: claude-perf
status: review
claimed: 2026-08-06T05:15Z
build: none
waiting_on: Michael to identify the actor he is hitting - five searches found nothing matching 'empty gigantic blocking volume'
files: 
  - Content/Maps/L_Tutorial_Island.umap
---

## Goal

Delete empty gigantic Dreamscape blocking volumes from L_Tutorial_Island

## Generate

**Nothing deleted. Nothing edited. The map is untouched and unsaved.** Five read-only searches of
the 9,067 actors in L_Tutorial_Island, looking for an empty gigantic blocking volume. None matched
the description, and I was not willing to delete visible geometry on a guess.

## Evaluate

**What I searched, and what came back:**

1. **StaticMeshComponents with `static_mesh == None` but collision on** - 12 actors
   (`House_2x1_L7_Detailed`, `WaterMill_Closed`, `Innbase`, `Bridge1/3/4`, `QuestBoard1`,
   `Square fence mid2`, `House_1x3_10`, `House_2x1_T9`, `House_1x15`, and `GS_Windmill`).
   **All report `get_actor_bounds(True)` = (0,0,0)** - an empty mesh component has no collision
   geometry, so these block nothing. They are dead clutter, not blockers. Note `GS_Windmill` is a
   live `GSMillObjective`, so a naive "delete everything empty" sweep would have destroyed a burn
   objective.
2. **`BlockingVolume` actors** - zero in the level.
3. **Volume / Brush actors** - 5, all engine infrastructure: RuntimeVirtualTexture, CullDistance,
   NavMeshBounds, LightmassImportance, PostProcess. None of them block a pawn.
4. **Actors whose only colliding components are bare shapes (Box/Sphere/Capsule, no mesh)** - zero.
5. **Colliding bounds >= 1500** - 63 actors. Every one is either real terrain/foliage
   (`SM_Terrain_*`, `Landscape`, `InstancedFoliageActor`), a real village prop (`VillageFence`,
   `StoneStairs`, `SM_VillageOak3`), or sky/backdrop.

**The nearest things to the description, all VISIBLE (`hidden_in_game=False`):**

| actor | mesh | scale | Z |
|---|---|---|---|
| `Plane2` | `/Engine/BasicShapes/Plane` | 11000 x 11000 x 100 | -1793 |
| `Plane9`-`Plane12` | `/Engine/BasicShapes/Plane` | 302 x 125 x 112 | +2024 .. +14821 |
| `Plane3/15/16` | Dreamscape `SM_CloudCard` | 39-74x | +21416 .. +42590 |
| `SM_Sphere` | Dreamscape `SM_Sphere` (skydome) | 141x | +47738 |

`Plane2` is a 110km colliding plane below the world. The rest are sky, clouds and backdrop. They
collide, which is arguably wrong, but they are not empty and deleting them removes visuals.

**Unrelated finding, and it matters more than this ticket.** `GEN_NavBounds_Village` is
**(4000, 4000, 2998)** at (-13000, 57000) - an 80m x 80m navmesh box for the whole village. That is
why #008 saw defenders walk off the navmesh and freeze the moment they chased the player any
distance: there is barely any navmesh to be on. Worth its own ticket.

**Owes AGENT_STATE.md** (FAILED/gotcha): an empty `StaticMeshComponent` has NO collision - a
mesh-less actor cannot be an invisible wall. When hunting invisible collision, sort by
`get_actor_bounds(True)`, not by "mesh is None". Also `PrimitiveComponent.bounds` is not readable
from Python here (returns nothing usable); `AActor.get_actor_bounds(bOnlyCollidingComponents)` is.

## Refine

Changed in response to my own evaluation: abandoned the "delete everything with an empty mesh"
approach after the bounds came back (0,0,0). Had I acted on the first search I would have deleted
12 actors that block nothing - including a live mill objective - and reported the problem fixed
while Michael kept walking into whatever is actually there.

**Deliberately not done:** deleting `Plane2`, the cloud cards or the skydome. They are visible
geometry from a marketplace pack; removing them to fix collision would be treating the symptom with
the wrong tool, and if they ARE the culprit the right fix is disabling their collision, not
deleting the sky.

**Handed back with a question rather than a guess.** I need Michael to point at the thing - walk
into it and read off the coordinates (`GS.Raid.Goto`-style debug or just the viewport transform),
or name it from the World Outliner. With a position I can identify the actor in one query.
