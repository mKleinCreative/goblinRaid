---
id: 322
title: Clean up the treeline: trees climb the moved walls and their roots float in mid air
agent: claude-crumble
status: done
claimed: 2026-08-26T19:06Z
build: none
waiting_on: Set dressing - needs Michael to look at the wall line. 23 trees removed, 10 re-snapped, 2 oaks still ~1m proud. Backup of every affected transform is in 322-treeline-backup.json.
evaluated: 2026-08-26T23:58:07Z
observed: 2026-08-27T00:16:31Z | Treeline follows the moved walls; no floating roots, old line and water strays gone
scenario: Michael flying the L_Tutorial_Island viewport top-down and at ground level after the rotation fix
files: []
---

## Goal

Clean up the treeline: trees climb the moved walls and their roots float in mid air

## Report (not started - Michael's request, nobody has picked it up)

**Michael, 2026-08-26:** *"we need to clean up the treeline. I moved the walls to more accurately
encase the area I want encased, and you still haven't cleaned up the trees that were going up the
wall so we can see their roots floating in mid air."*

**What is wrong.** He moved the boundary walls inward/outward to enclose the intended play area. The
tree line did not move with them, so trees now intersect the walls and climb them, and where the
ground falls away behind the new wall line their root flares hang unsupported - visible floating
roots. Note "you still haven't" - this has been raised before and not actioned. It is a set-dressing
defect on the map the demo runs on, so it is seen by anyone who plays.

**Two distinct symptoms, and they may want different fixes:**
1. **Trees intersecting / climbing the walls** - instances that should be culled or pushed back now
   that the wall line has changed.
2. **Roots floating in mid air** - instances whose base no longer sits on ground, either because the
   wall replaced the ground beneath them or because they sit proud of a slope.

**Before touching anything, establish which of these the trees actually are.** L_Tutorial_Island's
vegetation is FOLIAGE INSTANCES, not actors - ticket #313 measured it while investigating the map's
177 MB size:

```
SM_VillageWheat_01_FoliageType      91,510
SM_VillageWheat_02_FoliageType      91,417
SM_VillageWheat_Grass_FoliageType   91,930
SM_VillageOak / Reeds x3 / Trees x3  ~3,500 combined
```

So the trees are ~3,500 foliage instances across `SM_VillageOak` and three tree types. They are
edited through the Foliage tools / `unreal.FoliageService`, NOT by selecting actors in the outliner,
and a fix is a cull-and-resettle pass over instance transforms rather than a code change. Confirm
that before planning: if any trees ARE placed actors, they need the other treatment.

**Likely shape of the work** (not a decision, just the obvious route):
- Find the current wall line, and cull tree instances within some distance of it or intersecting it.
- Re-drop the survivors onto the ground so no root flare floats - a trace-down-and-snap per instance.
- Michael judges the result by eye. This is set dressing; a screenshot pass beats any metric, and the
  measured numbers should be handed to him rather than iterated on blind.

**Not investigated here.** I was mid-way through #317's building-collapse work when this was raised
and have not looked at the map. Nothing above is a diagnosis - the instance counts come from #313's
measurements, and the cause of the floating roots is Michael's description, not something observed.

**Related:** #313 (the World Partition attempt on this same map, now closed as blocked) documents the
foliage layout and holds the `pre-worldpartition` git tag if a mistake needs undoing.

## Generate

**Measured before touching anything, because the scale was unknown and the fix deletes art.**

The trees are foliage instances on one `InstancedFoliageActor`, as #313's numbers implied - 2,232 tree
instances across five types (`SM_VillageBirch_01/02/03`, `SM_VillageOak`, `SM_Tree_Apple_01`) out of
279,337 foliage instances total. Wheat, reeds, lilies and grass were not touched.

The boundary is **177 actors**: `SM_VillageWall_TIling` x110, `SM_VillageWall_End` x64,
`SM_VillageWall_TIling_2` x3. Deliberately NOT the 377 `SM_House_Wall_*` / `SM_StoneWall_*` actors -
those are buildings, and culling trees against them would strip the hamlet's own planting.

