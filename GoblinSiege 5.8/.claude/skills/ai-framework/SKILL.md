---
name: ai-framework
description: Create, configure and wire NPC AI in ACF — behavior-tree controllers, combat behaviour, threat management, patrol/splines, group spawners and wave systems.
globs: []
alwaysApply: false
---

# AI Framework — ACF Ultimate

The **AIFramework** module is the full NPC brain for ACF. Each NPC is an `AACFCharacter` possessed by an `AACFAIController`, which drives an Unreal **Behavior Tree** through a Blackboard and a set of ACF components: `UACFCombatBehaviourComponent` (how it fights), `UACFThreatManagerComponent` (who it hates most), `UACFAIPatrolComponent` (where it walks), and `UACFCommandsManagerComponent` (player/lead orders). Crowds are organized by `UACFGroupAIComponent` on an `AACFAIGroupSpawner`, and arena/horde encounters are driven by `UACFAIWavesMasterComponent` with `FWaveConfig` waves. This skill covers building a new NPC, its combat behaviour, patrols, groups and waves.

Useful reference docs in the repo: `Docs/AIFramework_Wiki.md` and `Docs/ACFThreatManager_Wiki.md`.

---

## 1 — Understand the assets

| Asset | Class | Location (sample) | Purpose |
|---|---|---|---|
| Sample NPC character | `AACFCharacter` (BP subclass) | `/Game/FullSample/` | The possessed pawn (mesh, stats, equipment, AI controller class) |
| AI Controller | `AACFAIController` (BP subclass) | `/Game/FullSample/` (assigned on the NPC) | Possesses the NPC, owns the Behavior Tree + AI components |
| Behavior Tree | `UBehaviorTree` | `/AscentCombatFramework/` AI content | Decision tree assigned on the controller's `BehaviorTree` |
| Combat behaviour | `UACFCombatBehaviorDataAsset` / `UACFBaseCombatBehaviorDataAsset` | sample AI folders | DataAsset of conditional combat actions (`ActionByCondition`) |
| Character DataAsset | `UACFCharacterDataAsset` | sample character configs | Stats, starting items, AI config consumed by spawners |
| Group spawner | `AACFAIGroupSpawner` | `/Game/FullSample/` | Pawn that spawns and coordinates a group/team |
| Spline patrol path | `AACFSplinePath` | placed in level | Waypoints for `EFollowSpline` patrols |
| AI routine | `UACFAIRoutineDataAsset` | sample routines | Time-of-day task schedule (`FAIRoutineTask`) |

> **Never edit sample assets.** Duplicate the sample NPC, controller, behaviour DataAsset and behavior tree into your own folder (e.g. `Content/YourGame/AI/`) and customize **your copies**. Editing the originals means your work is lost on the next plugin/sample update.

### Key classes

| Class | Role |
|---|---|
| `AACFAIController` | Possesses the NPC; owns BT + Blackboard, targeting, threat, combat behaviour, commands, patrol |
| `UACFCombatBehaviourComponent` | Picks combat actions/states; resolves ideal distance and melee range; equips weapon on engage |
| `UACFThreatManagerComponent` | Tracks per-actor threat; raises `OnNewMaxThreateningActor` when the top threat changes |
| `UACFAIPatrolComponent` | Patrol loop in `EFollowSpline` or `ERandomPoint` mode |
| `UACFCommandsManagerComponent` | Executes commands like FollowMe / GoThere (companions) |
| `UATSAITargetComponent` | The AI's targeting component (from the Targeting System) |
| `AACFAIGroupSpawner` | Spawns a coordinated group, assigns team via `UACFTeamComponent` |
| `UACFGroupAIComponent` | Group brain: membership, formations, lead |
| `UACFAIWavesMasterComponent` | Wave/horde controller driving `FWaveConfig` waves |
| `AACFSplinePath` | Spline waypoint actor for patrols |
| `UACFAIRoutineComponent` / `UACFAIRoutineDataAsset` | Daily schedule of `UACFTask` entries by `FRoutineTime` |

