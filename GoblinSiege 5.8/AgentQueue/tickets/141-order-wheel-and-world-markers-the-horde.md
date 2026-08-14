---
id: 141
title: Order wheel and world markers: the horde takes Attack/Hold/Loot/Follow
agent: claude-orders
status: done
claimed: 2026-08-12T18:10Z
build: required
waiting_on:
evaluated: 2026-08-13T03:18:04Z
observed: 2026-08-13T00:46:48Z | Michael watched the warband break off and drift straight back to heel within a third of a second of an Attack order, with no beacon ever appearing; he reported there was no way to designate anything to attack - every order resolved to bare ground
scenario: PIE in L_CombatArena, 8 horn-summoned goblins from two blasts, driving GS.Horde.Order Attack/Hold/Follow from the console with the crosshair on open ground
files: 
  - Source/GoblinSiege/Horde/GSHordeOrderTypes.h
  - Source/GoblinSiege/Horde/GSHordeOrderMarker.h
  - Source/GoblinSiege/Horde/GSHordeOrderMarker.cpp
  - Source/GoblinSiege/Horde/GSHordeCommandComponent.h
  - Source/GoblinSiege/Horde/GSHordeCommandComponent.cpp
  - Source/GoblinSiege/Horde/GSHordeSubsystem.h
  - Source/GoblinSiege/Horde/GSHordeSubsystem.cpp
  - Source/GoblinSiege/Horde/GSHordeAIController.h
  - Source/GoblinSiege/Horde/GSHordeAIController.cpp
  - Source/GoblinSiege/Horde/GSHordeGoblin.h
  - Source/GoblinSiege/Horde/GSHordeGoblin.cpp
  - Source/GoblinSiege/UI/GSHordeOrderWheelWidget.h
  - Source/GoblinSiege/UI/GSHordeOrderWheelWidget.cpp
  - Source/GoblinSiege/AI/Tasks/BTTask_PickUpCargo.h
  - Source/GoblinSiege/AI/Tasks/BTTask_PickUpCargo.cpp
  - Source/GoblinSiege/AI/Tasks/BTTask_DeliverCargo.h
  - Source/GoblinSiege/AI/Tasks/BTTask_DeliverCargo.cpp
  - Source/GoblinSiege/AI/Tasks/BTTask_SmashOrderTarget.h
  - Source/GoblinSiege/AI/Tasks/BTTask_SmashOrderTarget.cpp
  - Source/GoblinSiege/Characters/GSPlayerCharacter.h
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - Source/GoblinSiege/Combat/GSDebugCommands.cpp
  - Config/DefaultGame.ini
  - Content/Input/IA_HordeOrder.uasset
  - Content/Input/IMC_Default.uasset
  - Content/AI/BB_HordeGoblin.uasset
  - Content/AI/BT_HordeGoblin.uasset
  - Content/UI/WBP_HordeOrderWheel.uasset
  - Content/Blueprints/BP_HordeOrderMarker.uasset
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
---

## Goal

Order wheel and world markers: the horde takes Attack/Hold/Loot/Follow.

Michael, 2026-08-12: *"We need a widget menu for ordering the goblins around with the horn
equipped."* Then, on being asked how "equipped" should work: *"we don't need to really equip
anything. I think honestly instead just mapping a key to give orders or better yet, put markers on
the map that can be seen by the player and other players in the future might be the best bet. MMB to
summon, then markers to direct. I.E. Attack, Hold, Loot, Follow."*

So: no horn weapon slot. The horn stays a GAS ability on MMB. A separate key opens a radial wheel,
and the committed order plants a visible, replication-ready world marker. Full vertical slice - the
goblins must actually obey.

## Generate

### New C++ (8 files)

