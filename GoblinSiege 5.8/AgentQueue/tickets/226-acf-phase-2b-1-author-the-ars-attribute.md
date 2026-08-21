---
id: 226
title: ACF Phase 2b-1: author the ARS attribute DataTable for all six archetypes
agent: claude-acf
status: done
claimed: 2026-08-21T02:03Z
build: none
waiting_on:
evaluated: 2026-08-21T02:09:23Z
observed: 2026-08-21T02:20:24Z | The authored ARS data reaches every character: GS.Stats.Dump shows HordeGoblin 40, Militia 30, Archer 20, Player 100 - each matching its DT_GSAttributeInits row - across 17 characters with zero mismatches against our own attribute set.
scenario: L_CombatArena, PIE with a summoned horde and the arena defenders, 2026-08-21 02:19.
files: 
  - Content/Data/DT_GSAttributeInits.uasset
---

## Goal

ACF Phase 2b-1: author the ARS attribute DataTable for all six archetypes

## Generate

The ARS data layer, authored end to end. **The project's first DataTable asset.**

- **`/Game/Data/DT_GSAttributeInits`** - row struct `FACFAttributeInits`, six rows. Health and armour
  transcribed from `DA_Race_Goblin` / `DA_Race_Human`; the five RPG primaries authored fresh per
  Michael's ruling to carry them alongside rather than derive from them.

  | row | HP | Armour | Str | Con | End | Dex | Int |
  |---|---|---|---|---|---|---|---|
  | HordeGoblin | 40 | 0 | 6 | 6 | 8 | 12 | 6 |
  | Brawler | 75 | 6 | 12 | 12 | 10 | 8 | 5 |
  | Militia | 30 | 0 | 8 | 8 | 8 | 8 | 8 |
  | Archer | 20 | 0 | 7 | 6 | 8 | 14 | 9 |
  | Knight | 75 | 6 | 13 | 13 | 12 | 8 | 9 |
  | Player | 100 | 0 | 8 | 10 | 14 | 14 | 10 |

  Stamina/MaxStamina 100 on every row - **provisional**, placed now so 2b-3 does not need a second
  pass. Attributes span three sets: `ACFStatisticsSet` (Health, Stamina), `ACFAttributeSet`
  (PhysicalDefense), `ACFPrimaryAttributeSet` (the five primaries).
- **Five `UACFCharacterDataAsset`s** under `/Game/Data/Characters/`, `LevelingType = ECantLevelUp` so
  `CharacterRow` is used rather than curves.
- **Six Blueprint assignments**, mapped from each BP's own `ArchetypeRowName` rather than guessed:
  CastleGuard01/02 -> Militia, ErikaArcher -> Archer, KnightDPelegrini -> Knight, HordeGoblin ->
  HordeGoblin, GSPlayerCharacter -> Player.
- **`SetAutoInit(true)`** in `AGSCharacterBase`. Verified safe first: the DataAssets carry an empty
  `MeshComponents` array, and `ApplyAppearence` ITERATES that array
  (`ACFCharacterInitializerComponent.cpp:286`), so an empty one applies nothing rather than wiping
  every character's mesh.

## Evaluate

**Verified at the ASSET level only, by read-back after save:** six row names with correct values, five
DataAssets resolving to the right rows and table, six Blueprints resolving to the right DataAsset.

**NOT VERIFIED: the runtime values.** I could not read an ARS statistic from Python -
`get_current_value_for_statitstic` needs an `FGameplayTag` and three attempts to construct
`RPG.Statistic.Health` all produced an empty tag, so the reading `Health 0.0 / -1.0` is a FAILED
LOOKUP and must not be quoted as a health value. Stopped at three per the circuit breaker.

**This is exactly the "build the instrument before the feature" lesson.** There is no runtime readout
for ARS statistics, so the next step in 2b should be a `GS.Stats.Dump` cvar printing each character's
ARS Health/MaxHealth/Stamina beside our `UGSAttributeSetBase` values. Without it the consumer switch
in 2b-1's second half is unwatchable, and "two pools disagree" is precisely the failure ruling 25
exists to catch.

**TWO HEALTH POOLS EXIST RIGHT NOW, KNOWINGLY.** ARS health is initialised from this data; every
consumer still reads `UGSAttributeSetBase`. That is the intermediate state, not the destination - do
not leave it here.

**Two new ACF errors per run, both consequences of enabling auto-init on an unconfigured ACF:**
- `Invalid Ability Set - UACFAbilitySystemComponent::GrantAbilitySet` - the DataAssets have no
  `DefaultAbilitySet`. Expected; ACF abilities are ruling 29's work.
- `Missing Team Config - UACFTeamManagerComponent` (x7) - the GameState's team manager has no team
  config asset. **That is ruling 28**, adopted and not yet built. It was silent before because
  nothing asked; auto-init asks.

## Refine

Primaries are inert - nothing consumes Strength or Dexterity yet. They are authored anyway because
the ruling was to carry the RPG layer's raw material forward, and adding five numbers now is cheaper
than a second pass over six rows later.