### Important enums / structs

- `EAICombatState`: `EMeleeCombat`, `EChaseTarget`, `ERangedCombat`, `EStudyTarget`, `EFlee`.
- `EPatrolType`: `EFollowSpline`, `ERandomPoint`.
- AI state tags (string): `AIState.Patrol`, `AIState.Combat`, `AIState.ReturnHome`, `AIState.FollowLead`, `AIState.Wait`, `AIState.Routine`.
- `FWaveConfig`: `GroupSpawner`, `WaveAgentsOverride` (`FAISpawnInfo[]`), `secondsDelayToNextWave`.

---

## 2 — Setup / Configuration

### A. Create your NPC and AI controller

1. **Duplicate** a sample NPC `AACFCharacter` Blueprint and a sample `AACFAIController` Blueprint into `Content/YourGame/AI/`.
2. On your NPC Blueprint, set the **AI Controller Class** to your duplicated controller, and **Auto Possess AI** = `Placed in World or Spawned`.
3. On your controller Blueprint, assign `BehaviorTree` to your duplicated Behavior Tree asset.
4. Set `DefaultState` (e.g. the `AIState.Patrol` tag) and fill `LocomotionStateByAIState` to map each AI state tag to an `ELocomotionState` (Walk/Jog/Run).

### B. Configure perception & combat ranges on the controller

On the duplicated `AACFAIController` defaults:
- `bIsAggressive` — attack enemies as soon as they are perceived.
- `bShouldReactOnHit` — play a hit reaction when damaged.
- `LoseTargetDistance` — distance at which the AI drops its current target.
- **Home**: `bBoundToHome` + `MaxDistanceFromHome` — return home when wandering too far.
- **Teleport** (for companions/large maps): `bTeleportToLead`, `TeleportToLeadTriggerDistance`, `TeleportNearLeadRadius`.

### C. Create the combat behaviour DataAsset

1. **Duplicate** a sample `UACFCombatBehaviorDataAsset` into `Content/YourGame/AI/`.
2. Populate `ActionByCondition` (`FConditions`): each entry pairs a `UACFActionCondition` (e.g. `UACFDistanceActionCondition`) with an action tag and a trigger chance. This is the modern, condition-driven path.
3. Assign it on the NPC's `UACFCombatBehaviourComponent.CombatBehaviour`, and set the default action tags `EquipMeleeAction`, `EquipRangedAction`, `EngagingAction`.

> The `CombatStatesConfig`, `ActionByCombatState` and `DefaultCombatBehaviorType` fields are marked **DEPRECATED** — prefer `ActionByCondition` on the behaviour DataAsset for new content.

### D. Threat tuning

On `UACFThreatManagerComponent`:
- `DefaultThreatMap` — `TMap<TSubclassOf<AActor>, float>` initial threat per actor class perceived.
- `ThreatMultipliersByActor` — per-class multiplier applied to threat added.

### E. Patrol setup

1. Add a `UACFAIPatrolComponent` to the NPC (sample NPCs already have one).
2. Set `PatrolType`:
   - `EFollowSpline` → place an `AACFSplinePath` in the level and assign it to `PathToFollow` (or to `FAISpawnInfo.PatrolPath` in a spawner).
   - `ERandomPoint` → set `RandomPatrolRadius` around the AI's home.
3. Set `WaitTimeAtPoint` (seconds to idle at each waypoint).

---

## 3 — Core Workflow / Runtime API

### Targeting & state (on `AACFAIController`)

```
AICtrl->SetTarget(TargetActor);        // force a target
AActor* T = AICtrl->GetTarget();       // current target
bool bHas = AICtrl->HasTarget();
AICtrl->RequestAnotherTarget();        // ask threat/targeting for a new one
bool bBattle = AICtrl->IsInBattle();

AICtrl->SetCurrentAIState(StateTag);   // e.g. AIState.Combat
AICtrl->ResetToDefaultState();
AICtrl->SetCombatStateBK(EAICombatState::EChaseTarget);
```