| File | What it does |
|---|---|
| `Horde/GSHordeOrderTypes.h` | `EGSHordeOrder {None, Attack, Hold, Follow, Loot}` (UENUM, written to the blackboard as a byte) + `FGSHordeOrder`, the per-summoner record. The struct is deliberately NOT a USTRUCT and its pointers are weak: it lives in a bare `TMap` that GC does not trace, so a `TObjectPtr` there would look like a hard reference, be treated as one, and dangle anyway. |
| `Horde/GSHordeOrderMarker.h/.cpp` | `AGSHordeOrderMarker : AActor`. Replicated `Verb`/`Subject`/`Issuer`, `OnRep_Verb` → `ApplyVisuals()` (the server calls it directly so a listen server sees its own beacon). `bAlwaysRelevant`, no movement replication, no tick, `NoCollision`. Static `ColourForOrder()` is the single colour table, shared with the widget. |
| `Horde/GSHordeCommandComponent.h/.cpp` | The wheel. `OpenOrderWheel()` (latches, then opens; returns false and does NOT open if the trace found nowhere to send anybody), `AddWheelInput`, `CloseOrderWheel`, `OrderForDirection`, `GetLatched{Location,Subject,SubjectLabel}`, two `BlueprintAssignable` delegates, one `Server, Reliable` RPC, and the public static `ResolveOrderSubject`. |
| `UI/GSHordeOrderWheelWidget.h/.cpp` | Five `BindWidgetOptional` `UTextBlock`s (`Label_Attack/Hold/Follow/Loot/Subject`), all `BlueprintReadOnly` - see Evaluate. Repaints off the two delegates; the highlight lights in `AGSHordeOrderMarker::ColourForOrder`'s colour, dimmed while inside the dead zone. |
| `AI/Tasks/BTTask_PickUpCargo.h/.cpp` | Instant. Range-checks `OrderSubject`, calls `UGSCarryComponent::StartCarry`, writes `CargoActor`. |
| `AI/Tasks/BTTask_DeliverCargo.h/.cpp` | Instant. `PutDown()`, clears `CargoActor`, then `NotifyCourierDelivered` **deferred one tick** - see Evaluate. |
| `AI/Tasks/BTTask_SmashOrderTarget.h/.cpp` | Instant. `TryLightAttack()` for the swing, then `UGSBreakableComponent::Break`. |

### Modified C++ (6 files)

- **`GSHordeSubsystem.h/.cpp`** — the order board: `ActiveOrders` map, `IssueOrder`/`ClearOrder`, four
  `Get*For` accessors, `DescribeOrders()`, `OnHordeOrderChanged`, `OrderMarkerClassPath` config,
  `FindOrderFor`/`FindSummonerFor`/`PruneStaleOrders`/`ResolveDeliveryLocation`/`ResolveOrderMarkerClass`.
  - `IssueOrder` ground-corrects through `FindStandableSpotNear`, retires the previous marker, spawns
    the new one 150uu up, caches the delivery point, and calls `RegisterThreat` on an Attack - the
    call `GSHordeSubsystem.h:132-133` predicted from the day the stimulus bus was written.
  - `GetAssignedTargetFor` gained a **first clause**: a standing Attack order returns its subject and
    is deliberately **not** capacity-gated by `HasEngagementRoom`.
  - `PruneStaleOrders` runs on the **existing** 0.5s `ScanForThreats` timer. No second timer.
  - `GetFollowTargetFor`'s inline double loop became `FindSummonerFor` (8 lines shorter).
  - `ResetPoolForNewRaid` and `Deinitialize` retire/clear orders.
- **`GSHordeAIController.h/.cpp`** — four new `EditDefaultsOnly FName` keys; `RefreshStimulus` writes
  them and is now **the first writer of `EGSHordeState::Commanded` in this codebase's history**.
- **`GSHordeGoblin.h/.cpp`** — gained a `UGSCarryComponent`. It existed only on the player before this,
  which is why the courier verb had no code path at all.
- **`GSPlayerCharacter.h/.cpp`** — creates the command component; `HordeOrderAction` and
  `HordeOrderWheelWidgetClass` properties; guarded `BindAction` with an else-branch warning; a second
  swallow clause in `Input_Look`; widget created at ZOrder 11; **mutual exclusion between the two
  wheels lives here**, in the pawn, so neither component learns about the other.
- **`GSDebugCommands.cpp`** — `GS.Horde.Order <Attack|Hold|Loot|Follow>` traces from the player camera
  and issues straight at the subsystem, bypassing the wheel, the widget and the RPC. `GS.Horde.Status`
  now prints the standing orders and flags `[NO MARKER]`.
- **`Config/DefaultGame.ini`** — `OrderMarkerClassPath`.

