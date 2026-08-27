---
id: 329
title: Road network as splines: GS_Road/GS_Junction actors, ACF spline-following, and a Road Tools panel
agent: claude-crumble
status: done
claimed: 2026-08-27T01:08Z
build: none
waiting_on:
evaluated: 2026-08-27T21:11:09Z
observed: 2026-08-27T21:10:40Z | Michael drew the whole 25-road network and 18 junctions with the Road Tools panel, and defenders were seen walking GS_Road_12 and GS_Road_20 in PIE
scenario: PIE on L_Tutorial_Island with BP_CastleGuard01 patrolling assigned road splines, after the navmesh bounds were widened
files: 
  - Content/Python/gs_roads.py
  - Content/Python/gs_treeline.py
  - Content/Python/init_unreal.py
  - Content/EditorTools/EUW_GSRoadTools.uasset
  - Content/Maps/L_Tutorial_Island.umap
---

## Goal

Road network as splines: GS_Road/GS_Junction actors, ACF spline-following, and a Road Tools panel

## Generate

Michael: *"We need a system for the ai to detect and use the roads"*, then
*"let's draw a spline on the road to make things simple"*, then
*"We need the ability to draw junctions and mark where they are"*, then
*"is there a way for this to be a popup menu so I don't have to click and drag"*.

**THE ROADS ARE NOT DATA, AND PROVING THAT WAS THE WHOLE FIRST HALF.** Two candidate sources were
tested and both failed:

- **The 339 MI_Decal_Dirt decals are not a road.** Median nearest-neighbour spacing 67uu (stacked
  clumps of mud dressing), 23 disconnected components even linking anything within 1500uu, and the
  nearest decal to the village gate road is **9,734uu away**.
- **The paint layers do not separate road from field.** Tested properly against ground truth once
  Michael had drawn five roads: sampling ON GS_Road_4 versus 1500uu to the side of the SAME segment
  gave AutoLandscape=1.00 **in both places - identical**. GS_Road_2 gave Layer_04 0.824 on-road
  against 1.000 off. Nothing to threshold, so no mask, so no generated network.

**I also had to retract a reason I had already written into the code.** I claimed
`get_layer_weights_at_location` costs ~5 seconds per call and that this made sampling impossible.
That was FIRST-CALL WARMUP - four later calls took 202ms total. The conclusion held but the stated
reason was wrong, and it was wrong in a file the next agent would have trusted. Corrected in
`gs_roads.py`.

**THE ACTOR IS ACF's, ON PURPOSE.** `GS_Road_*` are `ACF_SplinePath_BP` instances because ACF
already ships AI spline following - `ACFFollowSplineCommandBP`, `ACF_NPCFollowSpline_BP`. Patrols
and reinforcement columns can follow a drawn road with **no new C++**, which matters with the build
gate shut. This is the third time this project has been told to check the vendor plugin first.

Delivered in `Content/Python/gs_roads.py`:

- `start_road` / `add_point` / `remove_last_point` - fly the road and drop points at the camera,
  tracing along the camera's AIM so looking at a spot puts the point on that spot.
- `mark_junction` / `auto_junctions` / `branch` / `roads_at` - junctions as real marked objects.
- `snap` / `check` - ground-snapping and network validation.
- **`EUW_GSRoadTools`**, a floating Editor Utility Widget with eight buttons, each wired to
  `PythonScriptLibrary::ExecutePythonCommand` on its OnClicked and reloading the module first, so
  command changes need no reopen. Built entirely from Python: widget factory, WidgetService for the
  layout and `bind_event`, then `create_node_by_key` + `set_node_pin_value` + `connect_nodes`.

## Evaluate

Michael drew the whole network. **25 roads, 236,455uu, 18 junctions, 0 unmarked meeting points.**

**TWO BUGS IN MY OWN VALIDATOR, BOTH FOUND BY THE DATA AND BOTH THE SAME MISTAKE - measuring the
wrong thing and believing the clean number:**