### Threat (on `UACFThreatManagerComponent`)

```
Threat->AddThreat(Attacker, 50.f);
Threat->RemoveThreat(Attacker, 20.f);
AActor* Top = Threat->GetActorWithHigherThreat();
Threat->RemoveThreatening(DeadActor);
// React to a new top threat:
Threat->OnNewMaxThreateningActor.AddDynamic(this, &MyClass::HandleNewTopThreat);
```

### Combat behaviour (on `UACFCombatBehaviourComponent`)

```
Behaviour->TryExecuteConditionAction();                 // evaluate ActionByCondition
Behaviour->TryExecuteActionByCombatState(EAICombatState::EMeleeCombat);
bool bMelee = Behaviour->IsTargetInMeleeRange(Target);
float dist  = Behaviour->GetIdealDistanceByCombatState(EAICombatState::ERangedCombat);
Behaviour->SetCombatBehaviour(MyBehaviourDataAsset);    // swap behaviour at runtime
```

### Patrol (on `UACFAIPatrolComponent`)

```
Patrol->SetPatrolType(EPatrolType::EFollowSpline);
Patrol->SetPathToFollow(MySplinePath);
Patrol->StartPatrolLoop(true);   // bind to controller move-completed and start
Patrol->StopPatrolLoop();
```

### Groups (on `AACFAIGroupSpawner` / `UACFGroupAIComponent`)

```
int32 n = Spawner->GetGroupSize();
AACFCharacter* near = Spawner->GetAgentNearestTo(Location);
FAIAgentsInfo agent;
Spawner->GetAgentWithIndex(0, agent);
FGameplayTag team = Spawner->GetCombatTeam();
```

### Waves (on `UACFAIWavesMasterComponent`) — **call server-side**

```
Waves->StartWave();                                  // begin from first wave
Waves->AddAgentToWave(0, EnemyClass, 3);             // server RPC
Waves->ProceedToNextWave();                          // server RPC (auto on wave end)
int32 idx = Waves->GetCurrentWaveIndex();
// Events:
Waves->OnWaveStarted / OnWaveEnded / OnWaveProgressed / OnAllWavesEnded
```

---

## 4 — Wire to Characters / Blueprints

1. **NPC → controller**: NPC Blueprint `AI Controller Class` = your duplicated `AACFAIController`; the controller auto-creates its BT/Blackboard/combat/threat/targeting/commands/patrol components on possess.
2. **Behavior Tree → services**: the BT uses ACF services (`ACFUpdateStateBTService`, `ACFUpdateCombatBTService`, `ACFUpdatePatrolBTService`, `ACFCheckActionsBTService`) and tasks (`ACFPatrolSplinePathTask`, `ACFFollowSplinePathTask`, `ACFRandomPatrolAroundPointTask`). Keep these wired when duplicating — they write/read the Blackboard keys the controller manages.
3. **Hit/death reactions**: bind `AACFAIController.OnDamageReceived` / `OnPawnDeath`, or rely on `bShouldReactOnHit`. The controller internally feeds damage into the threat manager.
4. **Groups**: place an `AACFAIGroupSpawner`, configure its agents and `UACFTeamComponent` team tag, set `bSpawnOnBeginPlay` (or call spawn from BP). Spawned members get a `GroupOwner` and a `GroupIndex` for formations.
5. **Waves**: put a `UACFAIWavesMasterComponent` on a manager actor; fill `Waves` (`FWaveConfig`) referencing your group spawners (or `WaveAgentsOverride`), then call `StartWave()` from the server (e.g. a trigger volume).
6. **Routines (towns/villagers)**: add `UACFAIRoutineComponent`, assign a duplicated `UACFAIRoutineDataAsset` with `FAIRoutineTask` entries (a `FRoutineTime` + a `UACFTask`); the AI switches to the `AIState.Routine` state to run them.

