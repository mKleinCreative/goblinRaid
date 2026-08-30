---
id: 331
title: Defenders never start patrolling: TargetLocation seeds to homeLocation so the first MoveTo is a no-op
agent: claude-acf
status: done
claimed: 2026-08-27T22:13Z
build: none
waiting_on: 
evaluated: 2026-08-28T23:14:48Z
observed: 2026-08-28T01:41:20Z | Root cause found and it is NOT the StartPatrolLoop gap. 5 of 7 guards share ACF_SplinePath_BP_C_13 whose point [2] (-782,32846,-396) does not project onto the navmesh (only resolves with a 500x500x1000 extent, 147uu lower). ACF TryGetNextWaypoint falls back to the RAW unreachable point and still returns true, so MoveTo fails, Result.IsSuccess() is false, HandleMoveCompleted early-returns and the index deadlocks forever. The other 2 guards, on fully navigable roads, orbit waypoint [0] at a pinned 309-311uu without converging despite a valid 264uu path.
scenario: PIE on L_Tutorial_Island, navmesh projection and find_path_to_location_synchronously per guard and per spline point
files: []
---

## Goal

Defenders never start patrolling: TargetLocation seeds to homeLocation so the first MoveTo is a no-op

## Report (not started - split out of #330)

**The symptom.** Defenders on ACF's `BT_Defender` sit still forever. Nudge one and it patrols
correctly and keeps going - the loop is self-sustaining once started.

**The mechanism, measured 2026-08-27.** `AACFAIController::OnPossess` writes
`TargetLocation = homeLocation` (`ACFAIController.cpp:101`). The patrol branch's `MoveTo` therefore
targets the spot the guard is already standing on, completes instantly or not at all, and
`BTTask_GoToNextWayPoint_C` - gated behind `IsCharacterNearPoint` - never replaces the value. Calling
`controller->TryGoToNextWaypoint()` once frees them: all 6 stuck guards immediately took real road
waypoints and 5 of 7 were `MOVING` on the next sample.

**Aggravated by placement.** Guards were auto-assigned their nearest `GS_Road_*`, and for some the
nearest is very far - `BP_CastleGuard02_C_2` is **7,245uu** from `GS_Road_12`. Even once started, its
patrol opens with a 7km walk to reach the route. Worth deciding whether guards should be placed on
their roads, or whether a guard far from any road should be a `Marker.GuardPost` instead of a
patroller.

**Options, cheapest first:**
1. **Asset/level:** place guards on or near the road they patrol. Fixes the 7km walk but NOT the
   self-start - a guard standing exactly on its first waypoint still has `TargetLocation == its own
   position`.
2. **BT:** add a decorator/service that calls `TryGoToNextWaypoint` when `TargetLocation` equals the
   pawn's location, or seed the patrol on entering `AIState.Patrol`. ACF ships
   `ACFUpdatePatrolBTService` - **check whether adding it to `BT_Defender`'s root already does this**
   before writing anything (it is referenced by `BT_Defender` but its behaviour was not verified).
3. **C++:** seed the first waypoint at possess in `AGSAIControllerBase::OnPossess`. Durable, but
   needs a build.

**Check option 2 first.** ACF probably already solves this; #330's lesson was that hand-built
workarounds around ACF were the source of the bugs.

---

## ACF service checked first, as instructed - it is NOT the seeder

`UACFUpdatePatrolBTService` **is already attached** to the patrol branch in `BT_Defender`, and it does
only one thing (`AIFramework/Private/BehavioralThree/ACFUpdatePatrolBTService.cpp`):

    const float distanceToTarget = FVector::Distance(CharOwner->GetActorLocation(),
                                                    aiController->GetTargetPointLocationBK());
    aiController->SetTargetPointDistanceBK(distanceToTarget);

It maintains `TargetLocationDistance`, which is what `IsCharacterNearPoint` reads. **It never seeds a
waypoint.** So option 2 as written in the report above is answered: adding the service is not the fix,
because it is already there.

