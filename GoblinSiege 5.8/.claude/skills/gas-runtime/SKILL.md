---
name: gas-runtime
description: Configure GAS attributes, leveling, attribute save/serialization, and data-driven difficulty scaling using the ACF AscentGASRuntime module.
globs: []
alwaysApply: false
---

# GAS Runtime — ACF Ultimate

The **AscentGASRuntime** module is ACF's Gameplay Ability System integration. Every combat actor carries a **`UACFGASAttributesComponent`** (extends `UARSLevelingComponent`) that initializes GAS attributes from a DataTable row or level curves, applies permanent/starting GameplayEffects, handles leveling perks, serializes attributes for save/load, and applies **difficulty scaling**. Global GAS configuration (health attribute/tag, max level, serializable attributes, SetByCaller tag tables, and the **Difficulty Scaling Table**) lives in **Project Settings → Ascent GAS Settings** (`UACFGASDeveloperSettings`). Difficulty is a single global tag held by **`UACFDifficultyManagerComponent`** on the GameState, which multiplies opted-in characters' base attributes.

---

## 1 — Understand the assets

| Asset | Class / Row struct | Location (sample) | Purpose |
|---|---|---|---|
| Attribute init table | DataTable of `FACFAttributeInits` | `/AscentCombatFramework/GAS/` | Base attribute values per character row |
| Level curves | `UCurveTable` | `/AscentCombatFramework/GAS/` | Attribute values per level (curve-based leveling) |
| Difficulty Scaling Table | DataTable of `FACFDifficultyScaling` | `/AscentCombatFramework/Configuration/DifficultyConfig` | Per-difficulty attribute multipliers |
| Attributes Config | `UACFAttributesConfigDataAsset` | `/AscentCombatFramework/GAS/` | SetByCaller / attribute config |

> **Never edit sample assets.** Duplicate the `DifficultyConfig` DataTable into `Content/YourGame/Configuration/` and reassign it in **Project Settings → Ascent GAS Settings → ACF|Difficulty**. Sample assets under `/AscentCombatFramework/` are overwritten on every plugin update. See `Docs/ACFDifficultySettings_Wiki.md`.

### Key classes & types

| Symbol | Role |
|---|---|
| `UACFGASAttributesComponent` | Per-actor: init attributes, perks, starting effects, save/load, difficulty refresh |
| `UACFDifficultyManagerComponent` | On GameState; holds `CurrentDifficultyLevel` (Replicated + SaveGame) |
| `UACFGASDeveloperSettings` | Project Settings ("Ascent GAS Settings"): health attribute/tag, `MaxLevel`, serializable attributes, `DifficultyScalingTable` |
| `UACFRPGFunctionLibrary` | Static BP helpers for attributes, gameplay tags, and difficulty |
| `ARSLevelingComponent` | Base leveling component (perks, levels) |
| `FACFDifficultyScaling` | Row: `DifficultyLevel` tag, `AttributeMultipliers`, UI name/description/icon |
| `FACFDifficultyAttributeMultiplier` | `{ Attribute, Multiplier }` pair |
| `FACFAttributeInits` / `FAttributeInit` | Base attribute init rows (`Attribute`, `InitValue`) |
| `FAttributeSerializeNames` | Saved attribute name + value (save/load) |
| `ELevelingType` | `ECantLevelUp`, `EGenerateNewStatsFromCurves`, `EAssignPerksManually` |

---

## 2 — Setup / Configuration

### A. Project Settings (global)

**Edit → Project Settings → Plugins → Ascent GAS Settings:**
- **`SerializableAttributes`** — DataTable (`FAttributeSerializeKeys`) listing which attributes are saved/loaded.
- **`HealthAttribute`** / **`HealthTag`** — the canonical health attribute and its tag.
- **`MaxLevel`** — global level cap (default 100).
- **`AttributesToSetByCallerTags`** / **`StatsToSetByCallerTags`** — SetByCaller tag tables for default GameplayEffects.
- **`DifficultyScalingTable`** (ACF|Difficulty) — **your duplicated** `DifficultyConfig` (row struct `FACFDifficultyScaling`).

### B. Per-character attributes

On the character's `UACFGASAttributesComponent`:
- Choose **`LevelingType`**: `ECantLevelUp`, `EGenerateNewStatsFromCurves`, or `EAssignPerksManually`.
- If perk/DataTable based: set **`CharacterRow`** (row of `FACFAttributeInits`).
- If curve based: set **`AttributesByLevelCurve`** (`UCurveTable`).
- **`StartingEffects`** — permanent GameplayEffects applied on init (passive bonuses).
- **`bAutoInitialize`** — if true, `InitializeAttributeSet()` runs automatically server-side on BeginPlay; otherwise call it manually.
- **`bAffectedByDifficultyLevel`** — true to opt this character into difficulty scaling (usually enemies, not the player).