---

## 5 — Verify

**Checklist before testing:**

- [ ] NPC, AI controller, behavior tree and combat-behaviour DataAsset are **duplicates** under `Content/YourGame/AI/`.
- [ ] NPC `AI Controller Class` points to your controller and **Auto Possess AI** is set.
- [ ] Controller `BehaviorTree` is assigned and `DefaultState` is a valid `AIState.*` tag.
- [ ] `LocomotionStateByAIState` has an entry for each state the AI uses.
- [ ] AI Perception is configured (sight/hearing) and `bIsAggressive` matches intent.
- [ ] Combat behaviour `ActionByCondition` has at least one valid condition+action, and `EquipMeleeAction`/`EngagingAction` tags are set.
- [ ] Threat `DefaultThreatMap` lists the player/enemy classes so perception generates threat.
- [ ] Patrol: spline assigned for `EFollowSpline`, or `RandomPatrolRadius > 0` for `ERandomPoint`.
- [ ] Group spawner has a valid team tag and agent list; waves are started **server-side**.

**Common failures:**

| Symptom | Fix |
|---|---|
| NPC spawns but never moves/acts | No `BehaviorTree` assigned, or BT not started — check controller `BehaviorTree` and Auto Possess AI |
| AI sees the player but never attacks | `bIsAggressive = false`, missing AI Perception, or empty `DefaultThreatMap` so no threat is generated |
| AI attacks but stands still / wrong range | Combat behaviour distances misconfigured — verify `GetIdealDistanceByCombatState` / melee range via the behaviour DataAsset |
| Patrol does nothing | `StartPatrolLoop` never called, wrong `PatrolType`, missing `AACFSplinePath`, or `RandomPatrolRadius = 0` |
| AI keeps running back home mid-fight | `MaxDistanceFromHome` too small with `bBoundToHome` — raise it or disable via `DisableReturnHomeCheck()` |
| Waves never start / only host sees enemies | `StartWave()` / `AddAgentToWave` must run on the **server**; verify authority |
| Whole group shares one target oddly | Threat/targeting is group-aware — confirm each member has its own threat component and correct team tag |

---

## Goblin Siege addendum (2026-08-27)

Corrections and additions for this project. Full detail in the `gs-*` packs.

**Patrol has two mutually exclusive advancers — §2.E and §4.2 above let you wire both.**
`StartPatrolLoop` (bound to move-completed) and the BT's `BTTask_GoToNextWayPoint` both reach
`TryGetNextWaypoint`, which does `patrolIndex++` per call (`ACFAIPatrolComponent.cpp:43-45`). ACF has
**zero** `StartPatrolLoop` callers outside the three routine tasks; for a placed patroller the shipped
answer is `UACFAIStateFragment` (`Data/ACFAIStateFragment.cpp:22-34`, sets DefaultState + patrol path,
never starts the loop) with the BT as the sole advancer. GS runs both plus a hand-rolled third —
`gs-ai-patrol-advancers`.