### Design decisions worth not re-litigating

- **Attack needs no BT change for the living-target case.** `AGSAIControllerBase::TickFacing` reads
  `TEXT("TargetActor")` by hardcoded literal, and Block/MeleeAttack/chase all gate on it - so
  funnelling the ordered victim through `GetAssignedTargetFor` buys facing, separation, attack tokens
  and ring geometry unchanged, for one assignment.
- **`TargetLocation` is NOT free.** `UBTService_AcquireTarget` owns it and rewrites it every tick
  (`BTService_AcquireTarget.cpp:368`), so Hold got its own `OrderLocation` key. Reusing it would have
  reintroduced the ragdoll-bounds `AlreadyAtGoal` freeze that key exists to dodge.
- **Follow clears rather than sets.** A standing "Follow" that had to be honoured would be a third
  thing meaning the same as the absence of an order.
- **Wheel layout:** Attack up, Hold right, Follow down, Loot left. Follow sits 180° from Attack so a
  mis-drag can never turn a charge into a recall.
- **A new marker class is justified** against the repo's standing bias, on four grounds from
  `AGSRaidMarker`'s own header: it draws nothing outside the editor, its header forbids it carrying
  behaviour or lifetime, it sets `bReplicates = false` on purpose, and `GatherByType` is sized for
  authored actors rather than runtime churn. The "`AGSHordeSpawnMarker` will never be built" ruling is
  about spawn markers and is untouched.

## Evaluate

**REWRITTEN 2026-08-13** - the first Evaluate was stamped at 20:43Z and said "nothing has been
compiled or run" and "no editor asset exists yet". Both were true then and both are false now. The
close gate caught it, correctly.

**What is verified, and how:**

- **It compiles and links.** Editor-closed build 2026-08-12, `Result: Succeeded` in 8:51, zero errors
  across all 14 files, UHT wrote 21 generated files. The only warnings are the pre-existing
  `AbilityTags` C4996 in `GSGA_Block.cpp:24` and `GSGA_Interact.cpp:20`.
- **It ran, and Michael watched it** - see the `observed:` field. `GS.Horde.Order` issues, the
  subsystem logs the order, and `PruneStaleOrders` retires it. That run is what produced this
  ticket's most valuable output: two stacked bugs no amount of re-reading would have found (see
  Refine, "Second pass").
- **Editor assets that now exist and were read back:** `IA_HordeOrder` mapped to `R` (18 IMC rows,
  zero duplicate keys); `WBP_HordeOrderWheel` parented to `UGSHordeOrderWheelWidget` with all five
  labels; `BP_HordeOrderMarker` (cone mesh + `M_HordeOrderMarker` carrying the `MarkerColour`
  parameter `ApplyVisuals` writes) resolving at exactly the path `DefaultGame.ini` names;
  `HordeOrderAction` and `HordeOrderWheelWidgetClass` both assigned on `BP_GSPlayerCharacter` (both
  were `None`); `BB_HordeGoblin` reparented to `ACFAIBB` and `BT_HordeGoblin` repointed at it.

**What is written and has NEVER run:**

- **The four second-pass C++ fixes are uncompiled.** `TraceForOrder`'s sphere sweep, the
  subject-less Attack/Loot refusal, Hold-at-the-player, and the wheel's grey-out all exist in source
  only. The build that succeeded predates them. **In the binary Michael has, `R` still reproduces
  exactly the bug he reported.**
- **The wheel itself has never been pressed.** Everything observed went through `GS.Horde.Order`,
  which deliberately bypasses the wheel, the widget and the Server RPC. The latch, the drag, the
  sector maths, the widget repaint and the RPC are all unexercised.
- **`Commanded` has never been seen in a blackboard**, because the five keys do not exist yet.
- **Hold, Loot and Smash cannot work at all** - no BB keys, no BT branches. See Handoff.

**Still not authored:** the five BB keys, the three BT sub-trees, `BP_LootSack`, a breakable prop,
and the wheel label positions (canvas slots were not reachable from Python).

**Judged adversarially, what is most likely still wrong:**

- The engagement-ledger bypass in `GetAssignedTargetFor` is untested at scale. Ten goblins on one
  guard may read as a shoving match.
