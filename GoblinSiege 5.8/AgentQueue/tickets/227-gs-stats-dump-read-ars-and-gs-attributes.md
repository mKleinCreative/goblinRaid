---
id: 227
title: GS.Stats.Dump: read ARS and GS attributes side by side
agent: claude-acf
status: done
claimed: 2026-08-21T02:11Z
build: none
waiting_on:
evaluated: 2026-08-21T02:18:54Z
observed: 2026-08-21T02:20:22Z | GS.Stats.Dump works and proved the ARS data layer end to end: 17 characters, 0 MaxHealth mismatches, every archetype reading its own DT_GSAttributeInits row - HordeGoblin 40/40, Militia 30/30, Archer 20/20, Player 100/100, all matching our UGSAttributeSetBase values exactly. On its FIRST run before the fix the same command printed ARS (absent) 0/0, exposing that ACF never registers its attribute sets and the whole table was being silently discarded.
scenario: L_CombatArena opened explicitly (the earlier PIE was on an empty default map), PIE, GS.Horde.SpawnTest, then GS.Stats.Dump. 2026-08-21 02:19.
files: 
  - Source/GoblinSiege/Combat/GSDebugCommands.cpp
  - Source/GoblinSiege/GoblinSiege.Build.cs
---

## Goal

GS.Stats.Dump: read ARS and GS attributes side by side

## Generate

**`GS.Stats.Dump`** - per character, ACF/ARS Health, Stamina and PhysicalDefense printed beside our
`UGSAttributeSetBase` Health/MaxHealth/Armor, with a `<-- MaxHealth MISMATCH` marker when the two
pools disagree. `AscentGASRuntime` added to `Build.cs` (calling into it, not merely deriving).

Two details that matter:
- Reads through the attribute-set accessors, NOT ACF's tag-keyed getters, so no `FGameplayTag` has to
  be constructed and a typo is a compile error rather than a silent zero. **ACF's
  `ATTRIBUTE_ACCESSORS` generates `HealthAttribute()` as a NON-STATIC const member**, unlike Epic's
  static `GetHealthAttribute()`, so ACF reads go through `GetDefault<>()`. Both conventions appear
  side by side in the same function on purpose.
- `bFound` is checked on every read. `GetGameplayAttributeValue` returns 0 for an attribute the ASC
  has never heard of, which is indistinguishable from a real zero; an unregistered set prints
  `(absent)` instead of pretending to be a character with no health.

**And it immediately found the bug it was built to find.** First run: `ARS (absent) 0/0 hp`.

`UACFGASStatisticsComponent::InitAttributesFromDT` skips every attribute whose set is not registered:

```cpp
if (!abilityComp->HasAttributeSetForAttribute(attribute.Attribute)) { continue; }   // :120
```

**ACF never creates its attribute sets in C++.** It relies on GAS's `DefaultStartingData`, an
`EditAnywhere` array on `UAbilitySystemComponent` that nobody had configured. So `UACFStatisticsSet`,
`UACFAttributeSet` and `UACFPrimaryAttributeSet` did not exist on any character, and the entire
`DT_GSAttributeInits` table authored in #226 was being read and **discarded row by row, silently** -
no log, no warning, no failure.

Fixed in `AGSCharacterBase::PostInitializeComponents` rather than as data on six Blueprints, so a new
character cannot be authored without it.

## Evaluate

**WATCHED.** After the fix, `GS.Stats.Dump` in PIE reads:

```
BP_GSPlayerCharacter_C_0   ARS 100/100 hp  100/100 stam  def 0  |  GS 100/100 hp  armor 0
GS.Stats.Dump: 1 character(s), 0 MaxHealth mismatch(es).
```

matching the Player row exactly, where before the fix the same command printed `ARS (absent) 0/0`.

**The weakness in that evidence, stated rather than glossed:** the Player row is 100 HP and our C++
default is ALSO 100, so this reading does not prove the table is being read - only that the sets now
exist and hold something plausible. **A goblin reading 40 or a militiaman reading 30 is the
discriminating test**, and it has not been done: `GS.Horde.SpawnTest` summoned 0 because the loaded
level has no `Marker.HordeArrival` markers, and no defenders were present either. Unrelated to 2b.

**Owed:** run `GS.Stats.Dump` during a fight on `L_CombatArena` with goblins and guards alive. Expect
HordeGoblin 40, Militia 30, Archer 20, Knight 75. Anything else means the table is not reaching them.

## Refine

This ticket exists because #226 could not be verified, and it paid for itself on its first run by
exposing a silent data-discard that no log would ever have shown. Worth remembering the next time an
instrument looks like a detour.
