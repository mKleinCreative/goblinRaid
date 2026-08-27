---
id: 317
title: One destroyed state, three ways in: UGSCrumbleComponent unifies topple, burn-down and smash
agent: claude-crumble
status: done
claimed: 2026-08-26T03:24Z
build: none
waiting_on: Checked in-editor 2026-08-26: BP_Statue_Warrior DOES carry Topplable (+Breakable, Destructable, Collection root, IntactMesh child) - no regression, and its parent is Actor not AGSDestructibleObjective so that class was never in the statue chain. REAL BLOCKER: BP_Statue_Warrior has Replicates=False on the actor, so the component's replicated state reaches no client and the co-op fix is inert until that box is ticked. Michael's call. Then: editor-closed build once the gate opens, and PIE - grapple the idol and confirm LogGSCrumble reports MOVED, not just SWAP.
evaluated: 2026-08-26T19:50:47Z
observed: 2026-08-26T17:23:02Z | Michael watched the fractured house come apart and fall the instant the anchors were released - he called it perfect and exactly what he wanted. The release sequence (dormant -> Dynamic -> simulate -> anchors off a frame later, no impulse) drives a 267-chunk merged house the same way it drives the statue.
scenario: PIE in L_CombatArena, GS_CrumbleDemo_House using GC_MERGED_House_Small_03, released by hand from Python because the C++ is still unbuilt - single player, one machine
files: 
  - Source/GoblinSiege/Destruction/GSCrumbleComponent.h
  - Source/GoblinSiege/Destruction/GSCrumbleComponent.cpp
  - Source/GoblinSiege/Destruction/GSTopplableComponent.h
  - Source/GoblinSiege/Destruction/GSTopplableComponent.cpp
  - Source/GoblinSiege/Destruction/GSDestructibleObjective.h
  - Source/GoblinSiege/Destruction/GSDestructibleObjective.cpp
  - Source/GoblinSiege/Destruction/GSBreakableComponent.h
  - Source/GoblinSiege/Destruction/GSBreakableComponent.cpp
  - Source/GoblinSiege/Destruction/GSBuildingObjective.h
  - Source/GoblinSiege/Destruction/GSBuildingObjective.cpp
  - Content/Blueprints/Destructibles/BP_Statue_Warrior.uasset
  - Content/Destruction/GC_MERGED_House_Small_03.uasset
  - Content/Maps/Test/L_CombatArena.umap
---

## Goal

One destroyed state, three ways in: UGSCrumbleComponent unifies topple, burn-down and smash

## Generate

Michael's framing is the design: *"we don't want to burn statues, we break them with a grapple, we
break and destroy buildings with a torch. the commonality is they get destroyed, and on the destroyed
state after being on fire, or being torn down, they crumble into pieces like the statue."*

**NEW: `UGSCrumbleComponent` (.h/.cpp)** - one entry point, `Crumble(Impulse, ImpulseAt)`,
server-authoritative and idempotent. Owns the four-step release sequence that #193/#195 found by
measurement: hide intact mesh + collision and reveal the collection in one frame; `ObjectType =
Chaos_Object_Dynamic` BEFORE `SetSimulatePhysics`; `RemoveAllAnchors()`; impulse ONE FRAME LATER. All
four trap comments carried across verbatim. State is `UPROPERTY(ReplicatedUsing = OnRep_Release)
FGSCrumbleRelease` (bool + impulse + world point) and **every visual runs inside the OnRep**, server
included, so host and client execute one code path. `FindOrAdd()` attaches at runtime the way
`EnsurePiecesFlammable` does, so a monument or house placed as a plain StaticMeshActor needs no
hand-wiring. `GS.Crumble.ResetAll` added; `ResetCrumble` is deliberately NOT authority-gated so a
tester can put back one client's view.

**`UGSTopplableComponent`** - keeps the rope, the heave, the fall direction, the ward, the score, the
corruption report and `OnToppled`. The release now delegates. `ResolveCollection`,
`ReportToppleOutcome` and `ToppleOutcomeTimer` deleted (moved). `IntactMeshComponentName` is
propagated into the crumble component so an authored value cannot be silently dropped.

**`AGSDestructibleObjective`** - the broken copy, deleted. `HandleBurnedDown` (no authority guard,
`SetSimulatePhysics` with no `ObjectType`, no `RemoveAllAnchors`) is replaced by `HandleCrumbled`
bound to `OnCrumbled`. Gains a `UGSCrumbleComponent` subobject. **This resolves the #189 WARNING**:
the statue no longer completes by burning, per GDD 2.8.

**`AGSBuildingObjective`** - overrides `HandleCompleted()` and calls new `CrumblePieces()`: a burnt-out
building releases every adopted piece that has a fracture asset, with ZERO impulse (nothing shoved it;
it burnt through and gave way). Pieces with no collection are skipped **silently**, with one summary
line per building - "released 0 of 34; 34 have no fracture asset yet" - because warning 34 times per
house is how a log category gets filtered out and ignored.

