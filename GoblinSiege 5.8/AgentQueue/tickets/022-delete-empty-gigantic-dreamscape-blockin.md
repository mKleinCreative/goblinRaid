---
id: 022
title: Delete empty gigantic Dreamscape blocking volumes from L_Tutorial_Island
agent: claude-perf
status: done
claimed: 2026-08-06T05:15Z
build: none
waiting_on: Michael to walk through the spot and confirm
files: 
  - Content/Maps/L_Tutorial_Island.umap
evaluated: 2026-08-06T05:57Z
---

## Goal

Delete empty gigantic Dreamscape blocking volumes from L_Tutorial_Island

## Generate

**RESOLVED 2026-08-06. Found via Michael's own position, not by searching.**

Five blind searches (below) found nothing. Michael then said "right where I am now there's
something stopping me". Querying the live PIE pawn location and running a sphere overlap found it
in one shot - which is the lesson of this ticket.

**The culprits: 12 Dreamscape FOG CARDS with collision.** `StaticMeshActor`s using
`/Engine/BasicShapes/Plane` with material
`DreamscapeSeries/SharedResources/Materials/Effects/MI_Fog_02`, all responding `ECR_BLOCK` to Pawn.
The one blocking him was `Plane` at (-13872, 75452, 2522), colliding extent (5553, 412, 2538) -
a 111m x 8m x 51m slab, with the player inside its bounds on all three axes. A Plane mesh is
single-sided and the material is translucent fog, so from most angles you see nothing at all and
still cannot walk through it.

**Action taken:** `set_collision_enabled(NO_COLLISION)` on all 12 fog cards - `Plane`, `Plane4`
through `Plane14`. NOT deleted: they are atmosphere art, and removing collision fixes the bug while
keeping the fog. Map saved (`save_asset` -> True; `L_Tutorial_Island.umap` on disk went from the
22:54:06 backup to 22:55:57).

**Left alone deliberately:** `Plane2`, the thirteenth BasicShapes plane, whose material is
`MI_VillageWater` - that is the village water surface at 110km, not fog. It also blocks Pawn, which
may well be intentional (so you do not fall through the water). Flagged, not touched.

Backup of the map before editing: `D:\goblinRaid\Map_Backup_20260806\` (176.3 MB).

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

**VERIFIED after the fix:** read-back of all 13 BasicShapes planes shows the 12 fog cards at
`NO_COLLISION` and none still colliding; the map wrote to disk at 22:55:57.

**NOT verified:** that Michael can now walk through the specific spot. He should re-run and try -
that is a five-second check I cannot do for him, and it is the only thing that closes this.

**Owes AGENT_STATE.md** (FAILED/gotchas):
- An empty `StaticMeshComponent` has NO collision - a mesh-less actor cannot be an invisible wall.
  When hunting invisible collision, sort by `get_actor_bounds(True)`, not by "mesh is None".
  `PrimitiveComponent.bounds` is not readable from Python here;
  `AActor.get_actor_bounds(bOnlyCollidingComponents)` is.
- **The Dreamscape fog cards ship with collision on.** 12 in L_Tutorial_Island. If a new Dreamscape
  scene is dropped in, expect the same: `/Engine/BasicShapes/Plane` + `MI_Fog_02` + `ECR_BLOCK`.
- **Ask the player where they are.** Five structured searches over 9,067 actors found nothing;
  one sphere overlap at the reported position found it immediately. For "something is blocking me",
  get a position first and search second.

## Refine

Changed in response to my own evaluation: abandoned the "delete everything with an empty mesh"
approach after the bounds came back (0,0,0). Had I acted on the first search I would have deleted
12 actors that block nothing - including a live mill objective - and reported the problem fixed
while Michael kept walking into whatever is actually there.

**Deliberately not done:** deleting `Plane2`, the cloud cards or the skydome. They are visible
geometry from a marketplace pack; removing them to fix collision would be treating the symptom with
the wrong tool, and if they ARE the culprit the right fix is disabling their collision, not
deleting the sky.

**Departed from the literal instruction, and said so.** The ticket says "delete". I disabled
collision instead, because the things turned out to be fog cards rather than the empty volumes they
were reported as - deleting them would have removed atmosphere art to fix a collision flag. One
word from Michael and they go; the reverse (undoing a delete of 12 actors from a 176MB map) is far
more expensive.

**Also did not delete `Plane2`** despite it being the single largest collider in the level. It is
the village water. Deleting the water to fix a walking bug would have been the same error at
greater cost.
