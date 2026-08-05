---
id: 008
title: PIE-verify defender crowding, tune stand-off dials if they do not hold
agent: claude-perf
status: done
claimed: 2026-08-05T20:41Z
build: required
waiting_on: a normal human playtest - walk to the defenders inside the village and look
files: 
  - Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.h
  - Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.cpp
  - Content/AI/BT_Militia.uasset
---

## Goal

PIE-verify defender crowding, tune stand-off dials if they do not hold

## Generate

**No files changed.** Measurement only - the dials were never turned, because the measurement
never got clean enough to justify turning them. PIE on L_Tutorial_Island, sampling defender
positions, velocities, blackboard `TargetLocation`, navmesh projections and `move_to_location`
results across roughly thirty samples.

## Evaluate

**VERIFIED - the slot assignment works.** With all three defenders engaged, the blackboard reads:

```
BP_CastleGuard01_C_0   slot 170 @  -34 deg
BP_CastleGuard02_C_0   slot 170 @    7 deg
BP_PeasantMan_C_0      slot 170 @   31 deg
```

Every slot is exactly `StandoffRadius` (170) from the player and every bearing is distinct. That
is precisely what #006 set out to do: three defenders now want three different points instead of
one. Earlier samples with the pack out of range showed `TargetLocation` = 3.4028e38 (FLT_MAX, the
unset sentinel), confirming `AcquireRadius` gating clears the key correctly too.

**NOT VERIFIED - the thing that actually matters.** Whether they *stop* separated once they arrive.
They never arrived in any sample. Closest pairwise stayed >= 250 throughout, never near the 137
merge threshold, but that number is worthless as evidence: they were still hundreds of units out,
so it measures their starting spread, not the fix.

**My test rig was unsound, and that is the main finding.** Three separate ways it corrupted itself:

- Teleporting the player to computed spots dragged the defenders ~4000 units chasing him, out of
  `GEN_NavBounds_Village` (centred -13000,57000). Off the navmesh they freeze: `move status: IDLE`
  and a direct `move_to_location` returns `FAILED`, not `ALREADY_AT_GOAL`. Nothing wrong with the
  fix - I walked them off the map.
- The player now respawns at the runic site, thousands of units away, so every death silently
  ended the engagement and left them idle out of range.
- Resetting everyone to a nav-projected village anchor at `nav.z + 187` produced the clearest
  contradiction: velocity 826-851 uu/s with position frozen to the unit across 32 seconds of game
  time. They were pushing into geometry, running on the spot. `move_to_location` returned
  `REQUEST_SUCCESSFUL` there, so pathing was fine - the placement was not.

**What this owes AGENT_STATE.md** (FAILED): teleporting AI actors to synthesise a combat test does
not work in this level. They path out of the navmesh volume, or into geometry, and the resulting
numbers look like AI bugs. Engagements must be produced by moving the *player* normally inside the
village, not by placing actors.

## Refine

Changed in response to my own evaluation: nothing in the code. I came close to turning
`SlotAngleJitterDegrees` up on the strength of the frozen-position samples, and that would have
been tuning a dial against an artefact of my own test rig - the same class of error as #002's
"GameThread bound" verdict, which I have already made once this session.

**Deliberately handed back rather than forced.** The remaining question is "do three defenders
standing at their slots look separated" - which is thirty seconds of a human walking up to them in
the village, and which I have now spent far longer than that failing to synthesise. Michael should
play it and look. If they still merge, the dials are `SlotAngleJitterDegrees` (30) and
`StandoffRadius` (170), and the arithmetic from #006 still stands: at 170 radius, 30 degrees spans
~89 units of arc against a ~137 capsule-touch distance, so the jitter is under-sized for two
defenders sharing a bearing and is the first thing to raise.
