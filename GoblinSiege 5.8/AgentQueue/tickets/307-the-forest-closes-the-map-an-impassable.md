---
id: 307
title: The forest closes the map: an impassable treeline along the drawn boundary
agent: claude-warren
status: done
claimed: 2026-08-25T22:04Z
build: none
waiting_on:
evaluated: 2026-08-25T22:41:25Z
observed: 2026-08-25T22:41:37Z | Michael flew the map and judged the forest: he found trees standing on top of the invisible barrier and a dense clump sitting among houses, drew on screenshots to show which, and both batches were removed. He then reviewed the coast attempts and marked in green the shoreline he wanted against the red lines my hull cut across open water.
scenario: Michael in the editor viewport over L_Tutorial_Island, looking at the placed treeline and boundary from above and up close, and annotating screenshots.
files: 
  - Content/Maps/L_Tutorial_Island.umap
---

## Goal

Michael drew the playable area on a screenshot (`TutorialIslandRaid.png`) and asked for everything
outside it to be cut off by impassable forest, with more trees inside as well.

## Generate

**He placed the boundary, I built through it.** Nine `AGSRaidMarker`s with `GroupId = Boundary`,
`OrderIndex` 0-8, running from (-40390, 72010) in an arc over the north and down to (7540, 52990) -
**an open polyline, not a loop**, because the west and south are water.

**892 trees along 1,014 m**: four staggered rows at +/-300 and +/-900 uu from the line, 450 uu spacing,
+/-170 uu jitter, four species mixed, scaled 0.85-1.45 with random yaw. Placed as foliage instances
rather than actors - the level already has an `InstancedFoliageActor` and 900 more actors on a level
that already carries 9,000 would be a poor trade.

**8 invisible wall segments**, because trees do not actually stop anyone - a treeline is a picture, and
the player walks between the trunks. Scaled `/Engine/BasicShapes/Cube`, `BlockAll`, shadows off,
`Hidden In Game` true. The white bands in the editor are these; they are invisible in play.

**640 trees inside** in six pockets, then cleared back off everything that matters: the village (77
removed), the fields (68), the two mills (26), the graveyard and the gate. Clearing AFTER scattering
rather than scattering around exclusion zones - one pass of "keep this clear" is easier to reason about
and to re-run than six hand-authored no-go shapes.

## Evaluate

Verified from a top-down capture and then by Michael, who found two things wrong and drew on them.

**"Delete these trees, they placed themselves on the physical boundary."** He was exactly right about
the cause and it is mine: I placed the interior trees AFTER the walls, and `add_foliage_instances`
traces to **collision**, not to terrain - so it found the barrier's top face. Four oaks were standing up
to **4,165 uu in the air** on the wall. Found by tracing the true ground with the walls in the ignore
list and comparing heights, so they were caught by rule rather than by eye; all four removed. Only four
were affected because the boundary treeline went down BEFORE the walls existed.

**"Delete these trees"** - a dense clump among houses. Identified without guessing: of the six interior
pockets, the one at (-4000, 45000) had **14 building objectives inside it** against 2-5 for every other.
Removed by species - that cluster used only `SM_VillageBirch_01`, so removing that one species in that
radius took my 55 trees and left the level's own oaks and birches untouched.

**Also caught before he saw it:** the wall shut 7 objectives outside the playable area, one of them
`GS_FarmField` - a REQUIRED type. The raid stayed winnable (the Field type needs one carrier and the
wheat field is inside) but the HUD would have listed an objective the player can walk to a wall and
never reach. Deleted on Michael's instruction; the six outside houses were left because they collapse
into the grouped `Houses 0/27` row and 27 of 61 reachable is comfortable.

## Refine

**The coast was attempted four times and abandoned, which is the honest outcome.** Contour-following the
shoreline wandered into the inlet ("you went into the gap"); a convex hull cut straight out over open
water (Michael marked those lines RED against the GREEN he wanted); scanning north from the south found
a DIFFERENT landmass entirely; scanning outward from the village produced a wobbly line full of holes
where river mouths cut through. Four algorithms, four wrong shapes. **The peninsula's edge is too
irregular to infer, and the method that works is the one that worked for the forest: Michael draws it
with markers.**

That question was then overtaken - see #308. If deep water drowns you, the water IS the boundary and
the coast needs no wall at all.

**Deliberately not done:** the treeline follows the markers as straight segments, so wherever the drawn
line curved, the forest cuts the corner. Bending it costs one more marker mid-segment, not a re-run.
