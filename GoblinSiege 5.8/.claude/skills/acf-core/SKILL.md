---
name: acf-core
description: Set up AACFCharacter, ACFCharacterDataAsset, teams, damage, and the ACF GameState for the Ascent Combat Framework core module.
globs: []
alwaysApply: false
---

# ACF Core — Ascent Combat Framework

The **ACF Core** module is the foundation of the entire combat framework. It provides `AACFCharacter` (the base class for every combatant — player or AI), `UACFCharacterDataAsset` (data-driven configuration), the team/faction system, damage calculation, and the shared `AACFGameState`. All other modules (Inventory, Actions, Quests, Status Effects) build on top of this one.

---

## 1 — Understand the assets

| Asset | Class | Location (sample) | Purpose |
|---|---|---|---|
| `BP_ACFCharacter` (sample) | `AACFCharacter` | `/AscentCombatFramework/Characters/` | Base character with all ACF components pre-wired |
| `DA_CharacterData` (sample) | `UACFCharacterDataAsset` | `/AscentCombatFramework/Data/` | Stats, starting items, ability set, team, mesh data |
| `ACFGameState` | `AACFGameState` | Engine class / your BP subclass | Holds TeamManager, EffectsDispatcher, DifficultyManager |
| `ACFDeveloperSettings` | `UACFDeveloperSettings` | Project Settings → Ascent GAS Settings | Collision channels, difficulty table assignment |

> **Never edit sample assets.** Duplicate into your own content folder first (e.g. `Content/YourGame/Characters/`), then modify your copy.

### Key components on `AACFCharacter`

| Component | Property name | Purpose |
|---|---|---|
| `UACFAbilitySystemComponent` | `ActionsComp` | GAS — triggers actions, manages abilities |
| `UACFCharacterMovementComponent` | `LocomotionComp` | Walk / jog / sprint / strafe locomotion |
| `UARSStatisticsComponent` | `StatisticsComp` | Attributes (Health, Stamina, Mana, etc.) |
| `UACFEquipmentComponent` | `EquipmentComp` | Inventory + equipment slots |
| `UACFDamageHandlerComponent` | `DamageHandlerComp` | Receives damage, immortality toggle |
| `UACFRagdollComponent` | `RagdollComp` | Physics death / ragdoll |
| `UACFEffectsManagerComponent` | `EffetsComp` | VFX / SFX playback |
| `UACFCharacterInitializerComponent` | `InitializerComp` | Loads a `UACFCharacterDataAsset` at BeginPlay |
| `UACFTeamComponent` | `TeamComponent` | Faction / team tag; used by AI perception |

---

## 2 — Setup / Configuration

1. **Create your GameState subclass.** In the Unreal editor, create a Blueprint subclassing `AACFGameState`. Open **Project Settings → Maps & Modes** and set it as the GameState class for your game mode.

2. **Configure teams.** Open your GameState Blueprint and find the `ACFTeamManagerComponent`. Add team `FGameplayTag` entries (e.g. `Teams.Player`, `Teams.Enemy`, `Teams.Neutral`). Register attitude rules (friendly / hostile / neutral) between teams here.

3. **Assign a Difficulty Table (optional).** In **Project Settings → Ascent GAS Settings**, assign your difficulty `UDataTable`. Duplicate the sample at `/AscentCombatFramework/Configuration/DifficultyConfig` first and save under `Content/YourGame/Configuration/`.

4. **Create your CharacterDataAsset.** Duplicate the sample DataAsset from `/AscentCombatFramework/Data/` and save it under `Content/YourGame/Data/`. Open your copy and configure:
   - `ChatacterName` — display name.
   - `Team` — GameplayTag matching a team registered in GameState.
   - `CharacterRow` — row from your attribute data table (sets base Health, Stamina, etc.).
   - `LevelingType` — `ECantLevelUp` for fixed NPCs; curve-based for leveling characters.
   - `DefaultAbilitySet` — assign a `UACFAbilitySet` (see Actions System skill).
   - `StartingItems` — array of `FStartingItem` with item class, count, `bAutoEquip`, drop chance.
   - `MeshComponents` — skeletal mesh + material overrides per slot.
   - `CharacterEffectsConfig` — VFX/SFX config DataAsset.

