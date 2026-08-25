---
id: 300
title: Livestock become loot in place: MakeActorCarryable, animations untouched
agent: claude-warren
status: done
claimed: 2026-08-25T02:02Z
build: required
waiting_on:
evaluated: 2026-08-25T03:28:10Z
observed: 2026-08-25T03:27:39Z | Michael played it: he ordered his goblins to loot a pig and watched them fetch it, and saw one goblin hand the carried pig off to another mid-journey - the warband relaying it rather than one courier doing the whole trip. He also picked up the pig by the portal himself and carried it, which is the same livestock pickup that did nothing for him before this work.
scenario: Michael playing in PIE on L_Tutorial_Island with a summoned horde, issuing the Loot order at livestock and following the carry.
files: 
  - Source/GoblinSiege/Raid/GSRaidLibrary.h
  - Source/GoblinSiege/Raid/GSRaidLibrary.cpp
  - Content/Maps/L_Tutorial_Island.umap
---

## Goal

Michael, in play: *"I can't pick up the pigs."* Then: *"they have animations already assigned to them,
is it possible to assign the logic on top of them?"* And: *"did we ever work out your troops being
able to grab the livestock?"*

Three things, and they turned out to be one chain.

## Generate

**Why the pigs could not be picked up: they were never livestock.** `L_Tutorial_Island` is dressed
with **46 `A_Pig_*` actors** that are plain `SkeletalMeshActor`s from the Dreamscape pack playing
`A_Pig_Eat` / `SitLoop` / `SleepLoop` as single-node animations. No interactable, no loot value.
Before this session the level contained **zero** real `BP_Livestock_Pig`. Walking up to one and
getting nothing reads as the pickup being broken, not as that pig being scenery.

**`UGSRaidLibrary::MakeActorCarryable(Actor, LootValue, Prompt, ChannelSeconds)`** - the third sibling
of `MakeActorFlammable` and `MakeActorBreakable`, and it exists for the same reason they do: a
component added from Python any other way **does not survive a level save**, because the API that
makes it persist is `AActor::AddInstanceComponent` and that is not exposed to script.

**In place, not swapped.** Replacing each pig with `BP_Livestock_Pig` would work and would throw away
the dressing - the per-instance animation, the pose variety, the placement. This adds a
`UGSInteractableComponent` and touches nothing else, so the pig goes on eating and can now be lifted.
Michael's instinct here was better than my first proposal, which was to swap them.

**`UGSInteractableComponent::InitialiseAsCarryable`** was added rather than six public setters. The
authored fields are protected on purpose: one initialiser, called once at dressing time, is the only
moment any of them should change. Two of its defaults are wrong for livestock and it fixes both -
`bConsumeOnComplete` defaults **true** because the component was written for chests, which would make
the animal vanish when picked up.

**The interaction point is derived from the actor's bounds, and that is not cosmetic.** First runtime
test: every field on the pig was correct - available, carryable, loot 40, verb `Interact.Carry` - and
focus was still refused. `UGSInteractionComponent::HasLineOfSight` traces eye -> interaction point,
and a decorative animal's actor **origin sits ON the terrain**, so the trace hit `Landscape`
**15 uu short** and the focus was silently denied. Measured, not guessed. Using the bounds centre puts
the point inside the body and works for pig, sheep and chicken without three hand-tuned numbers.

**The Loot branch.** `EGSHordeOrder::Loot` has been on the command wheel and consumed by nobody;
`BTTask_PickUpCargo` and `BTTask_DeliverCargo` have existed since #141 and appeared **in no tree**.
Added to `BB_HordeGoblin`: a **`Cargo`** object key, the only one missing - `PickUpCargo` writes it,
`DeliverCargo` clears it. Added to `BT_HordeGoblin`: a **`Loot` Sequence at index 2** gated on
`OrderVerb == 4`, containing MoveTo(OrderSubject) -> PickUpCargo -> MoveTo(DeliveryLocation) ->
DeliverCargo.

**Index 2 is a design decision, not an accident:** below Block and Melee Attack so a laden goblin
still defends itself, above Menace Orbit so it does not wander off to posture while carrying a pig.
`flow_abort_mode = LOWER_PRIORITY` so issuing the order interrupts what the goblin was doing rather
than waiting for it to finish.

The gate uses **arithmetic EQUAL** where every other branch in this tree uses **basic SET** - because
`OrderVerb` is an enum and "is set" cannot express "equals this verb".

