---
id: 341
title: Fix GS_Road C_13: spline points float above the navmesh so 5 guards deadlock on point [2]
agent: claude-acf
status: done
claimed: 2026-08-28T03:00Z
build: none
waiting_on: 
evaluated: 2026-08-28T23:14:55Z
observed: 2026-08-28T04:05:51Z | All 16 remaining junction orbs shrunk to scale 1 and the 26 road points the shrink stranded re-seated on the navmesh (zero off-mesh points remain). ALL SEVEN guards now patrol: Guard01 traversed WP6->WP0->WP1->WP3->WP5->WP6->WP0 across its 7-point road at vel 606 having been pinned at 293.3 forever, and Guard02 runs WP2->WP0->WP1->WP2. The five on GS_Road_12 continue alternating WP0<->WP1.
scenario: PIE on L_Tutorial_Island after shrinking all orbs, saving the level and letting the navmesh rebuild; 12-point poll of all 7 guards over ~100s
files: 
  - Content/Maps/L_Tutorial_Island.umap
  - Content/Python/gs_roads.py
---

## Goal

Fix GS_Road C_13: spline points float above the navmesh so 5 guards deadlock on point [2]

## Generate

`GS_Road_12` (`ACF_SplinePath_BP_C_13`) point [2] was placed **on top of `SM_Gravestone_B2`** -
a visibility trace from that XY lands at z=-415.7 on the gravestone, 127uu above the walkable
ground, while the navmesh sits at z=-542.5. ACF's `TryGetNextWaypoint` projects a waypoint onto
the navmesh and, when that fails, **falls back to the RAW spline location and still returns true**
(`ACFAIPatrolComponent.cpp:49-54`). So every guard on this road was handed an unreachable
destination, `MoveTo` failed, `Result.IsSuccess()` was false, `HandleMoveCompleted` early-returned
and the patrol index deadlocked forever.

Fix: re-seated point [2] on the navmesh, (-781.9, 32845.8, -395.7) -> (-776.9, 32900.9, -542.5).
Level saved. No C++ and no Blueprint change.

## Evaluate

**`gs_roads.snap()` is NOT the fix here and MUST NOT be run on this level.** Two reasons, both
measured in a dry run over all 25 GS_Road actors:

1. It would not have fixed this point. Its ground trace already calls z=-396 correct (delta -0),
   because the gravestone IS what the trace hits. `_ignore()` excludes foliage, ForestWall,
   Plane*, rivers and roads - not props.
2. It would BREAK four currently-navigable points by lifting them 1300-1600uu onto something high,
   taking them OFF the navmesh: `GS_Road_13`[2] (-288->1242), `GS_Road_14`[0] (-128->1242),
   `GS_Road_20`[6] (-139->1421, and that is C_22, a road guards actually use today), and
   `GS_Road_5`[1] (-1627->19).

Also found, not fixed (no guard uses it): `GS_Road_6` point [8] is off-navmesh too.

## Refine

Verified after the edit: all three C_13 points project onto the navmesh with the DEFAULT extent.
Observed in PIE (below): three of the five C_13 guards now walk consecutive waypoints.

Two findings that came out of the same session and are NOT this ticket:

- **The double-advance predicted by `.claude/skills/gs-ai-patrol-advancers` is REFUTED by
  observation.** `BP_CastleGuard01_C_2` visited WP1 -> WP2 -> WP0 -> WP1 -> WP2 -> WP0. Guards do
  not skip every other point. The pack's own confidence note ("partial - asset name-table
  evidence, not observed in PIE") was the honest one. #339 can stay as written.
- **`MaxDistanceFromHome` (ACF default 8500) is now the limiting defect.** `homeLocation` is the
  SPAWN point, and several of these guards spawn 4700-5600uu from the road they are assigned. The
  home distance was watched climbing 128 -> 1391 -> 2860 -> 4192 -> 5501 -> 6322 -> 8510, and at
  8510 the guard is pulled to `AIState.ReturnHome` mid-patrol. `BP_CastleGuard02_C_2` sits pinned
  at home=8511 flipping Patrol/ReturnHome and never moves. This is exactly section 4 of
  `gs-ai-patrol-advancers`, whose "road length vs 8500 is NOT VERIFIED" is now verified.


## Follow-up 2026-08-28: home leash raised (Michael chose option 1)

`BP_GSAIController_Militia` **MaxDistanceFromHome 8500 -> 14000**, compiled and saved. 8500 was
never overridden - it was ACF's own default inherited straight through. Sized from measurement:
home->farthest-waypoint is 8689 for PLACED_BP_CastleGuard01 (which is why it tripped at 8500) and
12134 worst case across all seven, so 14000 clears every case with margin.

NOTE: this file is also claimed by #334. Both tickets are claude-acf and Michael authorised the
change directly, so it was made here rather than reopening #334.

**OBSERVED:** the recall is gone. All 7 guards now stay in `AIState.Patrol` past home=12000
where they were previously yanked to `ReturnHome` at 8510.

