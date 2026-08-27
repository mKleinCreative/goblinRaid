---
id: 328
title: Treeline generator: regenerate the GS_ForestWall treeline as a repeatable level-building command
agent: claude-crumble
status: done
claimed: 2026-08-26T23:52Z
build: none
waiting_on:
evaluated: 2026-08-26T23:58:08Z
observed: 2026-08-27T00:16:32Z | Regenerated treeline reads right along GS_ForestWall_6 and 7 - he said it is looking good
scenario: Editor viewport over the north and east boundary after regenerate with the per-wall rotation fix
files: 
  - Content/Python/gs_treeline.py
  - Content/Python/init_unreal.py
  - Content/Maps/L_Tutorial_Island.umap
---

## Goal

Treeline generator: regenerate the GS_ForestWall treeline as a repeatable level-building command

## Generate

**Michael, 2026-08-26:** *"regenerate the treeline, it had been moved and hasn't been regenerated
since. Maybe we should make that a command I can access easy, because this is going to be part of
the level building system."* Then, decisively: *"the treeline is along the GS_ForestWall_*"* - which
removed the one thing I would otherwise have had to guess.

**What the measurement showed**

**Only 142 of 2,123 forest trees lay anywhere near the blocker chain.** That is the whole bug in one
number: the `GS_ForestWall_*` boxes were moved to re-encase the play area and the forest never
followed, so the boundary was marked by an invisible wall with no trees on it.

**The tool** - `Content/Python/gs_treeline.py`

Built as a repeatable command rather than a one-off pass, because Michael said this belongs to the
level building system and the walls will move again.

    Tools -> Goblin Siege -> Treeline    (registered from init_unreal.py at startup)

    survey()        report only, changes nothing
    preview()       markers where trees WOULD go
    regenerate()    clear the band and re-scatter
    restore()       undo the last regenerate
    clear_preview()

**How it decides where the treeline goes.** For each blocker it takes the AABB, calls the longer
horizontal axis the wall run and the shorter one the outward normal, and picks the outward SIGN by
pointing away from the centroid of the 67 `GSBuildingObjective` actors - i.e. away from the village.
That resolved correctly with no hand-authoring: box 0 faces west, 1-5 face north, 6-7 face east.
Trees are then scattered in a band from 200uu to 3,700uu outside that face, rejected against a
minimum spacing, and **accepted only where the downward trace hits `Landscape`** - so none land on
roads, bridges, roofs, rivers or the water plane.

**Every regenerate backs up first** to `Saved/GSTreeline/treeline-<stamp>.json`, and `restore()`
replays it. Nothing here is one-way.

## Evaluate

**Result**

| | before | after |
|---|---|---|
| trees along the blocker chain | 142 | **658** |
| forest trees total | 2,123 | 2,639 |
| floating above real ground | 0 | **0** |
| rejected as not-landscape / steep | - | 0 / 0 |

Density is the one dial worth arguing about. Measured options at 3,500uu band depth:
spacing 750 -> 297 trees, 600 -> 453, **500 -> 658 (chosen)**, 420 -> 939, 350 -> 1,334. 500 gives
about one tree per 156uu of boundary, which reads as a forest edge that blocks sightlines rather
than an orchard row. It is a parameter: `regenerate(spacing=420)` re-runs denser.

**Judgement**

**NOT OBSERVED. Nobody has looked at the regenerated treeline yet** - and after this ticket already
shipped three "clean" measurements that Michael could see were wrong, that distinction is the whole
point. What the numbers prove is that 658 trees sit on landscape along the blocker chain and none
float. What they cannot prove is whether the edge reads as a forest, whether the band is the right
depth, or whether 500uu spacing looks right. Worth his eye specifically on:
- density, the dial above
- band depth 3,500uu - whether the forest is deep enough to hide the world edge
- the seams where two blockers meet at a corner, which the per-box scatter does not blend

**I could not capture a viewport screenshot** to check it myself: `CaptureViewport` rejected three
successive argument shapes and `get_toolset_json_schema` would not take a string toolset name. I
stopped rather than keep guessing at the schema, so this hand-back genuinely has no picture behind it.

**Also fixed this session:** the 8 `GS_ForestWall_*` blockers were rendering as solid white boxes in
game (`BasicShapeMaterial`, `hidden_in_game = False`) - visible in Michael's screenshot. Set hidden;
**collision deliberately left intact.**

**Still open:** the 50 trees standing on `Plane2` (`MI_VillageWater`) - marker `GS_MARKER_WaterTrees`
is planted at (3221, 60151) awaiting his call.

---

## Refine

### The generator was wrong, and Michael's screenshot is what found it

**I SHIPPED IT AND IT DID NOT WORK.** I reported 658 trees planted and clean numbers behind it.
Michael marked up a top-down screenshot - two walls selected in yellow, the old line drawn in red -
and said *"I need the red removed and the yellow places I have selected to generate new trees. This
hasn't happened yet."*

**The bug: `get_actor_bounds()` returns an AXIS-ALIGNED box, and every blocker is rotated.**
`GS_ForestWall_7` is yawed -122.7 degrees and `_6` -64.6. For a diagonal wall the AABB is a big
square, so "longer horizontal axis = the wall run" is meaningless and the outward normal is
whichever world axis happens to win. The scatter therefore ran along world X/Y instead of along the
wall. **All eight were affected, not just the two he selected** - their true forward vectors are
(0.39,0.92), (0.95,0.31), (0.98,-0.18), (0.99,-0.12), (0.86,0.51), (0.83,-0.55), (0.43,-0.90),
(-0.54,-0.84). Not one runs along an axis.