- `BTTask_DeliverCargo` defers `NotifyCourierDelivered` by a tick to avoid destroying the pawn that
  owns the running behaviour tree. That is reasoning about a lifetime hazard, unexercised.
- `Commanded` now suppresses ambient Frenzy for every verb including Hold and Loot. Argued safe,
  never watched.
- Two BT key selectors (`MeleeAttack.target_key`, the `FollowTarget Is Set` decorator) keep a stale
  editor-time type after the reparent. Both have correct names and both paths resolve at load or read
  by name, but that is an argument, not an observation.

**A correction this ticket owes its own record:** the first Evaluate flagged `IMC_Default` as possibly
stale because three `IA_` assets were unreferenced. Read live, that alarm was wrong - RMB is `IA_Aim`
with C++ routing block via `IsSwordEquipped()`, `E` is `IA_Traverse` which owns climb, and
`IA_ThrowTorch` was retired in #041. There are no dead keys. The disk read misled because live data
lives in `DefaultKeyMappings`, not the deprecated `Mappings` property.

**Judged adversarially, the things most likely to be wrong:**

- **The engagement-ledger bypass is the call I am least sure of.** Ten goblins on one guard may read
  as a shoving match rather than a swarm. If it does, the fix is to raise `UGSEngagementComponent`'s
  cap for ordered targets, NOT to reintroduce the gate - a gate would mean "attack that man" quietly
  sends six of them elsewhere while the beacon says otherwise.
- **`Commanded` now suppresses ambient Frenzy for every verb, including Hold and Loot.** A held goblin
  still fights back (the combat branches gate on `TargetActor`, which `GetAssignedTargetFor` still
  fills from the threat registry for non-Attack orders) - but this is reasoning, not observation, and
  it is the first thing to check in PIE.
- **`BTTask_SmashOrderTarget` breaks the prop at the START of the swing**, not on the contact frame.
  The honest version routes through the montage notify, which needs an ability that can target a
  non-ASC actor - a bigger change than this ticket. A crate bursts a few frames early.
- **`BTTask_DeliverCargo` defers `NotifyCourierDelivered` by one tick** because that call destroys the
  pawn, which unpossesses the controller that owns the behaviour tree component currently mid-
  `ExecuteTask`. Inline destruction would return a result into a tree being torn down under the call
  stack. Untested reasoning about a lifetime hazard is exactly the kind of thing that is wrong.
- **The carry socket does not exist on the goblin rig.** `CarrySocketName` defaults to `CarrySocket`;
  the component falls back to the mesh origin, so a delivered sack will ride at the goblin's feet.
  Ugly, not broken. Art work.
- **`IMC_Default` on disk references only 14 IA assets** and does NOT reference `IA_Block`,
  `IA_Climb` or `IA_ThrowTorch`. Either the saved asset is stale or those are three dead keys. That
  makes my "R is free" derivation provisional - it must be confirmed in the IMC editor before the row
  is authored, along with a check of `BP_GSPlayerCharacter`'s graphs for a legacy `R` key node.
  **This is a finding about existing content, not about this ticket, and it is worth Michael's
  attention on its own.**
- **No map can demo all four verbs today.** `L_CombatArena` has three `Marker.HordeArrival` markers
  and no `AGSRunicSite`; `L_Tutorial_Island` has the reverse. That is why
  `ResolveDeliveryLocation` falls back runic site → arrival marker → summoner, which makes Loot
  demonstrable in the arena with no level surgery.

**Owed to AGENT_STATE.md** (DECISION lines, once this is watched): `Commanded` has a writer; the
Attack order deliberately bypasses `HasEngagementRoom`; Follow clears rather than stores; Hold needs
its own vector key because `UBTService_AcquireTarget` owns `TargetLocation`; the order marker is a new
class and why that does not reopen `AGSHordeSpawnMarker`; and the stale-`IMC_Default` finding.

## PARKED - superseded by the ACF decision (Michael, 2026-08-12)

**Stop before finishing this ticket.** Michael's ruling, in his words: *"You've been trying to not use
the ACF the entire time we've had it and it's frustrating me. There's a lot of things in there that
would be great for us to use so we don't have to fall into the trap of trying to custom build
literally fucking everything."* He has chosen **full adoption**: the horde reparents onto the ACF
hierarchy and orders go through ACF's command system.

