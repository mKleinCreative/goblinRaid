---
id: 213
title: Hold is unbuilt: BT_HordeGoblin has no branch reading OrderVerb, so a Hold order lands nowhere
agent: claude-hordehold
status: abandoned
claimed: 2026-08-20T21:04Z
build: none
waiting_on: Michael, ten minutes in the Behaviour Tree editor - BT_HordeGoblin has an EdGraph so this cannot be done from Python without it vanishing on the next open. Full spec in the ticket.
evaluated:
observed:
scenario:
files: 
  - Content/AI/BT_HordeGoblin.uasset
---

## Goal

Hold is unbuilt: BT_HordeGoblin has no branch reading OrderVerb, so a Hold order lands nowhere

## Generate

**No asset was changed. This ticket is a specification and a refusal to use the wrong tool.**

`Hold` lands nowhere because `BT_HordeGoblin` is a flat Selector of six children — Block, Melee
Attack, Menace Orbit, Chase Target, Follow Summoner, Wait — and **not one of them reads `OrderVerb`**.
Attack only ever appeared to work because `AGSHordeAIController` writes `TargetActor`, which the
existing Chase Target branch already consumes. Hold, Loot and Smash have no consumer at all;
`BTTask_PickUpCargo`, `BTTask_DeliverCargo` and `BTTask_SmashOrderTarget` compile and sit in no tree.

### Why I did not add it from Python

`BT_HordeGoblin` **has an editor graph**. Measured against `BT_Militia`, which `AGENT_STATE` records
as Python-authored with none:

| Asset | `BehaviorTreeGraph` refs | `BehaviorTreeGraphNode` refs |
|---|---|---|
| `BT_HordeGoblin` | 5 | 4 |
| `BT_Militia` | 0 | 0 |

`UBehaviorTree` keeps a runtime `RootNode` **and** an editor graph, and opening the asset recompiles
the graph over the runtime tree. A node injected from Python would work until the next time anyone
opened that BT, and then disappear with no error — the mirror image of the trap already recorded for
`BT_Militia` ("opening it in the Behaviour Tree editor and saving may regenerate an empty graph and
wipe the tree"). VibeUE ships a StateTree service but **no BehaviorTree service**, so there is no
supported path that keeps both halves in step.

**This wants ten minutes in the Behaviour Tree editor, by hand.**

### The specification

Add one child to the **Root Selector**, at **index 2 — after `Melee Attack`, before `Menace Orbit`**:

- **Task:** `Wait`, 0.5s, random deviation 0.1
- **Name:** `Hold Position`
- **Decorator:** `Blackboard` → key `OrderVerb`, `Is Equal To`, enum value **Hold**
- **Observer aborts:** **Both**

### Why each of those

**The position is the whole design.** Below `Block` and `Melee Attack` means a held goblin still
raises its guard and still swings at anything already in reach — hold position, defend yourself.
Above `Menace Orbit`, `Chase Target` and `Follow Summoner` means it will not orbit, chase or walk back
to the player. Put it any higher and a held goblin stands there being hit; any lower and it wanders.

**`Observer aborts: Both` is not optional**, and the project has already paid to learn it. #086 and
#087 record that without a LowerPriority abort a Selector only re-reaches a higher branch once per
cycle — so a Hold issued mid-chase would not take effect until the current `MoveTo` finished, which
reads exactly like the order being ignored. `Both` also releases the instant the verb changes, so
clearing Hold resumes immediately.

**A `Wait` rather than a bespoke task**, because the goblin's job under Hold is to do nothing while
staying responsive. Note the trap recorded twice in `AGENT_STATE`: a Selector **restarts when a child
succeeds**, so a zero-length wait would spin the tree. 0.5s re-evaluates often enough to feel
immediate without busy-looping.

**Follow is already handled** — #210 nulls `FollowTarget` under any standing order, so
`Follow Summoner` cannot compete. `Chase Target` still can, because `TargetActor` may legitimately be
set while holding, which is exactly why Hold must sit above it.

## Evaluate

**The gap is measured, not assumed:** the tree was walked in the live editor and its six children and
single service listed; the EdGraph question was settled by comparing serialized graph references
against a known Python-authored tree.

**What I cannot claim:** that this spec is correct in play. It is reasoned from the tree's structure
and from two recorded BT failures, and no goblin has held position. The `Both` abort setting in
particular is the kind of thing that reads right and behaves differently.

**A string in a package proves a reference exists, not a wiring** — the EdGraph conclusion rests on
reference counts plus the contrast with `BT_Militia`, not on reading the graph object itself. It is
strong evidence, not proof, and the safe reading favours hand-authoring either way.

## Refine

Considered doing it from Python anyway and rejected it: a change that survives until the next time
someone opens the asset is worse than no change, because it fails silently and at a distance from the
edit that caused it.

**Left undone by the same reasoning:** `Loot` and `Smash` have the identical gap, and their tasks
already exist unused. Once a `Hold Position` branch proves the `OrderVerb` decorator pattern works,
those two are the same shape and worth doing in the same editor session.