### C. Difficulty (global, GameState)

1. **Use `AACFGameState`** (carries `UACFDifficultyManagerComponent`).
2. Duplicate `DifficultyConfig`, set it as the **`DifficultyScalingTable`** in Project Settings (step A).
3. Configure rows — each `FACFDifficultyScaling` maps a `DifficultyLevel` tag to `AttributeMultipliers` (e.g. `ACF.Difficulty.Hard` → Health × 1.3, MeleeDamage × 1.5). Built-in tags: `ACF.Difficulty.{VeryEasy,Easy,Normal,Hard,VeryHard}`.
4. Enable **`bAffectedByDifficultyLevel = true`** on each enemy's attributes component / Character Data Asset.

---

## 3 — Core Workflow / Runtime API

### Initialization & leveling

```
// Server-only init
AttribComp->InitializeAttributeSet();

// Leveling (perks)
AttribComp->AssignPerkToAttribute(MyAttribute, /*numPerks*/ 1);   // Server RPC

// Data sources
AttribComp->SetCharacterRow(RowHandle);
AttribComp->SetAttributesByLevelCurve(CurveTable);
```

### Difficulty (via `UACFRPGFunctionLibrary`, category `ACF|Difficulty`)

```
// Read / set (set is server-only)
FGameplayTag Cur = UACFRPGFunctionLibrary::GetCurrentDifficultyLevel(WorldContext);
UACFRPGFunctionLibrary::SetDifficultyLevel(WorldContext, FGameplayTag::RequestGameplayTag("ACF.Difficulty.Hard"));
UACFDifficultyManagerComponent* Mgr = UACFRPGFunctionLibrary::GetDifficultyManager(WorldContext);

// Lookups
FACFDifficultyScaling Row;
UACFRPGFunctionLibrary::TryGetDifficultyScaling(DifficultyTag, Row);
float Mult = UACFRPGFunctionLibrary::GetDifficultyMultiplierForAttribute(DifficultyTag, Attribute);

// UI helpers (populate a difficulty selector)
TArray<FGameplayTag> Tags = UACFRPGFunctionLibrary::GetAllDifficultyTags();
TArray<FText> Names = UACFRPGFunctionLibrary::GetAllDifficultyDisplayNames();
FText Name = UACFRPGFunctionLibrary::GetDifficultyDisplayName(DifficultyTag);
int32 Idx  = UACFRPGFunctionLibrary::DifficultyTagToIndex(DifficultyTag);
FGameplayTag T = UACFRPGFunctionLibrary::DifficultyTagAtIndex(Idx);
```

When difficulty changes, the manager fires **`OnDifficultyChanged`**; every `UACFGASAttributesComponent` with `bAffectedByDifficultyLevel` calls **`RefreshDifficulty()`** and re-multiplies its base attributes.

### Difficulty Manager (server-authoritative)

```
Mgr->SetDifficultyLevel(NewTag);              // server only
FGameplayTag Cur = Mgr->GetCurrentDifficultyLevel();
Mgr->BroadcastCurrentDifficulty();            // after save load — re-applies scaling
```

### Save / load

Attributes flagged in the **`SerializableAttributes`** table are persisted as `FAttributeSerializeNames` (name + value) on the component (SaveGame). On load, init is skipped so saved values are preserved; override `OnComponentLoaded`/`OnComponentSaved` for custom behavior. The difficulty tag is SaveGame on the manager — call `BroadcastCurrentDifficulty()` after load so pawns re-apply scaling.

---

## 4 — Wire to Characters / Blueprints

1. **Characters:** ensure each combat actor has a `UACFGASAttributesComponent` (present on `AACFCharacter`). Assign `CharacterRow` or `AttributesByLevelCurve` matching the chosen `LevelingType`, and any `StartingEffects`.
2. **GameState:** use `AACFGameState` (or add `UACFDifficultyManagerComponent`). Optionally set a default `CurrentDifficultyLevel` for PIE.
3. **Enemies vs player:** set `bAffectedByDifficultyLevel = true` only on actors you want scaled — typically enemies. Leave the player `false`.
4. **Difficulty timing:** set difficulty on new-game start, on options-menu apply (read preference → `SetDifficultyLevel` on server → save settings), and re-broadcast after save load.
5. **Options UI:** difficulty preference lives in `UAUTGameUserSettings` (`GetPreferredDifficultyLevel`/`SetPreferredDifficultyLevel`); it is **not** auto-applied — wire it to `SetDifficultyLevel` yourself. Use the `GetAllDifficultyTags`/`GetAllDifficultyDisplayNames` helpers to build the selector.