He is right, and the duplication is specific and embarrassing. ACF's `AIFramework` module already
ships, networked and save-game aware:

| What this ticket built | What ACF already had |
|---|---|
| `EGSHordeOrder {Attack, Hold, Follow, Loot}` | `AICommand.AttackMyTarget` / `StayThere` / `FollowMe` / `ComeHere` / `GoThere` / `FollowSpline`, registered in `Content/Configuration/CommandTags` |
| `UGSHordeCommandComponent::ServerIssueOrder` | `UACFGroupAIComponent::SendCommandToCompanions(FGameplayTag)` (Server, Reliable) |
| `FGSHordeOrder` + `ActiveOrders` order board | `UACFCommandsManagerComponent` - per-AI queue of instanced `UACFBaseCommand` objects with an `OnCommandFinished(bool)` delegate |
| `BTTask_SmashOrderTarget` / branch wiring | `BTTask_ExecuteCommand`, `BTTask_ResetCommand`, `BTDecorator_HasPendingCommands`, already wired into `ACFBT` |
| `GetFollowTargetFor` / `GetFollowSlotFor` | `UACFCompanionGroupAIComponent` - roster, `groupLead`, `GroupLeadDistance`, despawn-when-too-far, teleport-to-lead |
| — | ready-made `ACFComeHereCommandBP`, `ACFGoThereCommandBP`, `ACFAttackMyTargetCommandBP`, and an `ACF_SendCommandAction_BP` ability that issues them |

The blackboard keys this ticket's Handoff section called "marketplace keys, not ours"
(`GroupLeader`, `GroupLeadDistance`, `CommandDuration`, `HomeDistance`, `Paused`) are the schema for
exactly that system. They are dead in `BT_HordeGoblin` only because we never adopted the controller
that fills them - `AACFAIController` and `UACFUpdateStateBTService`.

**Survives the migration** (genuinely absent from ACF, so still ours to build):
`UGSHordeCommandComponent`'s radial wheel, `UGSHordeOrderWheelWidget`, `AGSHordeOrderMarker`, and the
courier verb (`BTTask_PickUpCargo` / `BTTask_DeliverCargo`, and `NotifyCourierDelivered`'s first
caller). ACF has no formations, no order wheel, no world marker and no cargo system.

**Dies in the migration:** `GSHordeOrderTypes.h`, the `ActiveOrders` board and its five accessors on
`UGSHordeSubsystem`, `EGSHordeState::Commanded` as a hand-rolled state, and `BTTask_SmashOrderTarget`
if an ACF command can be made to cover it.

**~~Already landed and worth keeping regardless of path~~ - REVERTED 2026-08-13, and this entry was
wrong.** It claimed the `BB_HordeGoblin` reparent to `ACFAIBB`, the `BT_HordeGoblin` repoint and the
two repointed selectors (`FollowTarget`, `TargetIsAttacking`) "should not be reverted". They have
been, because they broke the tree.

Measured in PIE 2026-08-13, after the change: eight summoned goblins stood at their spawn fan and
did not move at all, including after the player was teleported 2300uu across the arena. The log says
why:

```
LogBehaviorTree: Warning: BT_HordeGoblin has missing decorator node! (parent: Root Selector[0], branch: 0)
   ...the same for branches 1, 2, 3 and 4
```

**All five branches lost their decorators.** Setting `selected_key_name` on the key selectors from
Python and writing the structs back invalidated the decorator instances; saving the asset persisted
that. The reasoning behind the reparent still looks right, but the *method* was wrong - a behaviour
tree's decorators are not safely editable by poking `FBlackboardKeySelector` fields through
reflection, and nothing in the tool output said so. Both assets restored from git; the broken
versions are kept at
`<scratchpad>/broken-bt-backup/` rather than discarded, in case the diff is worth reading.

**What this costs the record:** the ACF-adoption reasoning in this section stands, but the claim that
a first step had already landed does not. The horde's blackboard is `ACFAIBB` again, `FollowTarget` /
`TargetIsAttacking` / `HordeState` are unresolved again, and "Follow the summoner" is a `MoveTo`
pointed at `SelfActor` again. Whether that state actually follows the summoner is now an open
question rather than an assumption - see #143's Evaluate.

