---
name: gs-ai-patrol-advancers
description: Goblin Siege — who is allowed to advance an ACF patrol index, the silent state/home gates that stop a patrol dead, and the collision geometry on waypoints that was the real cause (the every-other-waypoint skip was refuted in PIE).
globs: []
alwaysApply: false
---

# GS — ACF patrol: exactly one advancer

ACF has two ways to walk an `AACFAIPatrolComponent` along a spline, and they are **alternatives, not
layers**. Both end at `AACFAIController::TryGoToNextWaypoint` → `UACFAIPatrolComponent::TryGetNextWaypoint`,
which does `patrolIndex++` on **every** call (`ACFAIPatrolComponent.cpp:43-45`). Nothing dedupes them.
Goblin Siege currently runs both at once, plus a hand-rolled third. Read this before touching guard
patrols, `GSAIControllerBase`, `BT_Defender`, or any `GS_Road_*` spline.

`ai-framework` teaches `StartPatrolLoop` as *the* way to start a patrol and separately says to keep
ACF's BT tasks wired. That combination is this bug.

---

## 1 — The two ACF advancers

| Path | What starts it | What advances the index | ACF source |
|---|---|---|---|
| **Routine** (task-driven) | `UACFPatrolSplinePathTask` / `UACFFollowSplinePathTask` / `UACFRandomPatrolAroundPointTask` call `StartPatrolLoop` | `HandleMoveCompleted`, bound to `PathFollowingComponent->OnRequestFinished` | `ACFPatrolSplinePathTask.cpp:28-30`; `ACFFollowSplinePathTask.cpp:52-59`; `ACFRandomPatrolAroundPointTask.cpp:37-46`; binding at `ACFAIPatrolComponent.cpp:135-137`, call at `:124` |
| **Placed actor** (data-driven, no C++) | `UACFAIStateFragment::ApplyFragment` sets `DefaultState` and calls `AICont->SetPatrolPath(ResolvedPath)` — and never calls `StartPatrolLoop` | the BT task `BTTask_GoToNextWayPoint` (ACF plugin Content), behind a `BTDecorator_IsInAIState` in `ACFBT` | `Data/ACFAIStateFragment.cpp:22-34`; `AACFAIController::TryGoToNextWaypoint` at `ACFAIController.cpp:499-523` |

A grep over the whole ACF `Source` tree finds **zero** `StartPatrolLoop` callers outside those three
routine tasks and the component itself. That is the rule stated as evidence: ACF never starts the
loop for a placed patroller.

### What Goblin Siege does instead