5. **Create your Character Blueprint.** Create a new Blueprint subclassing `AACFCharacter` (or the sample player pawn). In **Class Defaults**, set `bAutoInit = true` so `UACFCharacterInitializerComponent` auto-applies the DataAsset on BeginPlay. Assign your `UACFCharacterDataAsset` to the Initializer component.

6. **Configure fall damage (optional).** On the character Blueprint, under **ACF | Fall**:
   - Enable `bEnableFallDamage` and set `FallDamageDistanceThreshold`.
   - Assign a `UCurveFloat` to `FallDamageByFallDistance` mapping distance (cm) → damage.
   - Enable `bTriggerActionOnLand` and set `ActionsToTriggerOnLand` GameplayTag.

---

## 3 — Core Workflow / Usage

### Triggering actions from Blueprints

```
// From any Blueprint that has a reference to AACFCharacter:
Character->TriggerAction(ActionTag, EActionPriority::ELow, false);  // respects priority
Character->ForceAction(ActionTag);                                   // bypasses priority
```

### Switching movesets (weapon equipped / stance)

```
Character->SwitchMoveset(MovesetTag);          // updates locomotion
Character->SwitchMovesetActions(MovesetTag);   // updates ability set
Character->SwitchOverlay(OverlayTag);          // animation overlay
```

### Death / Revive

```
Character->KillCharacter();                        // instant kill (Server RPC)
Character->ReviveCharacter(0.5f);                  // revive with 50% HP
Character->SetIsImmortal(true);                    // prevent all death
```

### Damage activation (weapon swing window)

```
Character->ActivateDamage(EDamageActivationType::EBothWeapons, TraceChannelNames);
Character->DeactivateDamage(EDamageActivationType::EBothWeapons, TraceChannelNames);
```

### Key delegates on `AACFCharacter`

| Delegate | Signature | When it fires |
|---|---|---|
| `OnDeath` | `(AACFCharacter* self)` | Character health reaches zero |
| `OnDamageReceived` | `(FACFDamageEvent)` | Any damage taken |
| `OnDamageInflicted` | `(FACFDamageEvent)` | Any damage dealt |
| `OnCombatTypeChanged` | `(ECombatType)` | Weapon equip state changes |
| `OnCharacterFullyInitialized` | `()` | Initializer component finishes setup |

---

## 4 — Wire to Characters / Blueprints

### Player pawn

1. Open your player character Blueprint (subclass of `AACFCharacter`).
2. Confirm `UACFAbilitySystemComponent`, `UACFEquipmentComponent`, and `UACFCharacterMovementComponent` are present (they are auto-added by the parent class).
3. Assign your `UACFCharacterDataAsset` to the `InitializerComp` slot in the component details.
4. Implement `SetupPlayerInputComponentBP` (BlueprintImplementableEvent) to bind Enhanced Input actions to `TriggerAction` calls.
5. Bind `OnDeath` to show a death screen / respawn logic.

### AI characters

1. Create a Blueprint subclassing `AACFCharacter` (or the sample AI pawn).
2. Assign a `UACFCharacterDataAsset` with `Team = Teams.Enemy`.
3. Add a `UACFTeamComponent` if not inheriting from the sample (it is auto-added in the base class).
4. The `UACFDamageHandlerComponent` handles death automatically; override `OnCharacterDeath_Implementation` in C++ or bind `OnDeath` in Blueprint for loot / despawn.

### BoneNameToDamageZoneMap

Populate `BoneNameToDamageZoneMap` on the character's Class Defaults to map skeleton bones (e.g. `head`, `spine_01`) to `EDamageZone` values (`EHead`, `ETorso`, `ELeg`). The damage system reads this to calculate zone multipliers.

---

## 5 — Verify

**Checklist before testing:**

