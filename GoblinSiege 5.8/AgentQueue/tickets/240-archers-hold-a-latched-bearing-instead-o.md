---
id: 240
title: Archers hold a latched bearing instead of re-racing for a melee ring slot
agent: claude-acf
status: done
claimed: 2026-08-21T19:15Z
build: none
waiting_on: Extended under #247 - the latched bearing was right but it still rewrote the hold point every tick. #247 adds the range band. Watch them together.
evaluated: 2026-08-21T22:40:53Z
observed: 2026-08-22T03:55:52Z | Archers stopped re-racing for melee ring slots and held a bearing; flick/s fell from 2.0 to 0.2 once the range band was added on top
scenario: 8s capture, two Erika archers engaged, measured before and after
files: 
  - Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.h
  - Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.cpp
---

## Goal

Archers hold a latched bearing instead of re-racing for a melee ring slot

## Generate

A ranged agent now **latches a standoff bearing** the first time it sees a target and holds it,
correcting only DISTANCE. It no longer claims a melee ring slot at all.

## Evaluate

**The bug was structural, and the arithmetic is the proof.** An archer used to:

1. `ClaimRingSlot` - one of six angular places at `RingRadius = 200`.
2. Take that slot's DIRECTION and stand at `StandoffRadiusOverride` (BT_Archer sets ~700) along it.

`UGSEngagementComponent`'s watchdog keeps a claim alive only while the claimant has **arrived**
(within `SlotArrivedRadius = 70` of the slot) **or is still closing** on it. An archer standing at 700
is 500 units from the slot it holds, stationary. It can never arrive and is never closing - so the
claim expires every `SlotClaimTimeoutSeconds = 6`, the archer re-races, and `ClaimSlotIn` picks the
slot nearest its CURRENT bearing, which has drifted. Result: a sideways sprint of up to a full
slot-width at 700 units, on a timer, forever.

Michael: *"twitching was the same for the archers movement. they also don't keep track of who they're
running away from."* Both halves of that sentence are the same defect - the position was re-derived
every few seconds instead of held.

**Found by measurement, not by reading.** `CombatTwitching.mp4` split to frames via `imageio_ffmpeg`
(1284 frames); per-frame motion over the viewport crop averages 3.96 with the burstiest window,
frames 1080-1130, at **13.85**. Those frames show goblins piled and blurred around the player.

**BUILT, NOT WATCHED.** What to look for: an archer that backs off in a straight line and then holds
its ground, rather than sliding around the target. It should also stop competing with swordsmen for
the six melee places - `GS.Combat.CrowdStats` should show archers no longer holding inner slots.

**A side effect worth watching for:** the latch is per-target, so an archer holds its first bearing
even if the player walks around it. That is intended - it should re-orient by turning, not by
relocating - but if it reads as an archer refusing to reposition when flanked, the latch wants a slow
decay rather than being permanent.

## Refine

Left the melee override branch in place rather than deleting `StandoffRadiusOverride`'s use there,
with a comment saying it is now unreachable for ranged agents. If nothing else ever sets an override,
both it and the branch can go.