The Handoff section below is **suspended**, not cancelled - do not author those five keys or three
branches until the migration shape is settled, or they will be built against a system we are
replacing.

## Handoff - the editor work I could not script (for Michael)

Everything below is blocked on the same wall, found the hard way: **`BlackboardKeyType_Enum`,
`_Object` and `_Vector` are not exposed to Python in this build** (only the abstract
`BlackboardKeyType` is), and VibeUE has no blackboard or behaviour-tree service. Four routes were
tried and all failed: direct `get/set_editor_property` on the key types, `unreal.new_object` (the
classes are not Python-bound), Epic's `editor_toolset.toolsets.object.ObjectTools` (it can READ and
WRITE an existing key's `enumType`/`baseClass`, but cannot CREATE one), and the engine toolset list
(no BT/BB toolset exists among the 405 registered tools). Circuit breaker hit; stopped.

**Nothing in this section blocks Attack or Follow** - those two work with no blackboard or tree
change at all. It blocks **Hold, Loot and Smash**.

### 1. `BB_HordeGoblin` - five new keys

| Key name | Type | Extra setting |
|---|---|---|
| `OrderVerb` | Enum | **Enum Type = `EGSHordeOrder`** |
| `OrderSubject` | Object | Base Class = `Actor` |
| `OrderLocation` | Vector | - |
| `DeliveryLocation` | Vector | - |
| `CargoActor` | Object | Base Class = `Actor` |

Names must match exactly - they are the `EditDefaultsOnly FName` defaults on
`AGSHordeAIController`, so a typo is a silent no-write rather than an error. Do not touch the
existing seven.

### 2. `BT_HordeGoblin` - root Selector goes from 5 children to 8

Root keeps `BTService_AcquireTarget` with `bSelectTarget = false`. Children **1, 2, 6, 7, 8 are the
existing five, unchanged and in the same relative order** - the three new ones are inserted between
the melee branch and the chase branch.

1. `BTTask_Block` - decorator on `TargetIsAttacking`. *unchanged*
2. `BTTask_MeleeAttack` - decorators `TargetActor Is Set` + `HasAttackToken`. *unchanged*
3. **NEW - Sequence, "Smash the ordered prop"**
   - Decorator `Blackboard`: `OrderVerb` **Is Equal To** `Attack`, Observer aborts = **Both**
   - Decorator `Is BBEntry Of Class`: key `OrderSubject`, Class `GSCharacterBase`,
     **Inverse Condition = TRUE**
     *(This second decorator is load-bearing. Without it an Attack order on a living guard also
     enters this branch, and its MoveTo walks the goblin in on the raw actor location - bypassing the
     ring-slot standoff that #107/#108 exist to maintain. Inverted, the branch only ever runs for
     non-character subjects, which is exactly what "smash" means.)*
   - Child 1: `MoveTo` - Blackboard Key `OrderSubject`, Acceptable Radius **140**
   - Child 2: `BTTask_SmashOrderTarget` - `TargetKey` = `OrderSubject`
4. **NEW - Sequence, "Courier the cargo"**
   - Decorator `Blackboard`: `OrderVerb` **Is Equal To** `Loot`, Observer aborts = **Both**
   - Child: **Selector**
     - Sequence "Fetch" - decorator `Blackboard`: `CargoActor` **Is Not Set**
       - `MoveTo` - Blackboard Key `OrderSubject`, Acceptable Radius **120**
       - `BTTask_PickUpCargo` - `CargoSourceKey` = `OrderSubject`, `CargoKey` = `CargoActor`
     - Sequence "Haul home" - decorator `Blackboard`: `CargoActor` **Is Set**
       - `MoveTo` - Blackboard Key `DeliveryLocation`, Acceptable Radius **200**
       - `BTTask_DeliverCargo` - `CargoKey` = `CargoActor`,
         `DeliveryLocationKey` = `DeliveryLocation`
