---
id: 093
title: Mitigation must never zero a hit: a minimum damage floor
agent: claude-plate
status: done
claimed: 2026-08-09T03:41Z
build: required
waiting_on:
evaluated: 2026-08-09T03:45:07Z
files: 
  - Source/GoblinSiege/Combat/GSDamageExecCalculation.cpp
---

## Goal

Mitigation must never zero a hit: a minimum damage floor

## Generate

Michael's ruling: *"There should be always a little bit of damage regardless."*

`GS.Combat.MinimumDamage` (default 1.0), applied once at the very end of
`UGSDamageExecCalculation` so it catches EVERY mitigation path - block, plate, flat armour, the race
matchup, the NPC-vs-NPC scalar - rather than needing a guard in each of them.

Gated on `RawDamage > 0`, so it can only lift a real blow. An effect that was always going to do
nothing must not be promoted into doing one point.

## Evaluate

**VERIFIED IN PIE: not one hit resolves to 0.0 any more** (grep count of `= 0.0` over the whole
fight: zero). The cases that used to vanish now read:

```
BP_HordeGoblin_C_7 -> BP_KnightDPelegrini_C_0   PLATE-FRONT - armor 6.0  = 1.0   (HP 70/75)
BP_HordeGoblin_C_1 -> BP_KnightDPelegrini_C_0   BLOCKED PLATE-FRONT      = 1.0   (HP 69/75)
```

**And it fixed the thing #091 flagged for a human.** A knight is no longer invulnerable to the
warband from the front: measured live, goblins chipped one 75 -> 71 -> 70 -> 69 -> 68 -> 67 and a
second to 60. He is still enormously resistant frontally - which is the design - but the fight
resolves instead of stalling forever.

**Reach is wider than knights, deliberately, and worth knowing:**
- **Blocks now chip.** A perfect block used to be 0.0 and is now 1.0. That is a real change to how
  blocking feels for the PLAYER too, not just for AI.
- **Fire shares this exec** (`UGSGE_FireDamage` uses the same calculation class), so every fire tick
  now has a floor. Good on balance - 6 points of armour was quietly making a knight fireproof
  against the goblins' main equaliser - but it does mean a burning character can no longer be
  fully immune to a weak fire source.

**Not verified:** whether 1.0 is the right value at the top end (a heavily-resisted heavy still
floors at 1, same as a jab), and no measurement of the fire DoT change over a full burn.

**Owed to `AGENT_STATE.md`:** a DECISION line, and the note that blocks now chip.

## Refine

**One floor at the end rather than a clamp per mitigation.** Each mitigation could have been given
its own minimum, but they compound - it was three multipliers stacking that produced the 0.00 in the
first place - so the only place that can honestly enforce "a hit always does something" is after all
of them have run.

**Deliberately left undone:** scaling the floor with the incoming blow (e.g. a floor of 5% of raw, so
a heavy always beats a jab through plate). That is a better rule and a bigger conversation; a flat
point satisfies the ruling and is one cvar to move.