- `AGSAIControllerBase::OnPossess` calls `Patrol->StartPatrolLoop(true)` unconditionally for any pawn
  with the component — `Source/GoblinSiege/AI/GSAIControllerBase.cpp:179-193` (#331).
- The guards simultaneously run `Content/AI/BT_Defender.uasset`, a node-for-node copy of `ACFBT`:
  one `BTTask_GoToNextWayPoint_C`, six `BTDecorator_IsInAIState`, `ACFUpdatePatrolBTService` +
  `ACFCheckRoutineBTService`, both `AIState.Patrol` and `AIState.Routine` present.
- `Content/AI/BTT_PatrolNextWaypoint.uasset` is a **third** advancer (§3).

**Symptom:** guards visit every *other* spline point. Each completed MoveTo fires `HandleMoveCompleted`
(index++ and a new `TargetLocation` written mid-task), then the BT's Patrol sequence restarts and its
`GoToNextWayPoint` increments again. On a 4-point `GS_Road` this reads as a guard bouncing between two
corners or cutting a diagonal — i.e. as a NavMesh or spline-authoring bug, not as a patrol bug.
(#331 measured 5 of 7 guards moving; nobody checked whether they hit *consecutive* points.
Confidence: partial — asset name-table evidence, not observed in PIE.)

> **REFUTED 2026-08-28 (#341).** Someone has now checked, in PIE, with both advancers live:
> `BP_CastleGuard01_C_2` walked WP1->WP2->WP0->WP1->WP2->WP0, and `GS_Guard_A1` alternated
> WP0<->WP1 for nine consecutive samples. **Guards do not skip points.** The `patrolIndex++`
> mechanism above is real in source, but it does not produce this symptom in practice. Do not
> rewrite `GSAIControllerBase` or migrate to `UACFAIStateFragment` on the strength of it.
>
> The real cause of the stalled patrols was **collision geometry sitting on the waypoints**:
> `GS_Junction_*` are `StaticMeshActor`s with QUERY_AND_PHYSICS collision at scale 4 (half-extent
> 200-240uu) placed ON the road points. Closest approach = orb radius + 68.6 capsule = 269-309uu,
> against a `BTTask_MoveTo` arrival need of ~79uu, so the move never completes and the index never
> advances - while `IsPatrolLoopActive()`, the state tag, `IsExecutingCommand()` and every other
> documented gate all read healthy. Fix: shrink the orbs, THEN re-seat the road points at that
> junction - shrinking drops the navmesh ~150uu and leaves the waypoints floating off-mesh.

**Fix — SUPERSEDED 2026-08-28, do not action.** This paragraph originally read: *"choose one
advancer. For placed guards the ACF-shaped answer needs no C++ at all — `UACFAIStateFragment`
(DefaultState + PatrolPath) on the character data asset, with `BT_Defender`'s Patrol branch as the
sole advancer. Then remove the `StartPatrolLoop` call."* It is kept for the record and because
`UACFAIStateFragment` is genuinely the ACF-shaped path if you ever rebuild this from scratch — but
it is **not a fix for anything currently observed**, because the symptom it targets does not occur
(see the REFUTED block above). Removing the `StartPatrolLoop` call would regress #339, which is the
only thing that seeds a placed guard's loop: with it removed, all 7 guards reported
`IsPatrolLoopActive() == false` and never patrolled at all.

---

## 2 — Two silent gates that make a live patrol look broken

`HandleMoveCompleted` returns early unless the AI state is `AIState.Patrol` or `AIState.Routine`
(`ACFAIPatrolComponent.cpp:111-116`), and again if `IsExecutingCommand()` (`:119-121`). The tag
literals are `ACFAITypes.h:454-459`.

`CurrentAIState` is seeded exactly once, by `SetCurrentAIState(DefaultState)` at
`ACFAIController.cpp:104` — **after three early returns**: non-`AACFCharacter` pawn (`:65-68`), no
`BehaviorTree` on the controller (`:73-76`), a BT with no `BlackboardAsset` (`:78-82`).

So: a controller that hits any of those returns has an empty state tag forever. `StartPatrolLoop`'s
binding is live, `IsPatrolLoopActive()` reports **true**, and every `HandleMoveCompleted` bails at
line 114. `ai-framework`'s "Patrol does nothing" row lists four causes and *"CurrentAIState is empty"*
is not one of them — check the state tag first.

Second gate: `SetCurrentAIState` early-returns when the tag is unchanged (`:340-342`), so
`UpdateLocomotionState` (`:381-387`, reads `LocomotionStateByAIState`) does not re-run on a no-op set.
`BP_GSAIController_Militia` authors only `AIState.Combat / Patrol / ReturnHome / Wait` — **`AIState.Routine`
is absent from that map** while `BT_Defender` branches on it, so a routine-driven patrol keeps whatever
gait it was last in.

---

## 3 — `BTT_PatrolNextWaypoint`: a clone that drops three guarantees

`Content/AI/BTT_PatrolNextWaypoint.uasset` calls `UACFAIPatrolComponent::TryGetNextWaypoint` **directly**
and writes a project-invented key `PatrolLocation`. Going through the controller wrapper instead
(`ACFAIController.cpp:499-523`) is what gives you:

1. a refusal while a command is executing (`:501-503`);
2. the destination written to ACF's own `TargetLocation` key via `SetTargetLocationBK` (`:517`);
3. `WaitTimeAtPoint` written to `CommandDuration` via `SetWaitDurationTimeBK` (`:518`) — which is what
   `BTTask_WaitBlackboardTime` in `ACFBT` consumes.

The clone drops all three, and advances `patrolIndex` from a third place. It is used by
`Content/AI/BT_Militia.uasset` (with `BB_Human`'s `PatrolLocation` key).

**Live hazard:** `GSAIControllerBase.h:26-29` asserts the Militia archetype's `BehaviorTree` is
CLEARED — but `Content/AI/DA_Race_Human.uasset` still carries the strings `/Game/AI/BT_Archer` and
`/Game/AI/BT_Militia`, and `DA_Race_Goblin.uasset` carries `/Game/AI/BT_Militia`. Which field owns
those paths cannot be read from raw bytes, so **open the DA in the editor before touching race data**.
If one is live, `GSAIControllerBase.cpp:146-155` calls `RunBehaviorTree` after ACF's `OnPossess`
already ran `StartTree` — the documented two-trees bug (see `gs-behaviour-tree-wiring`).

Preferred end state: delete `BTT_PatrolNextWaypoint` and `BT_Militia`, keep `BT_Defender`'s native
`BTTask_GoToNextWayPoint`.

---

## 4 — The home leash will pull a road patroller off its route

- `homeLocation = possPawn->GetActorLocation()` at `ACFAIController.cpp:99` — the **spawn point**.
  The spline never moves it.
- `bBoundToHome` defaults **true**, `MaxDistanceFromHome` defaults **8500.f** (`ACFAIController.h:82-87`).
- `UACFUpdateStateBTService::TickNode` checks every tick for a non-grouped AI: past that distance it
  calls `SetTarget(nullptr)` and `SetCurrentAIState(AIState.ReturnHome)` (`ACFUpdateStateBTService.cpp:53-60`),
  and `SetCurrentAIState(ReturnHome)` overwrites blackboard `TargetLocation` with `homeLocation`
  (`ACFAIController.cpp:354-356`).
- The only ACF code that moves home is `UACFRandomPatrolAroundPointTask` with
  `bSetHomeLocationToPatrolCenter` (`ACFRandomPatrolAroundPointTask.cpp:41-43`) and save restore
  (`ACFAIPatrolComponent.cpp:162-176`).

`GS_Road_*` are instances of ACF's `/AscentCombatFramework/Integrations/Actors/ACF_SplinePath_BP`
(`Content/Python/gs_roads.py:24-26`, deliberate). `BP_GSAIController_Militia` overrides neither
property, so 8500 stands. **Road length vs 8500 is NOT VERIFIED — `gs_roads.report()` needs an editor.**

Compounding: the moment the AI leaves `AIState.Patrol`, §2's gate stops the advance entirely. A soft
leash becomes a dead patrol. Symptom: a guard paces the first third of his road and turns around,
forever — which reads as a broken spline or a NavMesh gap.

Remedies: raise `MaxDistanceFromHome` on `BP_GSAIController_Militia`, or `SetHomeLocation` to the
spline midpoint, or `DisableReturnHomeCheck()`.

**Related hazard, same file:** `TryGetNextWaypoint` can return **true without writing `outLocation`**
when NavMesh projection fails and the spline point sits at the world origin
(`ACFAIPatrolComponent.cpp:49-54`) — a road point that fails to snap hands the AI an uninitialised
destination.

---

## 5 — Seed the loop from exactly one place

Same rule as §1, at the other end. `StartPatrolLoop` unbinds before rebinding, so the delegate is not
duplicated (`ACFAIPatrolComponent.cpp:133-137`) — but `bStartImmediately` fires `TryGoToNextWaypoint`
again (`:88-90`), which **consumes a waypoint**.

| Blueprint | BeginPlay `StartPatrolLoop`? | Controller `OnPossess` call? | Net |
|---|---|---|---|
| `BP_CastleGuard01` | yes (name table carries `StartPatrolLoop`, `WaitTimeAtPoint`) | yes | seeded twice — **"skips its first spline point" is UNVERIFIED, see below** |
| `BP_CastleGuard02` | no | yes | seeded once |

Two guard archetypes on the same road behave differently for a cause invisible in either Blueprint.
Confidence: partial (asset name-table evidence).

> **2026-08-28 (#341):** the skip half of this row rests on the same unverified reasoning as §1 and
> should be treated the same way — §1's skip prediction was refuted in PIE. Both `BP_CastleGuard01`
> and `BP_CastleGuard02` derived guards were watched walking consecutive waypoints. The
> double-seeding itself is real and worth tidying; the predicted *consequence* is not evidence. If you keep a seeding site, delete the **Blueprint**
one — it duplicates a C++ path and cannot be seen from the source tree. Note also that a BeginPlay
`StartPatrolLoop` returns silently if the controller has not possessed yet
(`ACFAIPatrolComponent.cpp:80-83`), with no log.

---

## 6 — Research provenance: FullExample cannot answer a patrol question

Do not re-run this search. `D:/UE_5.8/Unreal Projects/FullExample/Content` is the StylizedIsland
environment pack only (Collections, Developers, StylizedIsland, `__ExternalActors__`,
`__ExternalObjects__`). A name-table grep over all 1616 uassets for `ACFAIPatrolComponent`,
`ACFSplinePath`, `ACFAIRoutineComponent`, `ACFAIRoutineDataAsset`, `ACFAIGroupSpawner`,
`ACFSplineFollowerComponent`, `PathToFollow`, `ACFAIController`, `ACFCharacter` and `AIState` returns
**zero** files. (Technique validated: the same grep on `StylizedIsland/Blueprints/BP_Optimizer.uasset`
yields `BlueprintGeneratedClass`, `ActorBeginOverlap`, etc. The 693 `__ExternalActors__` hits for "NPC"
are base64 false positives.) `Config/DefaultGame.ini` is 12 lines, `DefaultEngine.ini` 101, with no
AIState / Patrol / Routine / Spline key; `DefaultEditor.ini` is empty.

The only patrol wiring in that install is in the **project-local plugin copy**:
`Plugins/Marketplace/AscentCombatFramework/Content/Blueprints/AI/Controllers/ACFNPCRoutineControllerBP.uasset`
(`ACFAIRoutineComponent`, `ACFSplineFollowerComponent`, `RoutineDataAsset`, `AIState.Routine`,
`AICommand.FollowSpline`), `ACF_NPCController_BP.uasset`, `AI/Routine/SO_Routine.uasset`,
`Characters/Fragments/ACF_AIRoutinesFragment_BP.uasset` — and `ACFNPCRoutineControllerBP` points at
`/Game/FullSample/Blueprints/AI/Routine/Data/DA_RoutineTask`, which does not exist in this checkout.

Read intended patrol wiring from ACF's own plugin Content (`ACFBT.uasset`,
`BTTask_GoToNextWayPoint.uasset`, `ACFNPCRoutineControllerBP`) and from `UACFAIStateFragment` in C++.
