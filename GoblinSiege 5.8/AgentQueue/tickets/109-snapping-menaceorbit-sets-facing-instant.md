---
id: 109
title: Snapping: MenaceOrbit sets facing instantly, up to 167 degrees in one frame
agent: claude-gobkit
status: done
claimed: 2026-08-10T00:37Z
build: none
waiting_on:
evaluated: 2026-08-10T00:38:22Z
files: 
  - Source/GoblinSiege/AI/Tasks/BTTask_MenaceOrbit.cpp
---

## Goal

Snapping: MenaceOrbit sets facing instantly, up to 167 degrees in one frame

## Generate

Michael, after #108: "the snapping is better, but still there read the data." So this one is measured
first and changed second.

**`BTTask_MenaceOrbit::TickTask` assigned the facing yaw outright every tick.** That is invisible while
the bearing drifts a degree a frame, and a hard snap the moment it does not - a victim running past an
orbiter swings the bearing through 180 degrees and the agent flipped to face it in ONE frame.

Now turned at `AGSCharacterBase::GetTurnRateRadPerSec()` - the character's own dial, already used for
this exact purpose by `UBTTask_Block`, so a goblin turns like a goblin instead of at a rate invented
here.

## Evaluate

**Instrumented with a per-frame recorder** (`register_slate_post_tick_callback`) rather than sampled
between MCP calls, because two samples in one call land on the same frame and say nothing. 65 samples
of a live patrol fight, recording target identity, goal position and yaw TOGETHER so they could be
correlated:

```
agent                switches  goalJumps  maxGoalJump   maxYawStep
BP_HordeGoblin_C_3          3         25       1236.8        149.5
BP_HordeGoblin_C_4          3         19        868.5        166.9
BP_HordeGoblin_C_2          4         16       1286.7        120.0
BP_CastleGuard02_C_1        6         10        668.1        121.8
```

**Two findings, and the second is the one that stopped me changing the wrong thing.**

1. **Yaw steps of 121, 149 and 166.9 degrees.** A near-180-degree flip in one step is a snap by
   definition, and it is in code I wrote in #107.
2. **The goal jumps are mostly NOT target thrash** - "0 of 12", "0 of 16", "1 of 25" of them coincided
   with a target switch. So the goal moving 1000uu+ is a ring slot legitimately following a victim
   that is sprinting; guards carry `MaxWalkSpeed` up to 1239uu/s. **I nearly "fixed" the targeting.**

**A methodology error I made on the way, worth recording because it produced a confident wrong
answer.** I first measured goal jumps and target identity in two SEPARATE captures and concluded
"zero target switches, so the goal has a second writer". The identity capture happened to run late in
a fight when almost everyone was dead. Correlating both in ONE capture reversed the conclusion.
Separate captures of the same phenomenon at different times are not evidence about the same phenomenon.

**NOT BUILT OR RE-WATCHED YET.** Michael has to look at it; a yaw-step histogram is not a judgement
about whether it reads smoothly.

## Refine

**Rate-limited the turn rather than lerping toward the target.** A lerp with a fixed alpha is
frame-rate dependent and still snaps on a big delta; clamping the per-tick step to the character's
turn rate is bounded by construction and reuses a number that already exists.

**Did NOT touch targeting or the ring**, despite the goal-jump numbers looking alarming, because the
correlation says they are a slot doing its job.

**Left undone:** `BTTask_MeleeAttack` also assigns rotation outright, clamped to
`MaxFacingSnapDegrees`. That is a smaller, deliberate snap (#089 needs it to land a swing) but it is
the same mechanism, and if Michael still sees a flick at the moment an attack starts, that is where
it lives.
