---
id: 303
title: Goblins steal cargo off each other and are eaten as couriers
agent: claude-warren
status: done
claimed: 2026-08-25T05:27Z
build: required
waiting_on: Michael: order Loot on a pig and count the goblins - they should all survive and the pig should bank.
evaluated: 2026-08-25T05:35:27Z
observed: 2026-08-25T06:58:08Z | Michael played it and confirmed the goblins no longer vanish when they turn loot in - ordering a Loot no longer eats the warband, where before five goblins disappeared for one pig and the pig arrived unbanked.
scenario: Michael playing in PIE on L_Tutorial_Island, ordering his horde to loot livestock and watching what happens to the goblins that carry it.
files: 
  - Source/GoblinSiege/Interaction/GSCarryComponent.h
  - Source/GoblinSiege/Interaction/GSCarryComponent.cpp
---

## Goal

Michael, playing #300: *"the A_Pig_SitLoop7 is causing goblins that turn it in to disappear but the pig
doesn't loot."*

Both halves are one bug, and it is the same bug as the thing he liked yesterday.

## Generate

**`UGSCarryComponent::StartCarry` asked "am I carrying?" and never "is it carried?"**

Every goblin under one Loot order shares an `OrderSubject`, so all ten walked to the same pig. The
second to arrive was not carrying anything, so `StartCarry` succeeded and re-parented the pig off the
first. The victim kept a **stale `CarriedActor`**, so `IsCarrying()` stayed true - it walked to the
delivery point, `BTTask_DeliverCargo` accepted it, `PutDown()` teleported a pig it no longer held, and
`NotifyCourierDelivered` **ate the goblin**. Repeat per thief.

Michael's log: **five** couriers consumed, one pig, nothing banked.

**This is exactly the "hand-off" recorded as emergent in #300** - *"amazing, it handed the loot off to
another goblin"* - and it was theft, not relay. It survived a play session, an observation and a
written ticket because a warband passing loot down a line is a better story than the truth. Nobody
counted the couriers.

**Fix:** `IsActorCarried(const AActor*)` - a static on the carry component - and `StartCarry` refuses
an object somebody already holds.

**In the component, not the BT task.** The invariant is *one object, one carrier*; a rule enforced in
one behaviour-tree node is a rule the next caller will not know about. It is asked of the object's
**attach parent** rather than by searching the world - carrying is expressed as an attachment, so the
carrier is one hop away and there is nothing to iterate. A parent that carries something *else* is
correctly not a carrier of this, which a bare "am I attached to a pawn" test would get wrong.

## Evaluate

**Build succeeded in 49s.** Same scenario both times - ten goblins, one converted decorative pig,
`GS.Horde.Order Loot`:

```
BEFORE   couriers consumed 5   pig banked: NO
AFTER    couriers consumed 0   goblins alive 10   pig banked: 40 loot, 1 item
```

`LogGSLootBank: The portal on 'BP_GS_RunicSite_C_1' swallowed 'A_Pig_SitLoop7_5' for 40 loot.`

**Zero couriers consumed is the correct outcome and worth explaining**, because "the counter went to
zero" could read as the verb no longer working. The carrier now survives the whole trip, and the site
banks what it carries the moment it crosses the banking sphere (`BankFromOverlap` -> `BankCarriedLoot`,
"a pawn banks what it CARRIES, not itself"). The pig is consumed there, so `DeliverCargo` never fires
and the goblin is never eaten. **The loot arrives and the goblin lives** - strictly better than the
old behaviour, in which the pig did not arrive and five goblins died for it.

**Consequence to watch:** `UGSHordeSubsystem::NotifyCourierDelivered` is once again reachable only
when a courier reaches a delivery point OUTSIDE any banking sphere. It was written in #069, first
called in #141, and on this map it now goes quiet again.

**NOT ESTABLISHED: Michael has not re-run it.** The counts above are from the log, not from watching.

## Refine

**The lesson is about the observation, not the code.** #300 closed with an `observed:` line describing
the hand-off as a warband relaying loot. That observation was real - Michael saw it - and it was a
bug wearing a feature's clothes. What would have caught it is the question nobody asked: *how many
goblins should one pig cost?*

**Deliberately not done:** no real hand-off. If a relay is wanted - a tiring courier passing cargo to a
fresh one - that is a designed behaviour with a reason to trigger, not an accident of two goblins
reaching a sack in the same second.