- [ ] `AACFGameState` subclass is assigned in Project Settings → Maps & Modes.
- [ ] Teams are registered in `UACFTeamManagerComponent` with correct attitude rules.
- [ ] `UACFCharacterDataAsset` is duplicated from sample and saved under `Content/YourGame/`.
- [ ] Character DataAsset `Team` tag matches a registered team in GameState.
- [ ] `CharacterRow` in the DataAsset resolves to a valid row in the Attribute data table.
- [ ] `DefaultAbilitySet` is assigned on the DataAsset.
- [ ] `bAutoInit = true` on the character Blueprint or DataAsset is initializing on BeginPlay.
- [ ] `BoneNameToDamageZoneMap` populated for head/critical zone multipliers.
- [ ] `EDeathType` set correctly (`EDeathAction` plays a death montage; `ERagdoll` enables ragdoll).

**Common failures:**

| Symptom | Fix |
|---|---|
| Characters have no stats / zero health | `CharacterRow` is empty or points to a missing DataTable row |
| Character doesn't die / takes no damage | Team attitude is set to `Friendly` or `DamageHandlerComp` has `bIsImmortal = true` |
| AI not attacking player | Teams are not registered as hostile in `UACFTeamManagerComponent` |
| `OnCharacterFullyInitialized` never fires | `bAutoInit` is false; call `InitializerComp->InitializeCharacter()` manually |
| Movement feels wrong (no strafing) | `RotationMode` in DataAsset is set to `EOrientToMovement` instead of `EStrafing` |
| Crash on BeginPlay | DataAsset is null on `InitializerComp`; assign it in the character Blueprint class defaults |

---

## Goblin Siege addendum (2026-08-27)

**Correction — `bAutoInit` does NOT gate the Data Asset.** Lines 59, 146 and 157 above tie
`UACFCharacterInitializerComponent` to `bAutoInit`; it never reads that flag.
`UACFCharacterInitializerComponent::BeginPlay`'s only condition is `if (CharacterInitDataAsset)`
(`ACFCharacterInitializerComponent.cpp:59-77`), and `InitializeComponent` applies mesh data even earlier
on the same condition (`cpp:40-52`). `bAutoInit` gates only the fallback branch at `ACFCharacter.cpp:198`.
There are three states: DA set (initializer runs, whatever `bAutoInit` says) / no DA + `bAutoInit` true
(the guarded fallback at `:203-218`) / neither. Setting `SetAutoInit(false)` as a bisection step while a
Blueprint still carries a DA changes nothing and produces a false conclusion — clear
`CharacterInitDataAsset` instead.

**The DataAsset field list at §49-58 omits fields that overwrite your Blueprint at BeginPlay.**
Unset ≠ inert: `CapsuleHalfHeight/Radius` default 88/34 and are pushed with no guard
(`ACFCharacterDataAsset.h:93-97`; `ACFCharacterInitializerComponent.cpp:242-255`, from both the server
path `:158` and the client path `:181`); `RotationMode` defaults `EStrafing` and forces
`bOrientRotationToMovement = false` (`ACFCharacterDataAsset.h:108-109`; initializer `:148-151` and
`:195-198`) — so line 158's troubleshooting row points the wrong way, the hazard is the default;
`Currency` defaults `10.f`; `StartingItems` **replaces and empties** the inventory (see the
`inventory-system` addendum). Five of Goblin Siege's six character DAs serialise no capsule at all. This
cost a day in ticket 330.

**`ReviveCharacter` is the whole call** — `ACFCharacter.cpp:815-823` already calls
`DamageHandler->Revive()`, writes health, unlocks actions and restores `MOVE_Walking`. Do not hand-roll
it. But GS's `ApplyRespawnState` (`GSCharacterBase.cpp:922-980`) never touches the damage handler and
`grep -rn "Revive" Source/GoblinSiege` returns zero — reviving in place yields a character alive to GS
and dead to ACF, permanently invulnerable. `gs-teams-damage-spawning` §7.

**Note also (`gs-character-data-asset` §1): FullExample never uses the DataAsset/initializer path at
all** — zero of its 1616 + 1456 uassets reference `CharacterInitDataAsset`. It authors `CharacterRow` on
the statistics component instead. Every bug above is in a path the vendor's own project never runs.