**BUT the patrol still does not run**, and this is now the whole remaining problem. Michael's
words: *"they seem to want to orbit around whatever patrol point, which looks like a strange
ritualistic dance"*. Measured: every guard travels the full 5000-8600uu to its waypoint and then
parks **300-310uu short of it**, milling, and the index never advances again.

Three candidate causes ELIMINATED by experiment, all negative:

1. **Our separation steer** - `GS.Combat.Separation 0` at runtime: distance held 290-345uu,
   no change. Not `UGSAISteeringComponent::TickSeparation`'s `AddMovementInput`.
2. **ACF's crowd follower** - `suspend_crowd_steering(True)` on all 7 controllers: no change.
   Capsule radius is 68.6uu, so 300 is not the body either.
3. **ACF's own documented gates** - all green on all 7: `IsPatrolLoopActive()=true`,
   `IsExecutingCommand()=false`, state=`AIState.Patrol`, blackboard `Paused`=false.

LEADING HYPOTHESIS, NOT TESTED: the waypoint advances we DID see earlier (WP1->WP2->WP0) were
happening while the 8500 leash was still churning guards through ReturnHome->Patrol transitions.
Raising the leash removed that churn - so the state transitions, not normal move-completion, may
have been what re-kicked the patrol all along. Cheap test: put one guard's
`SetMaxDistanceFromHome` back to 8500 at runtime and see if it starts advancing again.

Next place to look regardless: `BT_Defender`'s MoveTo node AcceptableRadius (a value near 300
would explain the park distance exactly). That needs the BT asset opened, which this session
did not do.


## The orbit: Michael's menace hypothesis tested, and where the gate actually is

Michael asked whether the guards were stuck in a menace loop around the junction. Tested and
**ruled out**, for two independent reasons:

- `UBTTask_MenaceOrbit::TickTask` calls `FinishLatentTask(Succeeded)` the moment `TargetActor` is
  invalid (`BTTask_MenaceOrbit.cpp:111-115`), and `TargetActor` reads **None** on all 7 guards.
  `ExecuteTask` refuses for the same reason (`:68-70`). The task cannot persist.
- It orbits `Target->GetActorLocation()` - the target ACTOR - not the station key.

The `OrbitRadius = 300.f` (`BTTask_MenaceOrbit.h:70`) matching the measured 300uu park distance is
a coincidence. Worth recording because it is a convincing one.

Also disambiguated a measurement this session had been conflating: blackboard `TargetLocation` is
**exactly** the patrol waypoint (`TL->nearestWP = 0.0` for all 7), NOT a ring/standoff point. So
nothing is overwriting the waypoint. The guards simply stop ~300uu short of a correct target.

**The gate is `BTDecorator_IsCharacterNearPoint`.** Walking `BT_Defender`, the patrol branch is:

    Sequence
      <ACFUpdatePatrolBTService>
      BTTask_MoveTo             -> TargetLocation
      BTTask_WaitBlackboardTime -> CommandDuration (2.0)
      [BTDecorator_IsCharacterNearPoint_C]        <-- gates the advance
      Sequence
        BTTask_GoToNextWayPoint_C

`GoToNextWayPoint` only runs when that decorator passes. The guard parks ~300uu out, the decorator
reads "not near", the advance never fires and the index freezes - which is every symptom, including
the milling (the branch keeps re-running MoveTo against a goal it thinks it has already reached).

The decorator is ACF's own
(`/AscentCombatFramework/Blueprints/AI/Tasks/BTDecorator_IsCharacterNearPoint`), and its threshold
is computed in the Blueprint graph - not readable as a CDO property, so **the actual number is NOT
yet known**. NEXT STEP: open `BT_Defender`, read that decorator's distance and the MoveTo
`AcceptableRadius` together, and make the near threshold >= where MoveTo actually stops.


## The decorator edit was authorised, trialled, and NOT SHIPPED - it does not fix it

Michael said to edit `BT_Defender`'s near-point decorator. Two things came out of trying:

**The threshold is `PointReachedDsitance = 200.0`** (ACF's own typo; the property is only
reachable from Python by its EXACT name - `get_editor_property("PointReachedDsitance")`. The
snake_case form fails). Guards park at 290-390uu, so 200 can indeed never pass. It looked like the
answer.

**It cannot be overridden per-instance**: the variable is `is_public: False, is_exposed: False`, so
`set_editor_property` on the `BT_Defender` node instance is refused with *"cannot be edited on
instances"*. The only ways to change it are editing the ACF plugin asset (marketplace content, lost
on the next ACF update, and it would also change ACF's own `ACFBT`) or duplicating the decorator
into `/Game/AI/` and swapping it in.

**So it was trialled IN MEMORY first** - CDO set to 500.0, never saved, PIE restarted. Result over
90s: **no change at all.** Still parked 290-325uu, still WP0, no advance. Value reverted to 200.0;
`get_dirty_content_packages()` is empty, nothing was written.

**Why it cannot work:** `BTTask_MoveTo` has `AcceptableRadius = 10.0` with both agent- and
goal-radius reach tests on, so arrival needs roughly 10 + 68.6 = ~79uu. And the live move status is
`PathFollowingStatus.MOVING` on all 7 guards with velocity 240-270 - **the move never completes.**
The sequence therefore never reaches the decorator at all; the gate is upstream of it.

