---
id: 130
title: RETROACTIVE - guards get their swing commitment back after 85 percent made them crowd
agent: claude-crowd2
status: done
claimed: 2026-08-11T04:01Z
build: none
waiting_on:
evaluated: 2026-08-11T04:01:36Z
files: 
  - GoblinSiege 5.8/Content/Blueprints/Abilities/Human/GA_HU_SwordLight.uasset
---

## Goal

RETROACTIVE - guards get their swing commitment back after 85 percent made them crowd

## Generate

Retroactive claim: the edit was made in response to Michael watching combat footage, before a ticket
existed. Recording it properly rather than letting it ride in on another ticket's commit.

#125 raised `MoveSpeedScale` to 0.85 on swing AND recovery to fix "you cannot catch an archer". That
ability asset (`GA_HU_SwordLight`) is the GUARDS' - the player and horde use `GA_GS_SwordLight` - so
it also removed the slowdown that passively kept defenders apart while fighting. #125's own Evaluate
flagged this exact risk and nobody had watched it.

Michael's footage showed guards clipping into each other. Measured in a live PIE duel: the closest
pair stood **129uu apart when their two capsules need 139uu** - interpenetrating - with other pairs
at 140, 149 and 178uu against a `StandoffRadius`/`RingRadius` of 180.

Restored the ORIGINAL per-stage values on `GA_HU_SwordLight` only:

| stage | swing | recovery |
|---|---|---|
| 0 | 0.85 -> 0.55 | 0.85 -> 0.75 |
| 1 | 0.85 -> 0.55 | 0.85 -> 0.75 |
| 2 (Flurry) | 0.85 -> 0.45 | 0.85 -> 0.65 |

Per-stage originals rather than one flat number: the Flurry was always the most committal stage and
#125 flattened that distinction too.

`GA_GS_SwordLight` untouched at 0.85 - Michael's chase fix stands for the player.

## Evaluate

**Verified:** both abilities re-read from the CDO after saving - guards at 0.55/0.55/0.45 swing and
0.75/0.75/0.65 recovery, player still 0.85 across the board. Data change, no rebuild.

**NOT verified:** whether it actually stops the clipping. Michael has not watched a duel since. This
treats a contributing cause, not the root one.

**The root cause is measured and NOT fixed here.** The spacing constants (StandoffRadius 180,
RingRadius 180, AttackRange 250, OrbitRadius 300) were tuned in #105-#110 while watching goblins
(capsule r=52). Guards are r=68.6 and 355uu tall, so 180 was only ever 41uu of clearance for them,
and they are not even holding 180. Collision is not the problem - the profile is `Pawn` and blocking,
RVO avoidance is on with a 600 radius. The agents are being ASKED to stand too close.

## Refine

**Deliberately a revert rather than a compromise.** The tempting move was to split the difference at
~0.70 and hope. Restoring the exact prior values keeps one variable moving, so if the clipping
persists it cannot be blamed on a half-measure.

**Left undone, and now the subject of its own design work:** minimum-distance enforcement. Michael
asked for it directly - *"Enemies shouldn't be pressing up so close to each other... Maybe use a
volume that causes them to back up if they get in your personal space"* - and a research workflow is
producing an implementation plan. One thing that plan must confront: UE crowd avoidance separates
agents while MOVING and does little once they have stopped, which is precisely when this clipping
happens.