1. **`check()` reported ZERO junctions on a network with four.** It compared endpoint to ENDPOINT.
   A road that ends partway along another is a T-junction, and that is most of a real network.
   Comparing each endpoint against the nearest point on every other spline found all four.
2. **`snap()` moved road points but not the junction MARKERS.** `roads_at()` measures from the
   marker, so a marker left at its old buried Z read as far from roads directly beneath it, and
   **GS_Junction_9 silently dropped GS_Road_20**. The junction still looked fine in the listing -
   it just had one fewer exit than the road network actually has, which is exactly the kind of
   error a patrol would expose weeks later.

Five spline points sat off the ground, four of them BURIED 1,400-1,700uu, three at one welded
junction. `snap()` fixed all of them: 65 road points and 10 junction markers moved, then 0 off
ground and every junction reporting its full set of roads.

**Payoff, and it is the thing the drawing was for:** the treeline generator now treats every road
as a 700uu keep-out sampled at 350uu (695 boxes), so no future regenerate can plant in a road.
**30 trees standing on roads were removed.** A painted road is invisible to every trace, so before
this there was no mechanism that could have found them.

**NOT OBSERVED.** Nobody has watched an AI follow one of these splines - that is the actual claim
worth testing and it needs ACF wiring plus PIE. I also have not seen the panel's buttons fire; the
graph reads back correct and compiled, which is read-back, not observation.

**One thing I did on an assumption:** GS_Road_4's original 566uu gap to GS_Road_3 was welded shut as
a sloppy endpoint. If there is a real break in the road there, nothing in the data will say so.

## Refine

Changed after evaluation: junction detection is endpoint-to-nearest-point-on-spline, not
endpoint-to-endpoint; `snap()` carries junction markers; the stale 5-second sampling claim removed
from the module header.

Deliberately not done: no NavMesh area costs and no road-following patrol behaviour. Michael:
*"I only want the enemies wandering from the roads if absolutely necessary."* Reinforcement arrival
and patrol nodes are marker-driven, so road-following is not needed yet and was not built.

Worth flagging separately: **all five Marker.* tags exist but only Marker.HordeArrival has a
consumer** (`GSHordeSubsystem.cpp:307,1022`, and that is the PLAYER's horde). `Marker.PatrolNode`
and `Marker.GuardPost` are read by nothing, and every placed GSRaidMarker is untagged. The
placement is ahead of the systems, not behind them.

### Re-read before closing - what changed after the Evaluate above was written

**OBSERVED, and by use rather than inspection.** Michael drew the entire network with the panel:
**25 roads, 236,455uu, 18 junctions, 0 unmarked meeting points**. The fly-and-click flow
(`Add point at camera`) is what he actually used, not the drag-the-gizmo flow, which justifies
having built it.

**The roads were then consumed by something real**, which the original Evaluate could not claim:
defenders on `UACFAIPatrolComponent` walked `GS_Road_12` and `GS_Road_20` in PIE, one covering
2,181uu at 447 u/s. Picking `ACF_SplinePath_BP` as the road actor is what made that possible with no
new C++ - ACF's own patrol consumes it directly.

**A blocker found only because the roads existed:** `GEN_NavBounds_Village` covered 4,000x4,000uu
while the network spans ~778x705m, so **5 of 10 sampled road points had no navmesh at all**.
Resized, rebuilt, and all 113 road points now project onto navmesh; the original volume transform is
backed up at `Saved/GSTreeline/navbounds-original.json`. That was already on the NEXT board as
outstanding work for the horde and would have blocked any patrol approach.

**Still true and still not fixed:** the `UCrowdManager` never builds ("Unable to find RecastNavMesh
instance"), so ACF-controlled pawns have no local avoidance and bump into each other. Pre-existing -
first logged 2026-08-26 23:17, before any of this work.
