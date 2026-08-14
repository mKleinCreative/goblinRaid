---
id: 105
title: Patrol combat run: baseline crowding measurement
agent: claude-gobkit
status: done
claimed: 2026-08-09T20:31Z
build: none
waiting_on:
evaluated: 2026-08-09T20:34:01Z
files: 
  - Content/Maps/Test/L_CombatArena.umap
---

## Goal

Patrol combat run: baseline crowding measurement

## Generate

Michael: "people kinda crowd around each other still, let's do a full patrol combat run."

**A patrol placed in `L_CombatArena` to his squad ruling** - 4 militia (`Patrol_Militia_1..4`,
CastleGuard01/02) + 2 archers (`Patrol_Archer_1..2`, Erika) - fought by the player and a 10-goblin
horde. 19 agents. Level saved, so the run is repeatable.

**Two measurements written in Python, because the existing readout cannot see this problem.**
`GS.Combat.CrowdStats` (`GSDebugCommands.cpp:388`) reports engaged / swinging / weight / slots and
**nothing spatial at all** - which is exactly why "the caps hold" and "they still crowd" have both been
true this whole time. It was never able to measure the complaint.

1. **Capsule clearance** - gap between capsule SURFACES for all 153 agent pairs, not centre distance,
   so it is comparable across a 52uu goblin and a 70uu guard.
2. **Ring occupancy** - for each victim, the bearing of every attacker within 400uu, reported as the
   largest empty arc against the even spacing 360/n. This is the one that shows clumping.

## Evaluate

**BASELINE, and it confirms the audit by measurement rather than by reading:**

```
victim                  n   largest empty arc      distances to victim
BP_CastleGuard01_C_2    4    307.7deg (even=90)    [330, 248, 248, 259]   <-- 4 attackers in a 52deg arc
BP_CastleGuard02_C_1    3    250.9deg (even=120)   [213, 194, 308]
BP_CastleGuard02_C_0    3    212.1deg (even=120)   [323, 218, 215]
min capsule clearance during the melee: -29.1uu   (capsules interpenetrating)
```

Two things this proves that reading the code only suggested:

- **Attackers stand at 194-374uu, clustered near 250-330** - `BTTask_MenaceOrbit`'s shared
  `OrbitRadius = 300`. **Not** the claimed ring radius of 180. The ring slot is claimed, converted to a
  world position and written to the blackboard, and then never walked to.
- **They occupy one arc, not a ring.** Four attackers inside 52 degrees of a circle that has 8 slots.

**A THIRD finding I was not looking for: the crowd is STATIC.** Sampled at t=77.7 and again at t=98.7 -
every bearing and every distance identical to the decimal across 21 seconds of world time. The fight
does not merely space itself badly, it settles into a motionless clump. `MenaceOrbit` is supposed to
strafe, so this is not it working badly; it is something not running. **Not yet diagnosed** - it needs
its own PIE pass with `GS.Combat.LogAI 1`, and it may well be the real complaint rather than the
geometry.

**Not verified:** whether the static clump is agents stalled in the tree, or simply everyone dead/idle
with the engagement lists never cleared. I am not going to guess between those.

**No fix applied in this ticket.** This is measurement only; the design is out at a workflow.

## Refine

**Measured capsule-surface clearance rather than centre distance.** Centre distance is meaningless
across mixed capsule radii - 137uu is touching for two guards and comfortable for two goblins.

**Added the angular metric after the clearance metric turned out to be weak evidence.** Only 1-2 pairs
of 153 ever interpenetrated, which would have let me report "barely any overlap, looks fine" and miss
the complaint entirely. The largest-empty-arc number is what actually matches what Michael can see.

**Deliberately left undone:** the static-clump diagnosis, and folding these two metrics into
`GS.Combat.CrowdStats` so the readout can finally see position - worth doing in the same C++ batch as
the fixes.