**`UGSBreakableComponent`** - **code unchanged**, header note added. See Evaluate.

## Evaluate

**The plan said to repoint `Break()` at the crumble component. That was wrong and I did not do it.**
`GSTopplableComponent.h:20-28` has carried the reason since #193: `Break()` delegates to ACF's
`ForceDestruction`, which shatters the collection WHERE IT STANDS - a statue that explodes at the top
of its lean instead of falling. Crumble is *release*; Break is *shatter in place*. They are opposites
that look identical, so I left `Break()` alone and wrote the distinction into
`GSBreakableComponent.h` so the next reader does not "unify" them. A prop may carry both.

**NOTHING HERE HAS BEEN COMPILED OR RUN.** `buildgate` is CLOSED on #313/#314/#315/#316. Every claim
below is read-by-eye, not verified:
- unverified: that it compiles at all.
- unverified: that the statue still topples. The behaviour should be identical - same four steps,
  same order, same constants - but "should" is exactly the word #193 spent a session disproving.
- unverified: the co-op claim, which is the entire point of the refactor.

**Known gap I could not close from here.** `AGSDestructibleObjective` no longer destroys itself; it
now waits to be crumbled by something. Whether `BP_Statue_Warrior` actually derives from it AND
carries a `UGSTopplableComponent` decides whether the statue objective still completes at all. I did
not open the editor to check (build gate + asset). **If that Blueprint has no topplable, the statue
objective is now inert and this ticket regressed it.** First thing to look at.

**Outside the goal:** `Missions/GSObjective_ToppleStatue.h:7` points at the #189 warning I just
resolved. I did not claim that file so I did not edit it - it wants a one-line comment fix.

AGENT_STATE owes: DECISION - crumble (release) and break (shatter in place) are permanently separate
verbs; the #189 statue-burns contradiction is closed by removing the burn trigger, not the burn.

## Refine

Three things I changed after re-reading my own first pass:

1. **The swap log lied on one actor shape.** `AGSDestructibleObjective` has no static mesh at all -
   the collection IS its only visual - and my first version logged that as "NOT FOUND, so nothing was
   hidden". Now three distinct outcomes: hidden (Log), nothing-to-hide (Log), and *ambiguous, several
   candidates, refused to guess* (Warning). Only the last is a fault. Training people to ignore a log
   category is how the old topple log came to announce success three times while the statue stood
   there.
2. **`*FString::Printf(...)` inside a ternary handed `UE_LOG` a pointer into a temporary.** It
   survives only because the temporary outlives the full expression. Replaced with a named local.
3. **`RemoveAllAnchors()` is now unconditional**, where the impulse is not. A building released with
   no shove still has to stop being pinned, or it burns to the ground and hangs in the air exactly as
   it stood - which the topple path never exposed because it always had an impulse.

Deliberately left undone: no house fracture assets (that is the plan's stage 2, one house, measured
first); no `decisions-ledger.md` row (stage 1 is a bug-fix refactor, the ledger row belongs with the
stage-3 rollout); `Break()` untouched, above.

## Addendum 2026-08-26 - the asset half, checked and changed in-editor

**Checked** `BP_Statue_Warrior` (via BlueprintService, editor open):
- Components: `Collection` (GeometryCollectionComponent, ROOT) with child `IntactMesh`
  (StaticMeshComponent), plus `Destructable` (ACFDestructableComponent), `Breakable`
  (GSBreakableComponent) and **`Topplable` (GSTopplableComponent)**. Its `IntactMeshComponentName`
  is `"IntactMesh"`, which matches the real component and the crumble component's default - the
  swap will resolve.
- **No regression from this ticket, and not for the reason I feared.** Its parent class is `Actor`,
  NOT `AGSDestructibleObjective` - so `AGSObjective_ToppleStatue`'s `Cast<AGSDestructibleObjective>`
  never matched the placed statue in the first place. Rewriting that class could not have broken the
  statue chain because the statue was never in it. Confirms AGENT_STATE's "instantiated NOWHERE".

**Changed, on Michael's instruction:** `BP_Statue_Warrior` had `Replicates = False` on the ACTOR
while `Topplable` had `bReplicates = True`. A replicated component on a non-replicating actor has
nowhere to send state, so `bToppled` never reached a client and the new `Release` would not have
either - the co-op fix would have been inert and the two-client test would have failed looking like
the new component's fault. Set `Actor.bReplicates False -> True`, compiled, saved. **Verified off
disk after a package reload**, not from the in-memory object: `replicates = True`, all five
components intact.

Rollback: untick Replicates on the Blueprint, or revert
`Content/Blueprints/Destructibles/BP_Statue_Warrior.uasset`.