The real defect is that the pawns path toward a reachable goal and converge to a ~300uu standoff
they never close, while path following still reports MOVING. Four causes are now eliminated by
experiment: our separation steer, ACF's crowd steering, ACF's command/pause/state gates, and the
near-point threshold. NOT yet examined: the CrowdFollowingComponent's own avoidance/pathing config
(`suspend_crowd_steering` suspends steering but the component still does the path following), and
whether the goal projects onto a navmesh poly the agent cannot enter at close range.


## SOLVED: the junction orbs were physically blocking the waypoints (Michael's call)

Michael asked whether the junction orbs were too large and shrank `GS_Junction_8` and `_11` to test
it. He was right, and this is the root cause of the ~300uu standoff that four of my own hypotheses
failed to explain.

`GS_Junction_*` are `StaticMeshActor`s with **QUERY_AND_PHYSICS collision**, placed at scale **4.0**
- a half-extent of 200-240uu. Every road waypoint that sits at a junction is therefore at the CENTRE
of a solid sphere the pawn cannot enter. Closest approach = orb radius + capsule radius 68.6, i.e.
269-309uu, against a MoveTo arrival requirement of ~79uu. The move can never complete, so
`HandleMoveCompleted` never fires and the index never advances. That is the whole "ritualistic
dance", and it is why raising `PointReachedDsitance` did nothing: the gate was physical, not logical.

The measured standoffs match the predicted geometry: Guard01 stalled at 293.3 against
`GS_Junction_14` (predicted 283), Guard02 at 247.8 against `GS_Junction_13` (predicted 307).

**Second-order effect, and a trap:** shrinking an orb LOWERS the walkable surface at that junction
by ~150uu, because the navmesh had been generated over the sphere. The waypoints, still at the old
centre height, were then left floating and stopped projecting - `tgtOnNav=False`, `path=False`,
move status `IDLE`, and all seven guards froze at spawn having never moved. **Shrinking an orb
REQUIRES re-seating the road points at that junction.** Seven points were re-seated onto the
navmesh (roads 10/11/12 at junction 8, roads 15/16/20 at junction 11, plus the pre-existing
`GS_Road_6[8]`); zero off-navmesh points remain and the level is saved.

**OBSERVED after the re-seat:** the five `GS_Guard_*` on `GS_Road_12` patrol, alternating WP0 <-> WP1
cleanly across six samples at vel 606.3. `GS_Road_12`'s only junction is the one that was shrunk.
Guard01 and Guard02 remain stuck because `GS_Road_20` and `GS_Road_16` still pass through junctions
9/12/13/14, all still at scale 4.

REMAINING: 16 junction orbs are still scale 4. Either shrink them (then re-seat the road points at
each, per the trap above) or turn their collision off and leave them visible. Michael's call.


## Each guard its own road (2026-08-28) - DONE for 2 of 7, PARTIAL for the village 5

`.claude/skills/` un-ignored first (`.gitignore:86-99` rewritten) so the 9 `gs-*` packs and the
hand corrections in the ACF-derived packs are versioned instead of sitting one
`Register-ACFSkills.ps1` run from deletion.

**Straight reassignment is impossible and the measurement says so.** The five `GS_Guard_*` are not
near the road network at all: they are clustered at (446, 27894) - a village, 43 static meshes, 2
`GSBuildingObjective`, loot and livestock inside 4000uu - and the nearest road point is **5615-8674uu
away**. A greedy unique assignment over all 25 roads makes every guard worse, up to **20294uu**.
`PLACED_BP_CastleGuard01/02` already had their own roads (661 / 569uu) and were left alone.

So five new perimeter arcs were authored instead: ring R=1800 about the village centre (36/36 sample
points on navmesh at that radius; 94% at 2200, 86% at 3000), split into 5 x 72 degree arcs of 4
points each, every point navmesh-projected before use. `GS_Road_25..29`, one per guard, assigned by
nearest bearing.

**OBSERVED, and it is not a clean win.** Over ~130s: `GS_Guard_A2` and `GS_Guard_B2` patrol their
arcs at vel 606, cycling waypoints. `A1`, `A3` and `B1` **never move at all** - the closest-pair
distance held at exactly 173uu for the whole run. Their state reads healthy: `selfOnNav=True`,
`tgtOnNav=True`, `path=True`, `MOVE_WALKING`, move status `MOVING`, patrol loop active, nothing
overlapping the capsule - yet **velocity 0.0 and acceleration 0.0**, with the target only 209-289uu
away for A1/A3 and 1853uu for B1.

That is the same signature as the pre-orb standoff (~200-300uu, path following says MOVING, nothing
drives the pawn), and there are NO junction orbs on these new arcs. So the orb fix was real and
observed - all 7 patrolled their old roads after it - but it was not the whole mechanism. Something
else produces a standoff of the same magnitude in tight village space.

REVERSIBLE: the five guards' previous `PathToFollow` was `GS_Road_12` for all of them, and
`GS_Road_25..29` are new actors that can simply be deleted. On `GS_Road_12` all five did move.