## Evaluate

**The whole chain ran, and the last line is the ticket:**

```
LogGSHorde:    Order: Loot at V(-1033, 30190, -121) on A_Pig_SitLoop7_5.
BB per goblin: verb=4  subj=A_Pig_SitLoop7_5  cargo=A_Pig_SitLoop7_5
pig:           attached to BP_HordeGoblin_C_2, then C_6, moving y 30190 -> 27434 -> 23548
LogGSLootBank: The portal on 'BP_GS_RunicSite_C_1' swallowed 'A_Pig_SitLoop7_5'
               for 40 loot. 40 banked here across 1 item(s).
```

**That pig was set dressing an hour ago.** It was converted in place, ordered, shouldered, carried
~125 m across the island by two different goblins in relay, and banked for 40.

**Persistence verified the way this project has learned to verify it** - saved, unloaded to another
level, reloaded, re-counted from disk: **46 pigs, 46 still carryable**, `A_Pig_Eat` still assigned,
prompt still "Carry the pig". Same for the blackboard key and the tree branch, both re-read after save.

**A near-miss worth recording:** I concluded BT nodes were not scriptable after
`get_editor_property("operation")` failed on `BTDecorator_Blackboard`. They are - the properties are
`arithmetic_operation` / `basic_operation`. `discover_python_class` said so in one call. I was one
step from telling Michael a tool could not do something I had not actually tested, which is a mistake
this project has explicitly warned about before.

**NOT ESTABLISHED: Michael has not seen it.** Everything above is script-driven and log-verified.

**Also observed and NOT chased:** goblins die steadily once they reach the village - the militia and
Knight are doing their job, but nobody has judged whether the courier survives the trip often enough
for the verb to feel usable. And the level logs *"No AGSWarren in this level"*, so delivery falls back
to the runic site; the banking above happened through the site's own overlap.

## Refine

**The helper is the reusable half and the branch is the specific half.** `MakeActorCarryable` works on
any actor in any level - sheep (`SK_Sheep`) and chickens (`SK_Chicken`) exist as assets with full
animation sets and are placed nowhere yet, so livestock stops being pig-specific the moment someone
places them.

**Deliberately not done:** no sheep or chickens placed, no second level dressed, and the pig's 40 /
sheep 25 / chicken 10 values are GDD 10's numbers rather than anything tuned in play.

**Backup left in place:** `/Game/AI/_Backup_BT_HordeGoblin_pre300` - the horde's brain is not a thing
to edit without a way back, and `git checkout` is blocked in this environment. Delete it once Michael
has watched the branch work.

---

## Watched, and two things came out of watching

**The hand-off.** Michael: *"amazing, it handed the loot off to another goblin."* This is emergent, not
authored: every goblin in the warband receives the same `OrderSubject`, so if the carrier is
interrupted or another reaches the cargo first, the pig changes hands and the relay carries on.
Observed twice - `BP_HordeGoblin_C_2` -> `C_6` on the first run, and by Michael on his own. It reads
as a warband passing loot down a line, which is better than it has any right to be. **Recorded as
emergent so nobody "fixes" it later without knowing it was liked.**

The flip side, unjudged: ten goblins all target one pig. Whether the whole warband should swarm a
single sack, or the order should distribute across nearby cargo, is a design question nobody has
answered.

**Ordering is finicky, and the cause is known.** Michael first hit *"it didn't let me put an order on
the pig."* The order path works - verified from the console at 450 uu, `Order: Loot on
BP_Livestock_Pig_C_0` - and the reason it misses is in `TraceForOrder`'s own comment: the subject
sweep reaches **8000 uu with a 60 uu radius**, so a slightly-off crosshair resolves nothing and the
order **degrades to a location with no subject** rather than being refused. `OrderVerb` is still Loot,
so the branch activates, `MoveTo(OrderSubject)` has nothing to walk to, and the sequence fails
silently - which reads exactly like "the order didn't take".

**Not fixed here.** Widening the radius or refusing a subjectless Loot order outright are both real
options and both change feel; that is Michael's call, not a thing to tune inside this ticket. The
collision theory was tested and **disproved** first: `BP_Livestock_Pig`'s `OverlapAllDynamic` mesh and
the decorative pigs' `PhysicsActor` mesh are **both** found by the object-type sweep.