Still unverified: everything that needs a compiler or a running game. Net relevancy is left at the
default (distance-based) rather than forced Always Relevant - property state is sent when a client
becomes relevant, so a late-arriving player still sees a fallen statue.

## Addendum 2026-08-26 - the first fractured house

`gsqueue check` was CLEAR on both new files before either was written.

**The earlier research I relayed was wrong and #192 already had the answer.** I reported "no Python
batch fracture pass exists - Fracture Mode has zero BlueprintCallable". True of Fracture *Mode*, and
irrelevant: **Dataflow is fully scriptable** via `DataflowAgent.DataflowAgentToolset` +
`unreal.DataflowBlueprintLibrary`, and Epic ships `DF_GC_Template_StaticMesh`
(`SourceStaticMesh` variable -> `StaticMeshToCollection` -> `UniformFracture` -> Terminal). #192
discovered this on 2026-08-18 and I did not read that ticket before writing the plan's stage 3.
Checking the repo's own prior art first would have saved the wrong claim.

**Made:** `GC_MERGED_House_Small_03` from `SM_MERGED_House_Small_03` (the most-placed small house on
L_Tutorial_Island - 4 instances; 41 distinct merged houses / 55 instances total on that map).
Route: create GC asset -> template picker (Michael chose *GC Template StaticMesh*; the factory dialog
is MODAL and stalled one script for 286s) -> `override_dataflow_variable_object(gc,
"SourceStaticMesh", mesh)` -> `regenerate_asset_from_dataflow`.

**Measured, not assumed** - `regenerate...` returning True only proves the graph ran, which is #192's
explicit warning:

```
rest transforms   268        (root + ~267 chunks)   [statue was 45]
bounds            1974 x 1999 x 1517 uu  - exact match for SM_MERGED_House_Small_03
```

**267 chunks is the template default, not a choice, and it is ~6x the statue.** Unproven whether that
reads as a collapsing house or as gravel, and unmeasured for cost - 267 simulating bodies per house,
with 55 merged houses on the map, is the number stage 2 exists to sanity-check. Do not roll out on
this figure.

**Demo placed** in `L_CombatArena` (NOT L_Tutorial_Island - #313 has right of way on that map and its
external actors): `GS_CrumbleDemo_House` at PlayerStart + 2600uu, resting
`Chaos_Object_Static` with `enable_damage_from_collision=true` and `allow_removal_on_sleep=false` -
the dormant setup #192 landed on after the statue fell and smashed itself at t=0.

Still true: no C++ compiled, gate held by #313-#316. The demo is driven from Python
(`remove_all_anchors` is exposed), which also lets the four-step release be validated on a house
before the build.

## Addendum 2026-08-26 - built, and the collapse shape is now code

**BUILT, TWICE, CLEAN.** Michael authorised `-IgnoreQueue` to break a genuine deadlock: #315 and
#316 both said "needs an editor-closed build" and the gate forbids building while anything is open,
so they had sat 15h unable to close. #313 closed (parked by Michael), #314 abandoned (Refine never
written by claude-ui; not mine to invent). Build 1: 96s, refactor compiles. Build 2: 31s, with the
attach. Only warnings are pre-existing AbilityTags C4996 in Weapons/Abilities.

**The collapse shape, tuned live in PIE with Michael against a 32-chunk merged house.** Every
number here was watched, and most of them were wrong first:

| Setting | Value | What it buys, and what it looked like wrong |
|---|---|---|
| `ClusterCrumblePasses` | 3 | Damage thresholds are PER LEVEL (measured [500000, 50000, 5000]); one pass brings the roof down as a bonded slab. Three and "part of the roof broke up". |
| `CollapseShoveCount` | 7 | 4 corners folded tight (1980x2002); Michael asked for 7. |
| `CollapseShoveMagnitude` | 5,000,000 | On a measured 100,000 kg house. 2M folded tightest; 5M spreads wider - Michael's call, and his eye beats the bounds number. |
| `CollapseInwardRatio` | 0.6 | Inward lean is what folds it into its own footprint. One shove straight down at the centre spread it sideways instead. |
| `CollapseShoveHeightFraction` | 0.63 | **The one that was silently doing nothing.** Shoves were first applied ABOVE the roof, where the topmost piece origin (+969 of 1518) meant there was no geometry to push. |

Defaults are ZERO on the component, so a monument is untouched - it still comes down on the rope's
impulse alone, which is the whole difference between a topple and a collapse.

**`EnsurePieceCollection`** resolves a piece's fracture by mesh name (SM_Foo -> GC_Foo in
`CrumbleCollectionFolder`, default `/Game/Destruction`), attaches it hidden and
`Chaos_Object_Static`, and returns null SILENTLY when none exists - which is every unfractured piece
of every building on the map, and is what lets houses be fractured one at a time. Rest collection is
set BEFORE `RegisterComponent`: doing it after leaves the component reporting zero rest transforms,
which reads exactly like an empty fracture asset (hit live, cost a take).

