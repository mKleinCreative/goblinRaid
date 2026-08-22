---
id: 192
title: Stage 0: the statue actually shatters - Dataflow fracture pipeline and the first real GeometryCollection
agent: claude-fracture
status: done
claimed: 2026-08-18T23:27Z
build: none
waiting_on:
evaluated: 2026-08-19T00:05:36Z
observed: 2026-08-19T00:16:50Z | CORRECTED: Michael watched the statue crumble spectacularly, but clarified he never hit it - the collection defaulted to Chaos_Object_Dynamic, so it simulated from BeginPlay, fell, and bEnableDamageFromCollision shattered it on landing at t=0. Fixed to Chaos_Object_Static so it stands until Break() promotes it. What IS proven: the collection is real, 44 chunks read as glorious rather than gravel, and Chaos simulates correctly. What is NOT yet observed: the statue breaking in response to being struck.
scenario: PIE in L_CombatArena, BP_Statue_Warrior_TEST at (-1700,380,-70), 44-chunk GC_Statue_Warrior, no player input - it broke on its own at level start
files: 
  - Content/Destruction/Dataflow/DF_PropFracture.uasset
  - Content/Destruction/GC_Statue_Warrior.uasset
  - tools/hamlet/gs_fracture.py
---

## Goal

Stage 0: the statue actually shatters - Dataflow fracture pipeline and the first real GeometryCollection

## Generate

**The project's first real fracture.** Until now it contained exactly one GeometryCollection - an
orphan (`GC_House_Window_C`, 10 KB) that nothing referenced. Nothing in the game shattered; the crate
and barrel work by mesh swap.

**The plan's riskiest unknown is answered, and the answer made most of Stage 0 unnecessary.** It asked:
*"Can a Dataflow variable drive `StaticMeshToCollection`'s mesh input in 5.8?"* - flagged as the thing
that had to be proved before scaling, with three fallbacks if not. It can, and **Epic already ships a
template built exactly that way**: `DF_GC_Template_StaticMesh`, discovered through
`ListDataflowTemplatesForAssetClass`. No `DF_PropFracture` needed authoring:

```
SourceStaticMesh (FGetDataflowVariableNode) --Value--> StaticMeshToCollection --> UniformFracture --> Terminal
                                                                              \-> Materials / InstancedMeshes / RootProxyMeshes
```

`RootProxyMeshes` is wired in the template, so the plan's load-bearing render lever - an intact prop
drawing as a plain static mesh until it breaks - comes for free rather than needing setup.

**Graph authoring turned out to be fully scriptable** via `DataflowAgent.DataflowAgentToolset`
(`CreateGraph`, `AddNode`, `ConnectNodePins`, `AddVariable`, `SetVariable`, `GetNodeTypeSchema`). The
plan assumed a hand-authored graph because Fracture Mode has no scripting surface; that is true of
Fracture Mode and false of Dataflow.

**`GC_Statue_Warrior`**: created from the template, `SourceStaticMesh` set to `SM_WarriorStatue`
(384x441x588 uu), regenerated through `regenerate_asset_from_dataflow`.

**`BP_Statue_Warrior`**: `GeometryCollectionComponent` (the collection, `bAllowRemovalOnSleep=false`,
`bEnableDamageFromCollision=true`) + `UACFDestructableComponent` + `UGSBreakableComponent`
(`SmashHitPoints=4`, `bOpensBuilding=false`). Placed at (-1700, 380, -70), 484uu from the PlayerStart
and 198uu clear of the crate so one swing cannot hit two props and muddle the test.

`bEnableDamageFromCollision=true` is the statue's own case and the opposite of every other prop: it
should tip as a figure and shatter on landing, not dissolve into gravel mid-swing. `bAllowRemovalOnSleep`
is off because, as `GSDestructibleObjective` argues for the granary, otherwise "the raid has no memory".

## Evaluate

**The collection is real, and measured rather than assumed:**

```
rest transforms   45          (root + ~44 chunks)
asset size        7,776,679 bytes   vs GC_House_Window_C at 10,225
bounds            384 x 441 x 588   exactly SM_WarriorStatue
```

Counted by spawning a `GeometryCollectionActor`, assigning the rest collection and calling
`get_local_rest_transforms()`. That matters because the plan warned specifically about "an empty asset
being saved" - `regenerate_asset_from_dataflow` returning `True` proves the graph ran, not that
geometry came out of it.

**PARTLY OBSERVED, AND THE FIRST READING OF IT WAS WRONG.** Michael reported *"it crumbles and it's
glorious"* and this ticket was closed on it. He then clarified: **"I didn't hit it at all though."**

So what was watched is a statue that **shatters itself on level start**, not a statue that shatters when
struck. The visual is right; the trigger is a bug, and the ticket briefly recorded a success that had
not happened.

**Cause, found immediately and not guessable from the visual:** `UGeometryCollectionComponent` defaults
to `ObjectType = Chaos_Object_Dynamic`, so the collection simulated from BeginPlay, fell, and hit the
ground. `bEnableDamageFromCollision = true` - set deliberately so the statue shatters on landing rather
than dissolving mid-swing - then did exactly what it says on the very first contact it ever had. The two
settings are individually correct and together mean "fall over and smash at t=0".

The plan called for a **dormant** collection and this Blueprint never set one. Fixed by setting
`ObjectType = Chaos_Object_Static` on both the class and the placed instance; `Break()` calls
`SetSimulatePhysics(true)`, which promotes it to dynamic at the moment it should come apart. That line
now has a second reason to exist.

**What the accident did prove**, and it is not nothing: the collection is real and genuinely simulates,
ACF's `ForceDestruction` path is not required for the pieces to behave, and 44 chunks of statue read as
"glorious" rather than as gravel - which was the open question about chunk count.

**Still unobserved: the actual feature.** Nobody has yet hit the statue and had it break in response.
The ACF branch in `Break()` and the `SetSimulatePhysics` line remain unexercised.

**Unproven:** chunk count is the template's default, not tuned. 44 leaves on one fracture level is in the
plan's ballpark for a statue (~40) but arrived at by accident rather than by choice.

## Refine

**Changed after self-review:** the first attempt guessed a node refPath to set `MinVoronoiSites` and
`UpdateNode` silently returned `None`. Nodes are addressed by GUID. Rather than reverse-engineer the
reference format for a value that only needed to be *plausible* at this stage, the template defaults
were kept and the result measured - 44 chunks is a reasonable statue. **The tuning path is real but
unexercised**, and that is stated rather than implied.

**Deliberately not doing: the two-level fracture from the plan's budget table** (6-9 clusters -> ~40
leaves, `MaxSimulatedLevel 0` so it topples as one figure before breaking). That is the *feel* pass, and
it should follow someone watching the single-level version - tuning a shatter nobody has seen is how the
first four values get chosen against an imagined problem.

**Deliberately not doing: `AGSDestructibleObjective` integration** (`TriggerCollapse` extraction, the
objective tag). #189 has just renamed that class to `AGSObjective_ToppleStatue` and is at review; wiring
into it while the rename is still unclosed would collide with another agent's work.

**Deliberately not fracturing the crate and barrel.** They already read well as mesh swaps at zero cost.
Chaos is reserved for the thing that genuinely wants to shatter, which was the plan's position and still
looks right.