> **CORRECTED 2026-08-28 (#341, #340).** The MECHANISM above is real and verified in ACF source, but
> the SYMPTOM it predicts — "guards visit every OTHER spline point" — **does not occur, and was
> refuted by direct observation in PIE.** With `StartPatrolLoop` on possess AND the BT's
> `GoToNextWayPoint` both live, `BP_CastleGuard01_C_2` walked WP1->WP2->WP0->WP1->WP2->WP0, and
> `GS_Guard_A1` later alternated WP0<->WP1 for nine consecutive samples. No skipping. Do NOT rewrite
> `GSAIControllerBase` or migrate to `UACFAIStateFragment` on the strength of this claim. The
> original pack labelled its own confidence "partial - asset name-table evidence, not observed in
> PIE"; that caveat was dropped when it was restated here.

**The real reason ACF patrols stall in this project is PHYSICAL, not logical (#341).** `GS_Junction_*`
markers are `StaticMeshActor`s with **QUERY_AND_PHYSICS collision** placed at **scale 4** (half-extent
200-240uu) directly on road waypoints. A pawn's closest approach is orb radius + capsule radius 68.6 =
**269-309uu**, against a `BTTask_MoveTo` arrival requirement of ~79uu (`AcceptableRadius` 10 + agent
radius). The move can therefore never complete, `HandleMoveCompleted` never fires, and the index
freezes — while `IsPatrolLoopActive()` is true, the state is `AIState.Patrol`, `IsExecutingCommand()`
is false and every documented gate reads green. Symptom: guards circle a waypoint at a fixed radius.
Four unrelated hypotheses (separation steer, crowd steering, ACF's state gates, the near-point
decorator) were each eliminated by experiment before the geometry was checked. **Check for collision
geometry sitting on the waypoint before debugging the AI.**

> Shrinking such an obstacle LOWERS the navmesh under it (it had been generated over the sphere), so
> the waypoints are then left floating and stop projecting — `tgtOnNav=False`, `path=False`, move
> status `IDLE`, every guard frozen at spawn. **Shrinking an orb requires re-seating the road points
> at that junction.**

**"Patrol does nothing" has a fifth cause the table omits: `CurrentAIState` is empty.**
`HandleMoveCompleted` returns early unless the state is `AIState.Patrol` or `AIState.Routine`
(`ACFAIPatrolComponent.cpp:111-116`), and `SetCurrentAIState(DefaultState)` (`ACFAIController.cpp:104`)
sits after three early returns. `IsPatrolLoopActive()` still reports true. Check the state tag first.

**"AI keeps running back home" is also a PATROL symptom.** Home is the **spawn point**
(`ACFAIController.cpp:99`) and a patrol spline never moves it; `MaxDistanceFromHome` defaults **8500**
(`ACFAIController.h:82-87`). Any road longer than that flips the AI to `AIState.ReturnHome`
(`ACFUpdateStateBTService.cpp:53-60`) — and leaving `AIState.Patrol` then stops the advance entirely.

**§4.2 is not sufficient for the combat services.** `ACFUpdateCombatBTService` lives only in the
`ACFCombatBT` **subtree**, reached via `BTTask_RunBehavior` from `ACFBT`. Flatten a tree and you keep the
services you can see while losing `CombatState` and `TargetActorDistance` forever —
`gs-behaviour-tree-wiring` §5.

**Never call `RunBehaviorTree` on an `AACFAIController` subclass** (nor ACF's own `SetBehaviorTree`).
ACF starts the tree with `StartTree` and never sets `BrainComponent`, so the engine allocates a **second**
`UBehaviorTreeComponent` — the #330 two-trees bug. Set the `BehaviorTree` UPROPERTY before possession, or
call `GetBehaviorThreeComponent()->StartTree(*NewTree)` — `gs-behaviour-tree-wiring` §1-§2.

**§4.4: a level-placed `AACFAIGroupSpawner` never spawns**, however `bSpawnOnBeginPlay` is set — the
guard at `ACFAIGroupSpawner.cpp:44-48` tests `GetOwner()`, and a placed actor has none. Call
`SpawnGroup()` yourself. `UACFGroupAIComponent` is also far more than formations: a
`MaxSimultaneousAgents` cap (default 20), spawn jitter and navmesh projection —
`gs-teams-damage-spawning` §1-§3.

**§3 targeting: `SetTarget` is not a blackboard write.** It also sets the targeting component (which is
what `GetTarget()`/`HasTarget()` read), enters `AICombat`, binds the target's `OnOwnerDeath`, and alerts
the group (`ACFAIController.cpp:622-674`). GS writes the key directly and lost all four —
`gs-behaviour-tree-wiring` §4.