Fixed by working in each wall's own frame: `get_actor_rotation()` for forward/right, and the mesh's
unscaled `box_extent` times the actor scale for its true half-size. Real lengths are 7,651 to 21,234
uu against a uniform 600 uu thickness - the AABB had been reporting the diagonals.

**Reading the selection was the cheap part.** `get_selected_level_actors()` said exactly which two
walls he meant, so no part of that had to be guessed.

**The red line had to be recovered from the drawing**, since a stranded treeline is geometrically
indistinguishable from any other planting. The viewport camera was top-down at (16894, 61737) yaw
-16.2, so pixels map to world by rotating about the camera; scale came out at 68 uu/px from the two
wall centres. **Validated before deleting anything:** the mapping projects `_6` to pixel (494,161)
against (505,145) read off the screenshot, and `_7` to (238,110) against (259,92) - within ~18px,
comfortably inside the 2,200uu corridor used for the removal.

**Result**

| | before | after |
|---|---|---|
| trees in GS_ForestWall_6's band | 0 | **142** |
| trees in GS_ForestWall_7's band | 0 | **140** |
| all eight bands | 142 | **730** |
| old treeline removed | - | 246 |
| water-plane strays removed | - | 29 |
| floating above real ground | 0 | **0** |

`clear_path(points, radius)` and `clear_water_trees()` are now part of the tool, so an abandoned
treeline can be deleted by tracing it rather than hunting it.

**STILL NOT OBSERVED.** I have not seen the result - `CaptureViewport` rejected three argument
shapes and I stopped rather than keep guessing. Given that I already reported this feature working
once when it was not, that gap is the point: the numbers say 730 trees follow the walls, and only
Michael can say whether the treeline reads right.

---

### Marking things for deletion, and the road

**Michael:** *"how do I mark things for you to delete? more off it's looking good, but there's one
spot where you inserted some trees into the middle of a road."*

**The marking answer: GS_NoTrees_* boxes.** Foliage instances cannot be selected as actors, so
"select it and I will delete it" does not work for trees - a volume is the only handle that does.

    mark()          drops a box at the viewport camera, selected and ready to move/scale
    apply_marks()   deletes every forest tree inside every GS_NoTrees_* box

Both are on the Tools > Goblin Siege > Treeline menu. **The boxes STAY in the level**, which is the
important part: `_scatter_points` rejects against them too, so a marked spot is honoured by every
future regenerate() rather than being re-planted the next time the walls move. A mark is a permanent
statement about the level, not a one-shot delete.

**VERIFIED, not assumed** - a command handed over untested is worse than none. A test box on a real
scatter point produced 34 keepout rejections (730 -> 722 candidate points) and `apply_marks()`
deleted the 9 existing trees inside it. Box then removed and the band regenerated to 730.

**The road: found the mechanism, did NOT fix his case, and the distinction matters.**
The roads here are DECALS - 191 dirt decal actors of 476 total - and a decal has no collision, so a
downward trace passes straight through and reports clean landscape. "Only plant where the trace hits
Landscape" can never see a road. `_road_footprints()` now excludes 339 decal footprints by bounds.

**But that filter rejected ZERO points**, so whatever road he is looking at is not one of those
decals - most likely a landscape-painted layer or the stone path. The mechanism is real and worth
having; it is not evidence that his spot is fixed. Rather than guess at a third theory, that spot is
what `mark()` exists for.

---

### The buggy run's fallout, and who actually planted the road

Michael marked the spot: *"Look at GSRaidMarker18"* - the marking workflow used the moment it
existed, which is the point of building it.

**THE REAL DAMAGE FROM THE AABB BUG WAS NOT THE MISSING TREES, IT WAS THE ONES IT SCATTERED.** The
first run planted 658 trees along world axes. The corrected run then cleared the CORRECTED band and
replanted - so everything the buggy run dropped anywhere else was never cleared and stayed in the
level. **415 stray trees**, out past the walls at x 28,000-29,800 among other places.

Found by diffing against `Saved/GSTreeline/treeline-20260826-165614.json`, the backup taken at the
start of the very first regenerate - i.e. the last known-good state. Any tree present now, absent
there, and outside every corrected band was mine and wrong. All 415 removed; band still 730, nothing
floating.

**This is the argument for the backup-on-every-run rule paying for itself.** Without that file there
was no way to tell my strays from the map's own planting, and no way to clean up after a bug without
guessing.

**The road trees were NOT mine, and the check mattered.** Within 2,600uu of the marker: 27 trees,
**0 of them added by me** - every one is in the pre-regenerate baseline. The assumption in the
request was reasonable and wrong, and stating it plainly is worth more than quietly deleting them and
letting the generator carry the blame. The spot is genuinely bad either way, so they went.

Also worth recording: **there are no decals near that gate**, which is why `_road_footprints()`
rejected zero points. The road there is landscape-painted, and no trace-based or bounds-based test
can see it. **A painted road is invisible to the generator by construction** - only a marked volume
can protect it. That is not a gap to fix later; it is the reason `GS_NoTrees_*` has to exist.

`GS_NoTrees_0` now sits at the marker with a 1,200uu half-extent and took 9 trees. The ladder, so it
can be resized without re-measuring: <800uu 2 trees, <1200uu 5, <1800uu 15, <2600uu 27.

### What was left deliberately

The 27 pre-existing trees around the gate are down to 18: `GS_NoTrees_0` took the 9 within 1,200uu
and the box stays for the rest. Not widened on my own judgement - it is set dressing and the radius
is Michael's call, which is what the ladder in the section above is for.

A painted road cannot be detected. `_road_footprints()` covers decal roads and nothing else; the
landscape-painted ones need a marked volume, permanently. Recorded here rather than left as a
surprise for whoever regenerates next.
