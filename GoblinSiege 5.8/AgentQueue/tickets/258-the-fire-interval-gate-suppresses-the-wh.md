---
id: 258
title: The fire-interval gate suppresses the whole draw, so a quick second shot shows no bar at all
agent: claude-acf
status: done
claimed: 2026-08-23T19:05Z
build: none
waiting_on: BUILT. Fire, then immediately press again - the bar should appear straight away.
evaluated: 2026-08-23T21:42:23Z
observed: 2026-08-23T21:41:42Z | Michael fired and immediately pressed again - the timing bar appears straight away instead of being suppressed by the fire interval
scenario: ranged mode in play, back-to-back shots
files: 
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
---

## Goal

Michael: pressing attack again straight after a release showed no timing bar at all.

## Generate

Removed the fire-interval gate from `Input_AttackPressed`'s ranged branch, so the sweep always
starts. Previously it began only when `UGSGA_BowShot::GetFireCooldownRemaining` was zero.

The gate existed to stop the bar running a full sweep for a shot the ability would then silently
refuse. It was solving a problem that barely exists: `RangedAttackCooldownSeconds` is 1.5s on every
weapon and red sits at 2.7s into the sweep, so the interval has always expired by the time any
deliberate shot is loosed. Only a near-instant tap could still be refused, and that shot would have
been a minimum-damage yellow anyway. The interval is still enforced where it always was, inside the
ability.

## Evaluate

Observed by Michael: back-to-back shots, bar appears immediately. Confirmed alongside #259 as
"looked good".

`GetFireCooldownRemaining` now has no callers. It is correct and was verified against the same
timestamp `CanActivateAbility` gates on, but dead code is dead code - it should find a use or be
deleted. Noted on #241 as well.

## Refine

Nothing further. The lesson worth keeping: a gate that suppresses FEEDBACK to prevent a rare wasted
input trades a visible mechanic for an invisible one, and reads as the feature being broken.
