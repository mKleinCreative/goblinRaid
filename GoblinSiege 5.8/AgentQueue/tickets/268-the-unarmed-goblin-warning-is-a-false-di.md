---
id: 268
title: The unarmed-goblin warning is a false diagnostic: it fires before AGSHordeGoblin equips itself, and has now caused two misdiagnoses
agent: claude-warren
status: review
claimed: 2026-08-23T23:55Z
build: none
waiting_on:
evaluated: 2026-08-23T23:56:54Z
observed:
scenario:
files: 
  - Source/GoblinSiege/Weapons/GSWeaponComponent.h
  - Source/GoblinSiege/Weapons/GSWeaponComponent.cpp
  - Source/GoblinSiege/Horde/GSHordeGoblin.cpp
---

## Goal

The unarmed-goblin warning is a false diagnostic: it fires before AGSHordeGoblin equips itself, and has now caused two misdiagnoses

## Generate

`UGSWeaponComponent` gains `bExpectsExternalEquip`, and the null-EquippedWeapon warning is gated
behind it. `AGSHordeGoblin` sets the flag in its CONSTRUCTOR - not BeginPlay, because the component's
BeginPlay runs first and a flag set any later would arrive after the check it exists to suppress.

The warning is not deleted. Its own comment earns it: a silent null data asset cost an afternoon in
August, and it is still the single most likely state for a freshly-made character Blueprint. When
suppressed it drops to Verbose rather than vanishing, so a genuinely broken external equip is not
silent either.

`BP_HordeGoblin` was NOT touched.

## Evaluate

**NOT COMPILED.** The change is three edits and an access-specifier shuffle in the header; the
private members either side of the new public block were checked and are still private.

**Verified by evidence, and this is the part that matters:**

- `GSHordeGoblin.cpp:63` calls `WeaponComponent->EquipWeapon(DefaultWeapon)` in BeginPlay.
- The warning fires from `GSWeaponComponent.cpp:54`, in component BeginPlay, which runs first.
- #144 had already read `EquippedWeapon` off four live summoned pawns in PIE and got
  `DA_Weapon_HordeGoblin` on every one.

So the goblins were armed the whole time and the diagnostic was wrong every time it fired.

**This ticket exists because the warning caused a second misdiagnosis - mine.** I read it in a log,
told Michael the warband was spawning unarmed, called it a regression of #144, and recommended
editing `BP_HordeGoblin` to fix it. #144 had already disproved that exact conclusion and had written
down, in as many words, that changing the asset to match the wrong diagnosis was the worst outcome
available and was one step away. I got there by reading a log line instead of the runtime state -
the same shortcut its author took, and they reported it to Michael twice before catching it.

**Consequence worth stating plainly:** on the strength of that wrong reading I also advised Michael
NOT to run the #214/#215 observation session, on the grounds that the goblins had no abilities. That
advice was wrong and the session was never blocked.

**Unrun:** the suppression itself. After a build, a summoned goblin should produce NO unarmed warning
and no `Invalid Ability Set` error, while a character Blueprint with a genuinely unset weapon should
still warn loudly.

**Not investigated:** whether the ACF `Invalid Character - ActionsManager` and `Invalid Ability Set`
errors are the same timing artifact or something real. They fire on the same frame as the false
warning, which is suggestive but not evidence. **Do not assume they are benign** - that would be the
third time this pattern bit.

## Refine

Nothing changed on review.

**Deliberately not done:** the timer/frame-deferral alternative #144 also offered. An explicit flag
set in a constructor is deterministic; a deferral is a race that happens to win.

> 2026-08-23T23:56Z Warning gated behind an explicit opt-in. Uncompiled.
