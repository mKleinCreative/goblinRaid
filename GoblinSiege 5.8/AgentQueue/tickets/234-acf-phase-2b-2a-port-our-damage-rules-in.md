---
id: 234
title: ACF Phase 2b-2a: port our damage rules into a UACFDamageCalculation subclass
agent: claude-acf
status: done
claimed: 2026-08-21T04:28Z
build: none
waiting_on:
evaluated: 2026-08-21T04:32:16Z
observed: UNOBSERVED 2026-08-21T04:32:16Z - Nothing to watch by design - the calculator compiles but is not assigned to any DamageCalculatorClass, so no damage reaches it. It becomes watchable in 2b-2b when the transport switches.
scenario: none - never run
files: 
  - Source/GoblinSiege/Combat/GSACFDamageCalculation.h
  - Source/GoblinSiege/Combat/GSACFDamageCalculation.cpp
---

## Goal

ACF Phase 2b-2a: port our damage rules into a UACFDamageCalculation subclass

## Generate

`UGSACFDamageCalculation : UACFDamageCalculation` - our four damage rules expressed as ACF's
calculator. Compiles.

1. **Race matchup** - melee refuses the attacker's own side. Fire stays exempt: the torch is the
   goblin equalizer and burns everyone.
2. **NPC-vs-NPC scalar 0.55** when neither party is player-controlled.
3. **Directional plate** - 150 degree frontal arc, scaled 0.3 THEN reduced by flat armour; flank,
   back and takedown skip the armour entirely rather than reducing it.
4. **Minimum damage floor of 1**, applied LAST so it catches every mitigation path.

All four read the EXISTING cvars through `IConsoleManager` rather than declaring new ones -
`GS.Combat.PlateArc`, `GS.Combat.PlateFrontalScalar`, `GS.Combat.NPCvNPCScalar`,
`GS.Combat.MinimumDamage`. While both damage paths exist they must tune together, or an evening
spent balancing one silently leaves the other alone.

**Subclassed `UACFDamageCalculation`, NOT `UACFGASDamageCalculator`** - ruling 26's recorded trap.
That class declares `Internal_CalculateDamage` NON-VIRTUAL (`ACFGASDamageCalculator.h:30`), so
deriving from it produces a class whose base helper our code can never reach and whose own helper ACF
cannot see.

## Evaluate

**COMPILES. COMPUTES NOTHING. Deliberately not wired up.**

Nothing assigns this to `DamageCalculatorClass`, so no damage in the game goes near it. That is the
point: the calculator and the TRANSPORT are separate steps, and combat that Michael has tuned all
session should not change until the transport is watched. **This ticket cannot break anything, and
equally proves nothing.**

**Two fidelity gaps against `UGSDamageExecCalculation`, named rather than discovered later:**

- **The bow's "finds gaps" rule is MISSING.** Today it keys on `DamageTypeTag == GSTags::Damage_Bow`
  to ignore plate at any angle. ACF identifies damage by `TSubclassOf<UACFDamageType>` instead, and
  this project has authored none. 2b-2b has to create those classes and map our tags onto them, or
  archers quietly start bouncing off knights.
- **Armour still comes from `UGSAttributeSetBase`**, read through the ASC. #228 moved health to ARS,
  not armour; ruling 25 maps `Armor` to `UACFAttributeSet::PhysicalDefense` eventually and the
  comment marks the line that changes.

Also carried forward as a TODO in the source: the race check now asks `IsHostileTo`, which since #229
consults ACF teams rather than RaceTag. That is the right question but a subtly different one - a
civilian will be neither hostile nor the same race - and it wants watching the first time civilians
exist.

## Refine

Kept this as a pure addition on purpose. The alternative - write the calculator and switch the sword
over in one ticket - would have made "did the rules port correctly" and "did the transport work"
inseparable the first time something felt wrong.