**The branch, measured from `BT_Defender`:**

    [6] Sequence  dec=[IsInAIState]  svc=[ACFUpdatePatrolBTService]
        [0] MoveTo
        [1] WaitBlackboardTime
        [2] Sequence  dec=[IsCharacterNearPoint]
            [0] GoToNextWayPoint

**Leading hypothesis, NOT VERIFIED:** it is a Sequence, and `MoveTo` runs BEFORE the waypoint is
fetched. At possess, `TargetLocation == homeLocation == the pawn's own position`. If that first
`MoveTo` returns Failed rather than Succeeded, the Sequence aborts before `GoToNextWayPoint` ever
runs, the Selector falls to the idle `Wait` at [7], and the guard never patrols. Once a real waypoint
exists (one manual `TryGoToNextWaypoint`) `MoveTo` succeeds and the loop is self-sustaining - which
matches the observed behaviour exactly.

Against the hypothesis: UE's `UBTTask_MoveTo` normally treats `AlreadyAtGoal` as **Succeeded**, which
would make the branch work. So this needs watching in the BT debugger, not more reading. **Put a
breakpoint or watch node [6] in PIE and see which child fails.** That single observation decides it.

If confirmed, the fix is to reorder [6] so the near-point/GoToNextWayPoint sub-sequence runs first -
but note a Sequence aborts on a failed child, so it cannot simply be swapped; it likely needs the
fetch under a Selector or the decorator inverted.

> 2026-08-28T01:00Z Adopted by claude-acf (was unassigned). Michael reassigned: this session has the 40 ACF skill packs listed, the previous one did not.


## Generate

The ticket's own title is wrong and is kept only as the record of what we believed. Defenders did not
patrol for **two** reasons, neither of which was "TargetLocation seeds to homeLocation":

1. The patrol loop was never seeded for a level-placed guard - fixed in #339
   (`AGSAIControllerBase::OnPossess` calls `StartPatrolLoop`).
2. **The waypoints were physically unreachable.** `GS_Junction_*` markers are `StaticMeshActor`s with
   QUERY_AND_PHYSICS collision at scale 4 (half-extent 200-240uu) placed ON the road points, so a
   pawn's closest approach was orb radius + 68.6 capsule = 269-309uu against a `BTTask_MoveTo`
   arrival requirement of ~79uu. The move could never complete, `HandleMoveCompleted` never fired and
   the index froze - fixed in #341 (all 18 orbs shrunk, 33 road points re-seated on the navmesh).

Two further defects were found and fixed on the way: a road waypoint sitting on top of
`SM_Gravestone_B2`, and `MaxDistanceFromHome` at ACF's default 8500 recalling guards mid-patrol
(raised to 14000 on `BP_GSAIController_Militia`).

## Evaluate

**Observed:** all 7 guards patrol. Travel over ~200s of PIE: 26,894 / 31,864 / 31,897 / 33,263 /
34,719 / 45,725 / 46,962 uu. Waypoint order is consecutive and wrapping, at the ACF Walk/Jog bands.
Before any of this, the waypoint index did not change once in four minutes and five of seven guards
never moved at all.

**Four hypotheses were eliminated by experiment before the real cause was found**, and that is worth
recording because each looked plausible: our separation steer (`GS.Combat.Separation 0` - no change),
ACF's crowd follower (`suspend_crowd_steering` - no change), ACF's documented state/command gates
(all green: loop active, state Patrol, `IsExecutingCommand()` false), and the near-point decorator
(`PointReachedDsitance` trialled at 500 in memory - no change). The cause was geometry, and Michael
found it by looking at the screen.

## Refine

Nothing here was reverted. The one thing deliberately left undone is the **group-arrival shape**: the
five `GS_Guard_*` are village guards 5.6-8.7km from the road network, so they were given authored
perimeter arcs rather than a shared road. Four of five patrol those arcs; the fifth and the question
of whether guards should share a route at all is a design call, not a defect. Also left: the navmesh
`AgentRadius` was 35 against a 68.6 capsule, which is why AI walked into walls; now 70/360.
