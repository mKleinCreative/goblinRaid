---
id: 091
title: Knight workover: directional plate - front shrugs off a dagger, gaps do not (GDD 217)
agent: claude-plate
status: done
claimed: 2026-08-09T03:23Z
build: required
waiting_on:
evaluated: 2026-08-09T03:32:13Z
files: 
  - Source/GoblinSiege/Combat/GSDamageExecCalculation.cpp
---

## Goal

Knight workover: directional plate - front shrugs off a dagger, gaps do not (GDD 217)

## Generate

GDD §217: *"knights whose plate shrugs off a straight-on dagger rush but not a takedown from the
shadows or a bow shot placed at the gaps."* Flat armour could not express any of that - 6 points
mitigated a blade in the back exactly as well as one on the breastplate, so every counter the design
names bought the player nothing and a knight was a militiaman with 2.5x HP.

Armour is now **positional**, entirely inside `UGSDamageExecCalculation`:
- **Straight-on** (within `GS.Combat.PlateArc`, 150 deg): damage x `GS.Combat.PlateFrontalScalar`
  (0.3) and THEN flat armour, so plate compounds rather than merely subtracting.
- **Flank / back / takedown**: flat armour skipped entirely. "Not a takedown from the shadows" is a
  statement about what WORKS, and a 25% discount would not read as working.
- **`Damage.Bow`**: bypasses plate at any angle, no arc test. The design's own named counter, so it
  is absolute rather than a modifier.

Costs no new data: it keys off the armour value that already separates a knight (6) from a
militiaman (0), so an unarmoured defender never touches the path. Reuses the block arc's flat-dot
facing test - one idea about what "in front of me" means, not two.

`GS.Combat.LogDamage` now prints `PLATE-FRONT` / `PLATE-GAPS(flank)` / `PLATE-GAPS(bow)`, because a
1.5-damage hit and a 25-damage hit otherwise differ only by an angle nobody can reconstruct later.

## Evaluate

**PIE-VERIFIED, and it reads exactly like the design line.** Player against a knight:

```
PLATE-FRONT        raw 25.0  - armor 6.0  =  1.5   (HP 75/75)
PLATE-FRONT        raw 45.0  - armor 6.0  =  7.5   (a full heavy, straight on)
PLATE-GAPS(flank)  raw 25.0  - armor 6.0  = 25.0   (HP 75/75)
PLATE-GAPS(flank)  raw 45.0  - armor 6.0  = 45.0   (HP 20/75)
```

A frontal three-hit combo (25/30/45) took a knight from 75 to 63. The same combo from his flank took
him from 75 to 20. That is "you cannot beat this man from the front, you have to get around him",
which is what §217 asks for.

**Positioning composes with #090 exactly as hoped, and this surprised me.** The reserved ring slots
put attackers all round a target, so measured live against a knight: goblins sat at 174.9 and 92.9
degrees off his front - in the gaps - purely as a side effect of slot claiming. Nobody wrote
flanking behaviour; the crowd system produced it.

**THE NUMBER THAT NEEDS A HUMAN: a goblin's frontal hit is exactly 0.00 damage.** Three multipliers
compound - `25 x 0.55 (NPC-vs-NPC) x 0.3 (plate) - 6 (armour) = 0`. From the gaps the same goblin
does 13.75. So the warband can hurt a knight only from behind, and a knight standing still facing
one goblin is *literally invulnerable* to it. Arguably correct per GDD §76, which names the Shaman's
armour-ignoring magic as "the answer to ... a knight's armor" - but "immune" and "heavily resistant"
feel very different in play, and I am not the one who should pick. `GS.Combat.PlateFrontalScalar` is
live-tunable; raising it to ~0.6 gives chip damage (2.3) instead of nothing.

**Not verified:** the takedown path (it goes through `KillOutright`, which bypasses damage entirely,
so it should be unaffected - but nobody has performed one on a knight); and no measurement of
whether a knight is now *too* survivable in a full raid.

**Owed to `AGENT_STATE.md`:** a DECISION line - armour is directional, and a bow ignores it.

## Refine

**Skipped flat armour entirely at the gaps rather than halving it.** A partial discount would have
kept knights feeling like damage sponges from every angle, which is the exact failure being removed.

**Put the arc at 150 rather than the block arc's 140.** A shield is aimed; a breastplate simply faces
where the man faces, so plate should cover slightly more than a guard does.

**Deliberately left undone:** the frontal-scalar question above (his call, and it is a cvar), poise /
unstaggerable knights (offered and not chosen), and any change to the takedown path.