5. **NEW - Sequence, "Hold the ground"**
   - Decorator `Blackboard`: `OrderVerb` **Is Equal To** `Hold`, Observer aborts = **Both**
   - Child 1: `MoveTo` - Blackboard Key `OrderLocation`, Acceptable Radius **150**
   - Child 2: `BTTask_Wait` - Wait Time **1.0**, Random Deviation **0.3**
     *(The Wait is NOT padding. Without it `MoveTo` returns Succeeded instantly once the goblin is
     already there, the Selector immediately re-runs it, and you get a fresh path request every tick
     against a point it is standing on.)*
6. `MoveTo` -> `TargetLocation`, decorator `TargetActor Is Set`. *unchanged - but now sits BELOW
   Hold and Loot, which is what lets a held goblin fight back without leaving its post*
7. `MoveTo` -> `FollowTarget`. *unchanged*
8. `BTTask_Wait`. *unchanged*

The three new task nodes are compiled and registered - they appear in the node picker as
**"Smash Ordered Target (Break)"**, **"Pick Up Cargo (StartCarry)"** and
**"Deliver Cargo (NotifyCourierDelivered)"**.

### 3. `WBP_HordeOrderWheel` - lay the five labels out (cosmetic)

Created and parented correctly, with all five `TextBlock`s present and their text set, but the
canvas slots could not be positioned from Python (`WidgetTree` is not reachable as an editor
property on either the asset or its CDO). They are currently stacked at the origin. Drag them to
match the sector geometry in `UGSHordeCommandComponent::OrderForDirection`:

```
            ATTACK  (up)
   LOOT                    HOLD
  (left)   <subject>      (right)
            FOLLOW  (down)
```

`Label_Subject` sits in the middle and shows what the order is about to be aimed at.

### 4. `BP_LootSack` - a carryable to test the courier verb

Actor Blueprint in `Content/GoblinSiege/Test/`: StaticMesh (any prop) + a `GSInteractable`
component with **`bIsCarryable` ticked** and `VerbTag = Interact.Carry`. No C++ needed - that
component is already `BlueprintSpawnableComponent` and already carries the flag. Place two or three
in `L_CombatArena`.

*(Not scripted because `bIsCarryable` is `protected`, so reaching it would have meant a public
setter on `UGSInteractableComponent` plus a `MakeActorCarryable` helper on `UGSRaidLibrary` -
two more files and a second ticket, to save a minute of clicking.)*

### 5. A breakable in the arena, to test Smash

Any placed prop run through the existing `BlueprintCallable`
`UGSRaidLibrary::MakeActorBreakable` (which is Python-reachable - that is what it was built for).

## Refine

### Second pass - changed after Michael watched it run (2026-08-12)

The first build compiled and Michael exercised it live in PIE via `GS.Horde.Order`. His report:
*"There's no way to give it anything to attack, it says follow on the bareground / attack the bare
ground and hold at the bare ground. One thing to note, is the default behavior for hold, should be
whereever the player is currently."* The log agreed and showed a second failure he could not have
seen from outside:

```
21:00:38  Order: Attack at V(X=-1731.39, Y=856.75, Z=36.00).
21:00:38  GS.Horde.Order: Attack on bare ground.
21:00:38  Order expired - its subject is gone. Back to Follow.      <- 0.3s later
```

**This is the ticket's most valuable finding, and no amount of re-reading the code would have
produced it.** Two independent bugs were stacked, and each one alone would have been survivable:

1. **The trace could not hit anything.** `LatchTargetUnderCamera` ran a LINE trace down the exact
   centre of the camera. With no reticle on screen and a goblin-sized target at 20m, that misses
   essentially every time and silently reports ground. Replaced with
   `UGSHordeCommandComponent::TraceForOrder` - a **sphere sweep (radius 60) that walks ALL hits and
   prefers the first orderable one**. The multi-hit walk matters as much as the sweep: a single
   sweep returns the first blocking hit, which on sloped or cluttered ground is the dirt in front of
   the guard rather than the guard, so "sweep instead of line" on its own would still have reported
   bare ground most of the time. It also returns the subject's own location rather than the sweep's
   impact point, so the beacon lands at his feet instead of out at arm's length.
2. **A subject-less Attack was accepted and then immediately binned.** `PruneStaleOrders` cannot
   tell "never had a subject" from "its subject just died", so it correctly concluded the order was
   finished - half a second after it was given. `IssueOrder` now **refuses** Attack and Loot with no
   subject and says so, rather than inventing a meaning for "attack that patch of grass".

