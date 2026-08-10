---
id: 108
title: Jitter: MenaceOrbit and MoveTo fight over two different radii
agent: claude-gobkit
status: done
claimed: 2026-08-10T00:26Z
build: none
waiting_on:
evaluated: 2026-08-10T00:28:49Z
files: 
  - Source/GoblinSiege/AI/Tasks/BTTask_MenaceOrbit.cpp
  - Source/GoblinSiege/AI/Tasks/BTTask_MenaceOrbit.h
---

## Goal

Jitter: MenaceOrbit and MoveTo fight over two different radii

## Generate

**A regression I introduced in #107.** Michael, watching the standalone game: "there's a lot of
jittering and snapping happening from the AI's side. it looks very buggy."

**Two nodes disagreed about where an agent belongs, and took turns winning.** `BTService_AcquireTarget`
writes the claimed ring slot at `RingRadius` **180** into `TargetLocation`, which the `MoveTo` sibling
walks to. My #107 orbit took the same slot's BEARING but stood at `OrbitRadius` **300** along it. And
this task hands the branch back every `OrbitReevaluateSeconds` (0.6s) so the Selector can notice a
freed token - at which point MoveTo ran and dragged the agent in to 180, and on the next cycle the
orbit pushed it back out to 300. A 0.6-second ping-pong, forever.

**Fix: one station.** The orbit now READS `TargetLocation` (new `StationKey`, defaulting to the same
key MoveTo uses) instead of computing its own radius. Both nodes now aim at the identical point, so
handing the branch back is a no-op rather than a fight.

**Second jitter source, found while fixing the first and not reported by anyone.** The on-station
shuffle strafed tangentially with no restoring term, so an agent drifted off station until the 60uu
tolerance tripped and yanked it back - a slow drift-and-snap of its own. The shuffle now carries a
spring back toward the station that strengthens with the error, so it circles ABOUT the station.

## Evaluate

**Diagnosed by measurement, not by staring at the code.** In PIE, the MoveTo goal and the actual
standing distances disagreed exactly as predicted:

```
BP_CastleGuard01_C_1   dist 338.1   MoveTo goal radius 180.0
BP_CastleGuard01_C_2   dist 341.8   MoveTo goal radius 180.0
BP_HordeGoblin_C_1     dist 359.0   MoveTo goal radius 180.0
BP_CastleGuard02_C_0   dist 186.4   MoveTo goal radius 180.0   <- the other half of the ping-pong
```

Agents split between ~340 and ~185 against a single goal at 180 is the two nodes fighting, frozen at
whichever one last won.

**What the un-clumping loses: nothing.** The separation came from the slot being exclusive and
per-agent, not from the extra 120uu of radius. Waiting attackers now stand on the ring at 180 rather
than a band at 300, so they are closer in than #107 intended - if that reads as crowded, the fix is to
move the STATION (one number, in the service, one place) and not to reintroduce a second opinion.

**NOT YET BUILT OR RE-WATCHED.** The diagnosis is measured; the fix is not. Michael has to look at it -
jitter is a thing you see, and I have already once reported a crowd baseline that turned out to be
corpses, so a number saying "no oscillation" is not sufficient here.

**Owed to AGENT_STATE:** two BT nodes must never both write movement for the same agent against
different destinations - the Selector will alternate them and it reads as a bug.

## Refine

**Read the station from the blackboard rather than teaching the orbit to compute the same thing.**
Two places computing one number is how this bug happened; the second copy would have drifted from the
first the next time someone tuned a radius.

**Fixed the shuffle drift in the same pass** even though nobody reported it. Shipping a fix for jitter
while leaving a second oscillation in the same function would have looked like the first fix failed.

**Deliberately left undone:** the waiting band. #107 wanted unslotted agents held further out than
slotted ones; that needs the SERVICE to write a different radius by token state, which is a real
change to a file I have not claimed, and is not worth doing until Michael says the ring at 180 reads
too tight.