**Done:**
- Removed the **23 trees fouling the boundary wall** (`remove_foliage_in_radius`, 50uu each). Net
  change was exactly -23, so no neighbour was caught.
- Re-snapped every floating tree to real ground.
- Removed **1 orphan** with nothing beneath it at all, at (288512, 429893) - far outside the map.
- **Every affected transform is recorded in `322-treeline-backup.json`** beside this ticket, so the
  whole pass can be reversed by hand.

## Evaluate

**Verified by re-measuring against the current world:**

| | start | now |
|---|---|---|
| trees fouling the boundary wall | 23 | **0** |
| trees floating above real ground | 46+ | **0** |
| worst gap | 5,211 uu | **0 uu** |
| orphans with nothing beneath | 1 | **0** |

**MY GROUND TEST WAS WRONG THREE TIMES, AND MICHAEL CAUGHT ALL THREE BY LOOKING OUT OF THE WINDOW.**
Each time the counts said clean and he could see trees in the sky.

1. **It accepted fog cards as ground.** The trace took any blocking hit, and the map carries 15
   `Plane*` cards. *"found what the issue is, they're colliding with the fog plane"* -> *"Plane8 in
   particular"* -> *"it's still on Plane6 and the GS_ForestWall"*.
2. **`trace_to_surface=True` silently undid the repair.** Chaos' own trace re-snapped trees back onto
   `Plane2`, which still has QUERY_ONLY collision. Fixed by placing at an explicitly measured Z.
3. **THE WORST ONE: the trace hit each tree's own trunk.** Foliage has collision, so a downward trace
   from a tree's base hits the `InstancedFoliageActor` immediately and reports a gap of -4 uu. **566 of
   572 `SM_VillageBirch_01` instances resolved that way, so the birches were never tested at all** -
   a birch hanging 5,000 uu in the air produced the same "grounded, gap 0" as one on the ground.
   Michael: *"you've been checking the apple trees it looks like, not the birch?"* He was reading the
   per-mesh breakdown better than I was. Excluding the foliage actor from the trace turned "0 floating"
   into **46 floating, 39 of them birches**, worst 5,211 uu.

**The lesson is one line: a trace that hits anything is not a ground test.** It must exclude every
surface that is not ground - fog cards, blockers, water, and the foliage's own collision.

**Re-snapping is exact:** net instance change was **+0 on all five types**, so no neighbour was
removed by the 40uu radius.

**NOT OBSERVED for the thing that matters.** Counts are clean; nobody has judged whether the tree line
now READS as a boundary. Removing 23 trees can open holes that measure fine and look wrong.

**Found while investigating, and fixed:** the 8 `GS_ForestWall_*` blockers were 2,000-uu boxes using
`BasicShapeMaterial` and `hidden_in_game = False` - solid white walls in play, visible in Michael's
screenshot. Set hidden; **collision deliberately left intact** so they still block.

**CLOSED by Michael:** the trees standing on `Plane2` (`MI_VillageWater`) were not a design choice -
*"the trees in the water are part of the old treeline, so kill that ticket when you delete the
trees"*. 29 were removed with the rest of the old line in #328 and the marker is gone. Worth noting
the marker did its job: it turned a question I could not answer into one he answered in a glance.

## Refine

Changed after evaluation: the ground test now excludes `Plane*`, `GS_ForestWall_*` **and the
InstancedFoliageActor itself**; re-placement no longer trusts `trace_to_surface`.

Conservative choices: 40-50uu removal radius, boundary walls only, and a full transform backup written
before the first deletion - `322-treeline-backup.json` now holds both passes.

Deliberately left: nothing. The water trees turned out to be old treeline and went with it.

---

**Continued in #328.** Michael then asked for the treeline to be REGENERATED along the
`GS_ForestWall_*` chain, and for it to become a repeatable command rather than a one-off pass.
That work, the `gs_treeline.py` tool and its numbers are reported there. This ticket stays scoped to
the original defect: trees climbing the moved walls and floating roots.
