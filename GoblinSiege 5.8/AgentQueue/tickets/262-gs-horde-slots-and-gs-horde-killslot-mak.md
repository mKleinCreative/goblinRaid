---
id: 262
title: GS.Horde.Slots and GS.Horde.KillSlot: make the follow-slot fix testable
agent: claude-acf
status: done
claimed: 2026-08-23T22:05Z
build: done
waiting_on: BUILT. Horn, then GS.Horde.Slots, GS.Horde.KillSlot, GS.Horde.Slots again.
evaluated: 2026-08-23T22:43:20Z
observed: 2026-08-23T22:43:19Z | Michael ran GS.Horde.Slots, GS.Horde.KillSlot and GS.Horde.Slots again; both dumps printed the full per-goblin table and the kill picked slot 4 from the middle of eight as intended
scenario: live PIE with an eight-goblin band summoned by the horn
files: 
  - Source/GoblinSiege/Combat/GSDebugCommands.cpp
---

## Goal

Michael: *"239 isn't easy to test without a command."*

## Generate

Two commands in `Combat/GSDebugCommands.cpp`:

- **`GS.Horde.Slots`** - one row per live goblin: follow slot, distance from its formation post,
  distance from the player. Explicitly flags two goblins sharing a slot, which is the defect #239
  fixed and is invisible on screen: two goblins quietly standing in one place looks like one goblin.
- **`GS.Horde.KillSlot [slot]`** - kills the goblin holding that slot. With no argument it takes one
  from the MIDDLE of the band, because that is the case worth watching. Killing the last goblin
  proves nothing - nobody is behind it to be renumbered - and killing the first is the easiest case
  to pass by accident.

## Evaluate

Built. **Not yet run** - both need a live horde, so the first real exercise is Michael's.

The test they enable: horn, `GS.Horde.Slots`, `GS.Horde.KillSlot`, `GS.Horde.Slots` again. Every
surviving goblin should report the same slot number in both dumps, and duplicates should read zero.

## Refine

Nothing. These exist because the thing they measure cannot be seen, which is the same reason
`GS.Anim.Snapshot` and `GS.AI.LogLocomotion` exist.