**Still unobserved: the full cycle.** The earlier attempt was a puppet - Python swapping the
statue actor's rest collection - and it failed on both counts: the burn never completed
(`complete=False completion=0.50 burnt 1/2`, fire went out) and the borrowed collection would not
simulate. That rig is abandoned; this is the real code path and has never been run.

**Separate defect, not this ticket, blocks the cycle anyway:** `GSBuildingObjective_233` on
L_Tutorial_Island adopted only **2 pieces** and its fire stalled at 50% without spreading. Its own
header warns about exactly this - an adopt radius wider than spread distance can traverse makes a
building harder to burn, not richer. Worth its own ticket.

## Evaluate - REFRESHED 2026-08-26 at close (the earlier one described a tree that no longer exists)

**What is now OBSERVED, by Michael, on screen:**
- A fractured merged house burns out on L_Tutorial_Island and folds into its own footprint. His
  words: "exactly what I wanted, wonderful." Measured with GetCurrentTransforms: 32 of 33 pieces
  settle within 3000uu of the house; the settled pile is 2883 x 2458 x 1404 against an intact house
  of 1974 x 1999 x 1518 - a building that gave way rather than one that was thrown.
- The statue path still runs (topple -> crumble), though see the open item below.

**FOUR BUGS, each of which hid the next, and every one reported success while doing nothing.**
This is the part worth reading:

| Symptom | Cause |
|---|---|
| `simulating 0` | A StaticMeshActor's root is Static mobility; a child component cannot simulate. |
| `simulating 1`, motionless | The collection was still attached - driven by the parent transform, not the solver. |
| Windows vanished, "released 6 of 440" | GC_House_Window_C is a 10 KB stub with no geometry. |
| House vanished, pieces at z = -43,000 | A runtime-created component defaults to collision profile "Custom" and does not block the landscape. |

**And after all four were fixed it STILL failed** - 31 of 33 pieces ended 1.3 km away. The answer was
not another patch: Chaos builds a collection's proxy once, at registration, from the actor it belongs
to. A collection wants to BE an actor's root. `SpawnCollectionProxy` now spawns an
AGeometryCollectionActor at the piece's transform and hides the piece, which is the shape that had
been working in L_CombatArena the whole time. That retired three of the four fixes above.

**MY INSTRUMENT WAS WRONG TWICE, and it cost more of Michael's takes than the bugs did.**
1. `OUTCOME: MOVED / DID NOT MOVE` reads the ROOT CLUSTER's velocity, which is zero once the
   collection has shattered. A house whose pieces were falling a kilometre logged the same line as a
   house that never moved.
2. After I added the detach fix, `GetActorBounds` no longer included the collection at all - so a
   "spread dropped from 45,817 to 4,316" I reported as progress was measuring the leftover hidden
   mesh. The tell was two different runs returning byte-identical X/Y, and I missed it.
   `GetCurrentTransforms` (new in 5.8) is the honest measure and is what the outcome line should use.

**Also wrong, and stated to Michael as fact:** I reported the windmill's empty `ObjectiveTypeTag` as
a bug that would stop it counting. Burn objectives are typed by the `ObjectiveType` ENUM, not that
tag; it counted correctly all along. Checked the wrong field.

**Not observed:** co-op. Every replication path here is written and compiled and has never had a
second machine attached. `BP_Statue_Warrior` had `Replicates=False` until this ticket ticked it.

**Open, and deliberately left:** the statue reports `simulating 1`, Dynamic, anchors removed, impulse
90,000 applied - and does not move. Michael's report: "the statue cracked last time I tried it", and
he chose to prioritise the house. The likely cause is impulse-to-mass (the house needed 5,000,000 at
100,000 kg), but that is unmeasured and the outcome line now logs mass so the next run will say.

## Refine - what changed after this evaluation

Nothing further in code. The collapse ring's defaults (7 shoves, 5M) were tuned live with Michael and
are left as he set them, with the honest note that 5M was chosen while everything was still
scattering for other reasons and is probably now too strong - it is EditAnywhere and re-tunable
without a build.

Deliberately NOT done: the outcome line still reports root-cluster velocity rather than piece spread.
It should be changed, and it is the single highest-value thing left in this file, but changing an
instrument at the moment of closing a ticket is how you ship an instrument nobody has watched.
Carried to #324.

> 2026-08-26T19:50Z Refreshing the Evaluate: the tree changed substantially after the original was stamped.

> 2026-08-26T19:50Z Evaluate rewritten against the shipped tree.

> 2026-08-26T19:50Z Refreshing the Evaluate: the tree changed substantially after the original was stamped.

> 2026-08-26T19:50Z Evaluate rewritten against the shipped tree.