Also changed, both directly from his feedback:

3. **Hold now plants where the player is standing**, not where he is looking - his call, and the
   right reading of the verb. In play "hold" means "stop trailing me, stay HERE"; you walk to the
   doorway you want held and press it. It also makes Hold the one verb that cannot fail to find a
   sensible point, which matters because it is what you reach for when a fight is going wrong.
4. **The wheel greys out Attack and Loot when nothing orderable is latched**, mirroring the refusal
   in `IssueOrder` rather than guessing. Without it the wheel looks identical whether you are
   pointing at a guard or at grass, and the player finds out his order was meaningless by watching
   nothing happen - which is exactly what "there's no way to give it anything to attack" describes.
5. `GS.Horde.Order` now calls the same `TraceForOrder` as the wheel. Its first version ran its own
   line trace and therefore had its own aiming behaviour, which is how a debug path starts lying
   about the thing it exists to isolate.

**A correction to this ticket's own Evaluate section.** It flagged that `IMC_Default` referenced only
14 IA assets and that `IA_Block` / `IA_Climb` / `IA_ThrowTorch` were absent - "either the saved asset
is stale or those are three dead keys". Read live in the editor, that alarm was wrong and all three
are deliberate: RMB is `IA_Aim` and C++ routes it to block via `IsSwordEquipped()`, `E` is
`IA_Traverse` which owns climb, and `IA_ThrowTorch` was retired in #041. There are no dead keys. The
disk read was misleading because the live data lives in `DefaultKeyMappings`, not the deprecated
`Mappings` property that a naive read returns empty.

**Changed on self-review, before handing back:**

- `AGSHordeOrderMarker::ApplyVisuals` had a nonsense guard left in it - a half-written
  "does this material have the parameter" check ending in `&& false`, plus the `bColourParamWarned`
  member it needed. There is no cheap, version-stable way to ask a MID whether a parameter exists, so
  the failure mode is now **chosen and documented** rather than half-detected: a material without
  `MarkerColour` draws every verb in its authored colour, and the comment says to check the material
  first if that happens. The dead member is gone from the header.
- Dropped `FVector_NetQuantize10` from the RPC in favour of a plain `FVector`. It saves a handful of
  bytes on a message sent a few times a minute, against a real cost: the correct include for the
  quantised types has moved between engine versions and finding that out costs a six-minute build.
- Replaced both `SCENE_QUERY_STAT(...)` uses with plain `FCollisionQueryParams(TEXT("..."), ...)`,
  same reasoning - the macro's header is not one I could confirm was reachable from
  `GSDebugCommands.cpp`.
- Removed a `const_cast<UWorld*>` in `ResolveDeliveryLocation` that only existed because I had
  declared the local `const`; `UWorldSubsystem::GetWorld()` is const and returns a non-const `UWorld*`.
- Added `BehaviorTree/BlackboardData.h` to all three new BT task `.cpp`s after checking what
  `BTService_AcquireTarget.cpp` includes - `GetBlackboardAsset()` returns a type they use.
- Made all five widget labels `BlueprintReadOnly` as well as `BindWidgetOptional`. The weapon wheel
  gets away without it because nothing reads its labels from a graph; the moment `WBP_HordeOrderWheel`
  does, the widget BP fails to compile with "is not blueprint visible" - which is #064 exactly.

**Deliberately left undone:**

- Every editor asset. They are the next step, not an oversight - they need a live editor, and the C++
  needs to compile before a BP can be reparented to a class that does not exist yet.
- Per-goblin / squad selection; order queueing and waypoints; client prediction of the marker;
  see-through-walls rendering on the beacon; per-issuer marker colouring for co-op (`Issuer`
  replicates specifically so that can be added later without touching the wire format); marker audio
  and barks (`OnHordeOrderChanged` is `BlueprintAssignable` so a designer can hang one off it without
  a rebuild).
- The `FollowSlot` ring maths, still written by the controller every 0.2s and still read by nothing.
  Tempting while in that file; it is a different ticket and it would make this diff unreviewable.
- `Stranded` and `PanicStranded` still have no writer. They belong to the reachability checks.
