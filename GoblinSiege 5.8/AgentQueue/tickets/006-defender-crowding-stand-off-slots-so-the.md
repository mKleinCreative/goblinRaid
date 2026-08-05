---
id: 006
title: Defender crowding: stand-off slots so they stop converging on one point
agent: claude-perf
status: done
claimed: 2026-08-05T20:08Z
build: required
waiting_on: 
files: 
  - Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.h
  - Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.cpp
  - Source/GoblinSiege/Characters/GSEnemyCharacter.cpp
  - Content/AI/BT_Militia.uasset
---

## Goal

Defender crowding: stand-off slots so they stop converging on one point

## Generate

**Root cause.** `BTService_AcquireTarget` wrote the target's own location into `TargetLocation`,
so every defender pathed to one identical point. Capsules block (profile `Pawn`, response to Pawn
`ECR_BLOCK`), so they never interpenetrate - they arrive on the same spot and shove, which reads
as a merged blob. RVO avoidance, added earlier this session, steers them around each other on the
way in but cannot help once they all want the same destination.

**Written (C++, NOT yet compiled - see status):**

`AI/Tasks/BTService_AcquireTarget.h/.cpp` - the service now writes a slot on a ring around the
target instead of the target itself:

- `StandoffRadius` (default 170) - each defender walks to a point this far out, on the bearing it
  is ALREADY approaching from. Nobody crosses the pack to reach a slot, and three defenders coming
  from three sides end up spread around the ring. Deliberately under `BTTask_MeleeAttack`'s
  `AttackRange` of 250, or they would stand off and never swing.
- `SlotAngleJitterDegrees` (default 30) - two defenders on the same bearing would still pick the
  same slot. A per-pawn angular offset hashed from `GetFName()` breaks the tie. Deterministic per
  pawn, so a defender does not wander around the ring frame to frame, and no shared slot registry
  is needed.
- Zero-bearing guard: a defender standing exactly on the target falls back to its own reversed
  forward vector rather than normalising a zero vector.

**`Content/AI/BT_Militia.uasset` - DONE, sequenced deliberately.** MoveTo `acceptable_radius`
150 -> 60, verified by read-back (`MoveTo acceptable_radius: 150.0 -> 60.0`, asset saved). Held
until #004/#005 closed and the build was imminent: with the previously shipped binary, which wrote
the target's raw location, a 60 radius would have made defenders press HARDER into the player -
strictly worse. At a 170 stand-off, a 150 acceptance would have let a defender stop anywhere inside
the ring, i.e. right back on top of the target, which would have silently defeated the whole fix.

## Evaluate

**Verified:** nothing. This is the honest state - the code is written and has never compiled, let
alone run. Rule 4 and `gsqueue.ps1 buildgate` (exit 1) forbid me compiling while #004 and #005 are
open, and both of those require a build of their own.

**Queue position, checked not assumed:** `check -Id 6` returned CLEAR on all four claimed files -
#004/#005 claim `GSPlayerCharacter.cpp/.h`, `GSAimComponent`, `GSGA_BowShot`, `GSGA_TorchToss`,
`GSDebugCommands`, `GSWeaponComponent`, `GSArrowProjectile`, `IMC_Default` and two aim materials.
No overlap with `BTService_AcquireTarget`, `GSEnemyCharacter.cpp` or `BT_Militia`. I have right of
way to write; I do not have right of way to build.

**Reasoning that is not evidence, and should be treated as a hypothesis:** that a 170 stand-off
plus a 30 degree per-pawn jitter is enough separation for three defenders. Capsule radius is
~68.6, so two adjacent slots need roughly 140 of arc between them to not touch. At 170 radius,
30 degrees of jitter spans ~89 units of arc - that is NOT guaranteed separation for two defenders
sharing a bearing, only an improvement on zero. If PIE still shows two of them merged, raising
`SlotAngleJitterDegrees` or `StandoffRadius` is the first dial to turn, and a real slot-reservation
system is the honest fix if dials are not enough.

**Owes AGENT_STATE.md** a BUILT/DECISIONS line once it is verified in PIE - not before. Nothing
written yet.

## Refine

Considered and rejected:

- **Raising `AvoidanceWeight` above 0.5** on `GSEnemyCharacter`. The file is claimed and it would
  be one line, but changing two crowding variables in one build makes the PIE result unreadable -
  if it improves, I would not know which did it. The stand-off is the substantive change; RVO is
  already in and demonstrably insufficient on its own.
- **A shared slot-reservation subsystem** (defenders claim numbered slots around a target). It is
  the correct answer for a full patrol and the bearing heuristic is not. It is also a lot of new
  machinery to put in unbuilt and untested behind a closed build gate, for three defenders. If the
  dials do not hold at six, this is the thing to build.

Left undone on purpose: the `BT_Militia` acceptable_radius edit, sequenced deliberately as above.
