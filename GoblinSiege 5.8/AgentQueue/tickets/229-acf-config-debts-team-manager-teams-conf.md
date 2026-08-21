---
id: 229
title: ACF config debts: team manager + teams config, collisions master, ability set
agent: claude-acf
status: done
claimed: 2026-08-21T02:55Z
build: none
waiting_on:
evaluated: 2026-08-21T03:01:33Z
observed: 2026-08-21T03:22:14Z | ACF teams work end to end. GS.Teams.Check on a live arena with 17 characters reports 2 teams and the full matrix: Human->Human not hostile, Human->Goblin YES, Goblin->Human YES, Goblin->Goblin not hostile. Both of ruling 28 traps are avoided - same-team friendliness is authored (ACF default is Neutral) and hostility holds in both directions (ACF mirrors nothing). IsHostileTo now asks UACFTeamComponent with a RaceTag fallback only for actors with no team component.
scenario: L_CombatArena, PIE with a summoned horde and the arena defenders, GS.Teams.Check, 2026-08-21 03:21.
files: 
  - Source/GoblinSiege/Core/GSGameState.h
  - Source/GoblinSiege/Core/GSGameState.cpp
  - Source/GoblinSiege/Core/GSGameMode.cpp
  - Source/GoblinSiege/Core/GSGameMode.h
  - Content/Data/DA_GSTeams.uasset
---

## Goal

ACF config debts: team manager + teams config, collisions master, ability set

## Generate

The three ACF config debts that had been logging errors on every run since Phase 2a.

**1. Team manager (ruling 28).**
- Native `Teams.Goblin` / `Teams.Human` tags in `GSGameplayTags`.
- `UACFTeamManagerComponent` added to `AGSGameState`. **As a component, not by reparenting onto
  `AACFGameState`** - the team subsystem only does
  `GameState->FindComponentByClass<UACFTeamManagerComponent>()` (`ACFTeamManagerSubsystem.cpp:31`),
  and `AACFGameState` derives from `AGameState` rather than `AGameStateBase`, which would have
  dragged in match-state machinery this project does not use.
- Every character sets its ACF team from its `RaceTag` in `PostInitializeComponents` - a mapping, not
  a second source, so changing a character's race moves its team with it.
- `DA_GSTeams` created (`UACFTeamsConfigDataAsset`), **empty**.

**2. Collisions master.** `UACMCollisionsMasterComponent` on `AGSGameMode`, which is where each
character's collision manager looks for it. Present to satisfy the lookup and to be ready for 2b-2;
nothing reads it yet, since our melee still runs its own sweep.

**3. Ability set.** NOT done - see below.

`AscentTeams` and `CollisionsManager` added to `Build.cs`.

## Evaluate

**BUILT, NOT WATCHED.**

**BLOCKED, and it needs Michael at the editor:** `DA_GSTeams` is empty and cannot be filled from
here. `FTeamConfig` is keyed on `FGameplayTag`, and **editor Python cannot construct or read a
gameplay tag in this build** - `set_editor_property` refuses a string ("Failed to convert type 'str'
to property 'TeamTag'"), `import_text` silently yields an empty struct in all three syntaxes tried,
and `GameplayTagLibrary` exposes no request-from-string function. Three approaches, then stopped.

**A CORRECTION THIS FORCED, recorded because it was stated as fact earlier:** #228's addendum claims
`HealthTag` was "still empty" after the config fix. **That claim is unfounded.** Python renders EVERY
`FGameplayTag` as `{}` regardless of content - `RaceTag` on a live character CDO reads `{}` too, and
that one is certainly set. Whether `HealthTag` resolved was never actually measured, and the reading
that suggested otherwise was an artefact of the tool.

**Until `DA_GSTeams` is filled, ACF teams do nothing** and every attitude query still falls through to
`GetDefaultAttitude()`, which is Neutral. `AGSCharacterBase::IsHostileTo` deliberately still uses
RaceTag - rewiring it onto `AreTeamsHostile` before the config exists would make every character
neutral to every other and stop the game dead.

## Refine

Left `IsHostileTo` alone on purpose. Ruling 28's wrapper is a two-line change and it is the LAST step,
not the first - it must not land until the config is authored and watched, or the failure mode is a
world where nothing is anyone's enemy.

## Addendum - unblocked, and ruling 28 is complete

Michael authored `DA_GSTeams`. Two things were then still missing and both are done:

1. **The asset was not ASSIGNED.** `TeamConfigDataAsset` read `None` on the manager - the config
   existed and nothing pointed at it, which is indistinguishable at runtime from having no config at
   all. Wired in `AGSGameState`'s constructor via `ConstructorHelpers::FObjectFinder`, with a loud
   `UE_LOG(Error)` on failure so a renamed asset says so at startup instead of the world quietly
   turning neutral.
2. **`IsHostileTo` now asks ACF** (`UACFTeamComponent::IsHostileTowards`), with a RaceTag fallback
   ONLY for actors ACF cannot answer for - a target dummy or breakable with no team component. A
   character whose team is valid gets ACF's answer even when that answer is "not hostile", because
   second-guessing it would hide a misconfigured `DA_GSTeams` instead of exposing it.

**`GS.Teams.Check` added** because editor Python cannot construct or read an `FGameplayTag` in this
build - asking the running game and printing the answer is the only readout available.

**WATCHED**, 17 characters, 2 teams:

```
Teams.Human  -> Teams.Human   hostile: no
Teams.Human  -> Teams.Goblin  hostile: YES
Teams.Goblin -> Teams.Human   hostile: YES
Teams.Goblin -> Teams.Goblin  hostile: no
```

Both of ruling 28's recorded traps are demonstrably avoided: same-team reads friendly (the default is
Neutral, so this had to be authored) and hostility holds in BOTH directions (ACF's lookup mirrors
nothing).

**Still open from this ticket:** the ability set (`Invalid Ability Set` on every run). It is ruling
29's work - the character DataAssets have no `DefaultAbilitySet` - and was not attempted here.
