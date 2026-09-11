---
name: gs-behaviour-tree-wiring
description: Goblin Siege — never call RunBehaviorTree on an ACF controller, why a second tree wipes the one blackboard, and what SetTarget fans out to that a blackboard write does not.
globs: []
alwaysApply: false
---

# GS — behaviour trees and blackboards on an ACF controller

Every AI controller in Goblin Siege derives from `AACFAIController` (`GSAIControllerBase.h:41`) and
every pawn from `AACFCharacter` (`GSCharacterBase.h:47`), so ACF's `OnPossess` always runs first.
That one fact makes four ordinary-looking calls dangerous: `RunBehaviorTree`, ACF's own
`SetBehaviorTree`, a standalone Blackboard asset, and a direct write to the `TargetActor` key.
Read this before adding, swapping or flattening a BT, or before touching `BTService_AcquireTarget`.

None of this is in the 40 ACF packs: a grep for `RunBehaviorTree`, `StartTree`, `BrainComponent`,
`SetBehaviorTree`, `InitializeBlackboard` across all of them returns zero hits.

**Context from the shipping sample, added 2026-09-09 (#406).** The full ACF Ultimate sample content
is now installed at `/Game/FullSample/`, and it contains exactly **one** BehaviorTree —
`Blueprints/AI/Boat/BT_EnemyBoat`, driven by `ACFBoatAIController` — and **zero** Blackboard assets.
There is no humanoid BT, no humanoid BB, and no patrol asset anywhere in the sample's project
content. Every melee, ranged, gun, mage, defender, spider, zombie and companion AI in ACF's own
showcase runs on the `AIFramework` plugin defaults that `AACFAIController` sets up in `OnPossess`.

Two consequences for the rules below. First, they are load-bearing: GS hand-authors 14 BT/BB assets
on a controller whose vendor never intended a project-side tree, which is precisely the collision
§1 and §2 describe. Second, the note above — that none of this appears in the 40 ACF packs — now has
an explanation rather than just being an absence: the vendor documents no BT wiring because the
vendor authors no BTs.

---

## 1 — `RunBehaviorTree` allocates a SECOND BehaviorTreeComponent

`AACFAIController` creates its own `BehaviorTreeComp` and `BlackBoardComp` as subobjects
(`ACFAIController.cpp:53-54`) and starts the tree at `:106` with
`BehaviorTreeComponent->StartTree(*BehaviorTree)`. It **never writes `AAIController::BrainComponent`** —
grep for `BrainComponent` across the whole AIFramework module returns nothing.

The engine's `AAIController::RunBehaviorTree` (`Engine/Source/Runtime/AIModule/Private/AIController.cpp`)
casts `BrainComponent` to a `UBehaviorTreeComponent` and, when that is null, does
`NewObject<UBehaviorTreeComponent>(this)` + `RegisterComponent()` + `StartTree(..., Looped)`. Because
ACF left `BrainComponent` null, **that branch is always taken**.

ACF trips its own trap: `AACFAIController::SetBehaviorTree` (`ACFAIController.cpp:735-747`) does
`StopTree(EBTStopMode::Safe)` — deferred to the end of the current task — then `RunBehaviorTree`,
which builds the second component regardless.

| Symptom | Cause |
|---|---|
| Agent walks to one destination then immediately re-paths to another (#330, observed) | Two trees ticking one pawn and one movement component |

**Rules.** Never call `RunBehaviorTree` on an `AACFAIController` subclass. Never call ACF's
`SetBehaviorTree()` either. To choose a tree, set the `BehaviorTree` UPROPERTY
(`ACFAIController.h:67`) **before possession**; to swap one at runtime, call
`GetBehaviorThreeComponent()->StartTree(*NewTree)` directly.

GS call sites that break this rule: `GSAIControllerBase.cpp:148` (archetype tree) and
`GSHordeAIController.cpp:72` (`BT_HordeGoblin`). The #330 fix cleared the archetype tree rather than
removing the call — see `gs-ai-patrol-advancers` §3 for why `DA_Race_Human` may still make that branch fire.

---

## 2 — A second tree on a different Blackboard asset re-initialises the ONE blackboard

There is only ever **one** blackboard component on an ACF controller. `OnPossess` does
`BlackboardComponent->InitializeBlackboard(*bbData); Blackboard = BlackboardComponent;`
(`ACFAIController.cpp:84-85`) and then caches **ten integer key IDs** at `:88-97`: `TargetActor`,
`TargetLocation`, `TargetActorDistance`, `TargetLocationDistance`, `CommandDuration`, `GroupLeader`,
`CombatState`, `Paused`, `HomeDistance`, `GroupLeadDistance`. Every ACF setter uses the cached **ID**,
not the name — `SetTargetActorBK:326`, `SetCombatStateBK:403`, `SetTargetPointDistanceBK:454`,
`SetTargetActorDistanceBK:461`, `SetIsPausedBK:469`, `SetLeadActorDistanceBK:475`, `SetHomeDistanceBK:482`.

The engine's `UseBlackboard` does `Blackboard = FindComponentByClass<UBlackboardComponent>()` — it
**finds ACF's component** rather than making a new one — and then, if the asset differs,
re-`InitializeBlackboard`s it. `RunBehaviorTree` reaches that whenever
`!Blackboard->IsCompatibleWith(BTAsset->BlackboardAsset)`.

The failure is silent. `UBlackboardComponent::SetValue`
(`Engine/Source/Runtime/AIModule/Classes/BehaviorTree/BlackboardComponent.h:345-351`) returns `false`
with **no log** when the entry is missing or mistyped, and `FBlackboard::FKey` defaults to
`InvalidKey` (`BlackboardKey.h:29-33`, `uint16 Key = static_cast<uint16>(-1)`).

| Blackboard | Keys | Used by |
|---|---|---|
| ACF `ACFAIBB` (plugin Content `Blueprints/AI/`) | CombatState, ComboCounter, CommandDuration, GroupLeadDistance, GroupLeader, HomeDistance, PatrolLocation, Paused, SelfActor, TargetActor, TargetActorDistance, TargetLocation, TargetLocationDistance | `BT_Defender` |
| GS `BB_Human` (standalone, no ACFAIBB in its name table) | PatrolLocation, SelfActor, TargetActor, TargetIsAttacking, TargetLocation | `BT_Militia`, `BT_Archer` |
| GS `BB_HordeGoblin` (standalone) | Cargo, DeliveryLocation, FollowLocation, FollowSlot, FollowTarget, HordeState, OrderLocation, OrderSubject, OrderVerb, SelfActor, TargetActor, TargetIsAttacking, TargetLocation | `BT_HordeGoblin` |

Running a `BB_Human` tree over a controller whose ACF tree was `BT_Defender`/`ACFAIBB` re-initialises
the same component onto the five-key layout: every value wiped, and ACF's cached indices now point
into a different table. Worse than a clean failure — an index that lands inside the new layout with a
matching **type** writes the wrong key; where it misses, `SetValue` returns false and nothing logs.

**Do not author a GS blackboard from scratch.** Reparent it to `ACFAIBB`, or add GS keys to `ACFAIBB`
itself.

### Correction to a comment in our own source

`GSAISteeringComponent.h:151-155` says ACF's `BlackBoardComp` is "separate from the one
`AAIController::RunBehaviorTree()` sets up. After Phase 1 there may be two…", and
`GSAISteeringComponent.cpp:69-77` implements a `GetBlackboardComponent()`-then-`FindComponentByClass`
fallback against that imagined second component. **Both calls return the identical object**, so the
fallback is dead code that makes the component look defended when it is not. The duplication is in
the *BehaviorTreeComponent*; the blackboard damage is re-initialisation, not duplication.
`GSAIControllerBase.h:24-25` has the same mixed claim.

---

## 3 — An unset `BehaviorTree` is not "no tree" — it is no perception and no state

`ACFAIController.cpp:73-76` logs one Warning ("should be assigned with a behavior Tree") and
**returns**. Everything after is skipped: blackboard init (`:84`), key-ID caching (`:88-97`),
`homeLocation` capture (`:99`), `SetCurrentAIState(DefaultState)` (`:104`), `StartTree` (`:106`),
`PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(...)` + `RequestStimuliListenerUpdate`
(`:108-111`), and `ThreatComponent->OnNewMaxThreateningActor.AddDynamic(...)` (`:113-115`).

`Content/AI/BP_GSHordeAIController.uasset`'s name table contains only `CompanionBehaviorTree` and
`BT_HordeGoblin` — no `BehaviorTree` property and no ACF tree (contrast `BP_GSAIController_Militia`,
whose table has both `BehaviorTree` and `BT_Defender`). Every horde goblin therefore takes the early
return, and `GSHordeAIController.cpp:72` supplies a tree afterwards.

So the horde AI **looks functional** — it moves and fights — while ACF's `HandlePerceptionUpdated` and
`HandleMaxThreatUpdated` are permanently unbound and the AI state tag is unset, so every
`BTDecorator_IsInAIState` evaluates an empty tag. The only evidence is one Warning at spawn that reads
as boilerplate. If an ACF controller should run a tree, **assign ACF's `BehaviorTree`**.

---

## 4 — Writing `TargetActor` is not setting a target

`AACFAIController::SetTarget` (`ACFAIController.cpp:622-674`) does far more than write a key:

| Line | What it also does |
|---|---|
| `:623` | `SetTargetActorBK` (the blackboard key) |
| `:624` | `TargetingComponent->SetCurrentTarget(...)` — this, not the key, is what `GetTarget()` / `HasTarget()` read (`:677-687`) |
| `:632` | `SetCurrentAIState(ACF::AICombat)` |
| `:635-648` | binds `UACFDamageHandlerComponent::OnOwnerDeath` on the new target, unbinds the old |
| `:650-652` | `GroupOwner->SetInBattle(true, currentTarget)` |
| `:656` | `AlertTargetGroup(currentTarget)` |
| `:660-673` | on a null/friendly target: removes threat, `ResetToDefaultState` |

`Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.cpp:282` does only
`BB->SetValueAsObject(TargetKey.SelectedKeyName, Target)`. Selection is GS's own nearest-live-hostile
scan against `AGSCharacterBase::IsHostileTo`, with a `UGSEngagementComponent` ring-slot ledger
(`ReleaseAll` at `:234-238`) and a **polled** dead/out-of-range test (`:219-224`) throttled by
`ReacquireIntervalSeconds`. Wired into `BT_Militia`, `BT_Archer` and `BT_HordeGoblin`.

Consequences, all silent:

- `GetTarget()` / `HasTarget()` report **no target** to every other ACF system while the tree fights.
- The AI state never becomes `AICombat`, so ACF's `IsInAIState` combat branches and ACF's combat
  locomotion switch never open.
- The target's `OnOwnerDeath` is never bound, so death detection had to be re-implemented as a poll —
  which is exactly why a defender "stands over the body" long enough to need a special-case rescan
  (`BTService_AcquireTarget.cpp:242-248`). ACF's event would not have that lag.
- Neither `SetInBattle` nor `AlertTargetGroup` fires, so a squad does not aggro together.

**Route target changes through `AACFAIController::SetTarget`** and let the ring-slot ledger react to
it, rather than owning the key.

---

## 5 — The combat keys are written by one service, and it lives in a SUBTREE

`ACFBT.uasset` (ACF plugin Content) carries `ACFCheckRoutineBTService`, `ACFUpdatePatrolBTService` and
`ACFUpdateStateBTService`, plus a `BTTask_RunBehavior` whose subtree is `ACFCombatBT`.
**`ACFUpdateCombatBTService` is only in `ACFCombatBT`.** It is the sole writer of the combat keys:
`ACFUpdateCombatBTService.cpp:90` `SetTargetActorDistanceBK`, `:103` `SetCombatStateBK`, `:106`
`UpdateCombatLocomotion`, with disengage logic at `:47-52` and `:61-76`. Likewise
`ACFUpdatePatrolBTService.cpp:26-27` is the sole writer of `TargetLocationDistance`, and
`ACFUpdateStateBTService.cpp:32` / `:55` of `GroupLeadDistance` and `HomeDistance`.
(`ACFStaticBT.uasset` carries Combat + State in one tree; `AI/AIs/ACF_HorseBT.uasset` carries all three.)

`Content/AI/BT_Defender.uasset` faithfully copies `ACFBT`'s shape, including the `RunBehavior` into
`ACFCombatBT`. GS's other three trees contain **no ACF service at all**.

Two ways to break this with no error:

1. **Flattening `BT_Defender`** — inlining the combat branch instead of the `RunBehavior` — drops
   `ACFUpdateCombatBTService`. `CombatState` and `TargetActorDistance` then never change again: the AI
   holds whatever combat state it possessed with and never picks an attack. Obeying `ai-framework:170`
   as written ("keep these services wired") does not catch this, because the services you can *see* are
   still there.
2. **A hand-written tree on `ACFAIBB`** without the three services leaves every distance key at its
   possess-time value — and `TargetLocationDistance` is seeded to `MAX_FLT` at `ACFAIController.cpp:100`,
   so distance decorators evaluate against a constant.

Also: `ACFUpdateCombatBTService` and `ACFUpdateStateBTService` keep `aiController` / `CharOwner` /
`targetActor` as UPROPERTY **members without `bCreateNodeInstance`** (contrast
`ACFCheckRoutineBTService.cpp:14`, which sets it). A Blueprint subclass reading those BlueprintReadOnly
fields outside `TickNode` reads whichever agent ticked last, not its own.