---

## 5 — Verify

**Checklist before testing:**

- [ ] `Ascent GAS Settings` configured: `HealthAttribute`/`HealthTag`, `MaxLevel`, `SerializableAttributes`.
- [ ] `DifficultyScalingTable` points to **your duplicated** `DifficultyConfig` (not the sample).
- [ ] Each character's `UACFGASAttributesComponent` has a valid `CharacterRow` or `AttributesByLevelCurve` matching its `LevelingType`.
- [ ] `InitializeAttributeSet()` runs (either `bAutoInitialize` true or called manually, server-side).
- [ ] Enemies have `bAffectedByDifficultyLevel = true`; player typically false.
- [ ] GameState is `AACFGameState` (or has `UACFDifficultyManagerComponent`).
- [ ] Difficulty rows use real GAS attributes and tags under `ACF.Difficulty`.
- [ ] After save load, `BroadcastCurrentDifficulty()` is called.

**Common failures:**

| Symptom | Fix |
|---|---|
| Attributes all default / zero | `InitializeAttributeSet()` not called, or `CharacterRow`/curve unset for the `LevelingType` |
| Difficulty has no effect | Character missing `bAffectedByDifficultyLevel = true` |
| Wrong multipliers | Editing the sample DataTable, or Project Settings still points to the sample — use your duplicate |
| Difficulty tag not found | Row's `DifficultyLevel` doesn't match the tag passed to `SetDifficultyLevel` |
| One attribute not scaling | That attribute isn't listed in the active row's `AttributeMultipliers` (defaults to 1.0) |
| Stats wrong after load | `BroadcastCurrentDifficulty()` not called after deserialization |
| Difficulty change ignored on clients | `SetDifficultyLevel` must run on the server (authority) |
| Saved stats not restored | Attribute not listed in the `SerializableAttributes` table |
| No Difficulty Manager | Use `AACFGameState` or add `UACFDifficultyManagerComponent` to your GameState |

---

## Goblin Siege addendum (2026-08-27)

**`StartingEffects` is not an optional "passive bonuses" field — it is where derived statistics come
from.** `InitAttributesFromDT` writes only the attributes literally listed in the row
(`ACFGASStatisticsComponent.cpp:100-127`); `ApplyPermanentEffects` applies `StartingEffects` as infinite
GEs (`:173-187`). ACF's own sample derives everything that way: `ACF_SampleAttributesInit_DT` contains
**only** the five primaries (`ACFPrimaryAttributeSet.Strength/Dexterity/Constitution/Intelligence/Endurance`),
and `ACFCharacterBP` carries three shipped GEs under
`.../AscentCombatFramework/Content/GASRuntime/GameplayEffects/`:

| GE | What it does |
|---|---|
| `ACF_AttDependencies_GE` | maps Constitution / Endurance as `BackingAttribute` onto `ACFStatisticsSet.MaxHealth`, `.MaxStamina`, `.MaxMana`, `.MaxEquilibrium`, `.StaminaRegen`, `.ManaRegen`, plus `ACFAttributeSet.MeleeDamage/.RangedDamage/.SpellDamage/.CritChance/.PhysicalDefense` |
| `ACF_StatInit_GE` | fills Health / Stamina / Mana / Equilibrium from their Max counterparts |
| `ACF_Regen_GE` | health / mana regen — without it there is none |

**`StartingEffects` lives on the component and is `EditAnywhere` (`ACFGASStatisticsComponent.h:145`); the
character initializer does NOT set it** (it writes only `LevelingType`, `CharacterRow`,
`AttributesByLevelCurve`, `bAffectedByDifficultyLevel` and level —
`ACFCharacterInitializerComponent.cpp:134-143`). Attribute setup is therefore split half DataAsset, half
Blueprint component.

Goblin Siege writes Health/MaxHealth/Stamina/MaxStamina/PhysicalDefense as literal rows in
`DT_GSAttributeInits` and applies **none** of those GEs, so its five primaries drive nothing and there is
no regen — silently, since the values read back correctly. (`rpg-system`'s
`UARSGenerationRulesDataAsset` curves are a *different* derivation mechanism; do not build that instead.)
`gs-character-data-asset` §6.
