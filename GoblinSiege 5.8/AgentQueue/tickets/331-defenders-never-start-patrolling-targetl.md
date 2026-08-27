---
id: 331
title: Defenders never start patrolling: TargetLocation seeds to homeLocation so the first MoveTo is a no-op
agent: unassigned
status: queued
claimed: 2026-08-27T22:13Z
build: none
waiting_on:
evaluated:
observed:
scenario:
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
