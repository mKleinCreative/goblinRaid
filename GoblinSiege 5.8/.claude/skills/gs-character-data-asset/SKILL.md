---
name: gs-character-data-asset
description: Goblin Siege — the ACF Character Data Asset path nobody else runs: unset fields that overwrite the capsule, rotation mode and inventory at BeginPlay, and the switch that actually disables it.
globs: []
alwaysApply: false
---

# GS — the Character Data Asset overwrites what you did not author

Goblin Siege is running `UACFCharacterInitializerComponent` on every character in the game
(`GSCharacterBase.cpp:118` `SetAutoInit(true)`, #226; seven Blueprints carry a
`CharacterInitDataAsset`). **ACF's own sample project never runs this path** (§1), so its unset fields
have never been exercised by anyone else, and they are not inert — they push CDO defaults over your
Blueprint values at BeginPlay, on the server *and* the client.

The signature failure is: **the editor value is right and the runtime value is not.** You cannot
diagnose any of this by reading the Blueprint. `acf-core` lists the DataAsset fields an author should
configure and omits the capsule and rotation mode entirely, which reinforces the belief that an unset
field does nothing.

---

## 1 — "ACF ships it" is not "ACF exercises it"

`UACFCharacterInitializerComponent` is created on every `AACFCharacter` (`ACFCharacter.cpp:71`) and
`bAutoInit` defaults true (`ACFCharacter.h:137`). But zero uassets under
`D:/UE_5.8/Unreal Projects/FullExample/Content` (1616 files) or that project's bundled
`Plugins/Marketplace/AscentCombatFramework/Content` (1456 files) reference `CharacterInitDataAsset`.
The sample instead authors `FDataTableRowHandle CharacterRow` directly on the statistics component in
`Content/Blueprints/ACFCharacterBP.uasset` and `Blueprints/AI/AIs/ACFAICharacterBP.uasset`, both
pointing at `Content/Configuration/ACF_SampleAttributesInit_DT.uasset`. The bundled plugin source is
byte-identical to the engine copy (`diff -q` on `ACFCharacterInitializerComponent.cpp` returns 0), so
this is not a version gap — the vendor simply does not use the feature.

Every bug in §2–§5 is a bug in that unexercised path. Hence the second clause on the standing rule:
**adopt what ACF ships AND what the sample demonstrates.**

The cheap alternative is on the table: put `CharacterRow` on the `UACFGASStatisticsComponent` in the
Blueprint and leave `CharacterInitDataAsset` **empty**. That restores
`AACFCharacter::InitializeCharacter`'s guarded fallback (`ACFCharacter.cpp:198-219`) and stops the
capsule, team, rotation-mode and equipment overwrites in one move.

---

## 2 — The capsule is silently resized to 88 / 34

`UACFCharacterDataAsset` declares `CapsuleHalfHeight = 88.0f` and `CapsuleRadius = 34.0f` as CDO
defaults (`Public/Data/ACFCharacterDataAsset.h:93-97`). `ApplyCapsuleSize` calls
`Capsule->SetCapsuleSize(CharacterDataAsset->CapsuleRadius, CharacterDataAsset->CapsuleHalfHeight)`
with **no guard for "unset"** (`Private/Data/ACFCharacterInitializerComponent.cpp:242-255`), from both
`InternalHandleServerInit:158` and `InternalHandleClientInit:181`. Nothing adjusts the mesh's relative
Z to match.

`grep -a -o 'Capsule[A-Za-z]+'` over `Content/Data/Characters/*.uasset` yields
`CapsuleHalfHeight CapsuleRadius` for **`DA_Char_Militia` only**. Archer, Civilian, HordeGoblin, Knight
and Player serialise nothing, i.e. they equal the CDO and push 88/34.

This has already cost a day. `AgentQueue/tickets/330-defenders-ai-content-moves-onto-acf-acf.md:105-118`
records `DA_Char_Militia` carrying 88/34 while `BP_CastleGuard01` authored 177.5/68.6 — capsule bottom
89.5uu above the mesh's feet, and Michael reporting "they're still sticking partly through the ground".
The same ticket notes an earlier observation ("the navmesh is about the height above their knees") that
was explained away rather than believed. The fix landed for Militia only; the other five DAs were last
written 2026-08-23/24, before the cause was known (mtimes: `DA_Char_Militia` 2026-08-27 15:07, others
08-23/08-24). Erika was measured at a 300uu capsule and the goblins at 240uu
(`AGENT_STATE.md:896-898`) — all now reset to 176uu tall / 68uu wide at BeginPlay.

**Look for:** a character rendering knee-deep; arrows or traces passing through a body that used to
block them.

---

## 3 — `RotationMode` defaults to `EStrafing` and rewrites the movement flags

`UACFCharacterDataAsset::RotationMode` defaults to `ERotationMode::EStrafing`
(`ACFCharacterDataAsset.h:108-109`). The initializer pushes it on both paths —
`ACFCharacterInitializerComponent.cpp:148-151` (server) and `:195-198` (client) — and
`UACFCharacterMovementComponent::SetRotationMode` → `Internal_SetStrafe` sets
`bOrientRotationToMovement = !(RotationMode == EStrafing)` and
`bUseControllerDesiredRotation = (RotationMode == EStrafing)`
(`ACFCharacterMovementComponent.cpp:432-443`).

**None of the six GS DAs serialise `RotationMode`** (`grep -a -o 'RotationMode'` over
`Content/Data/Characters/DA_Char_*.uasset` returns nothing), so every one applies `EStrafing` and
forces `bOrientRotationToMovement = false`. A character that should turn to face where it is walking
will not, and the Blueprint checkbox still shows ticked.

`acf-core:158`'s troubleshooting row points the other way — it assumes you *wanted* strafing and that
the field was deliberately authored. The hazard is the **default**.

Second-order (stated as reasoning, not an observed symptom — confidence partial):
`UGSAISteeringComponent` lazily captures `bOrientRotationToMovement` as "the default"
(`GSAISteeringComponent.cpp:129-136`) and restores that value at `:250`; its comment at `:231` asserts
"It is already false on both defender CDOs", which is true at runtime but is a consequence of the
initializer, not of the CDO. Give any DA a non-strafing `RotationMode` and that captured baseline
changes underneath the steering component with no code change.

Contrast `Team`, which the same initializer also overwrites with an empty tag (`cpp:145-146`; no GS DA
serialises `Team` either). That one is harmless *only* because `AGSCharacterBase::BeginPlay` re-sets it
after `Super::` (`GSCharacterBase.cpp:269-272`). Do not rely on that accident twice.

---

## 4 — `StartingItems` on the DA replaces the inventory; blank means "wipe it"

`ApplyEquipData` calls `SetCurrency(CharacterDataAsset->Currency)` then
`SetStartingItems(CharacterDataAsset->StartingItems)` then `RefreshEquipment()`
(`ACFCharacterInitializerComponent.cpp:228-240`). `UACFEquipmentComponent::SetStartingItems` assigns
and immediately calls `InitializeStartingItems()` (`ACFEquipmentComponent.cpp:1063-1067`), and that
does **`GetInventoryList().Empty()`** before re-adding from the DA array (same file, `172-186`).

Note the guard asymmetry: the non-DA path in `AACFCharacter::InitializeCharacter` is protected by
`if (EquipmentComp->GetIsInitialized() == false)` (`ACFCharacter.cpp:213`). **The DA path has no such
guard.** So `inventory-system:207`'s "(or directly on the component)" is the wrong model — the two
locations are not interchangeable; the DA wins and erases.

`DA_Char_Civilian.uasset` is the only GS character DA whose name table has **no** `StartingItems` entry
(`grep -a -o 'StartingItems'` over `Content/Data/Characters/*.uasset` — Archer, HordeGoblin, Knight,
Militia and Player all have it), so `BP_PeasantMan` initialises with an empty array.

Symptom: a character spawns bare-handed with no weapon abilities — the same symptom
`GSWeaponComponent.cpp:379-388` warns about for a null weapon data asset, so it gets misread as a socket
or mesh problem. `AGENT_STATE.md:889-890` records the previous instance of characters silently fighting
bare-handed for weeks. `UACFCharacterDataAsset::Currency` also defaults to `10.f`
(`ACFCharacterDataAsset.h:81-82`), so an unset DA hands every character 10 currency — the same shape of
accident, quieter.

Related, already worked around: `UGSWeaponComponent::BeginPlay` uses a next-tick timer to re-sync the
drawn slot "because `UACFCharacterInitializerComponent` equips the starting items in its own BeginPlay
and component BeginPlay order is not guaranteed" (`GSWeaponComponent.cpp:359-375`).

---

## 5 — `SetAutoInit(false)` does not disable the Data Asset

There are **three** states here, not two:

| `CharacterInitDataAsset` | `bAutoInit` | What runs |
|---|---|---|
| set | either | the initializer, in full — `bAutoInit` is never read on this path |
| empty | true | `AACFCharacter::InitializeCharacter`'s fallback (`ACFCharacter.cpp:203-218`): grants initial abilities, initialises the attribute set from component values |
| empty | false | neither |

`UACFCharacterInitializerComponent::BeginPlay` never reads `bAutoInit`; its only condition is
`if (CharacterInitDataAsset)` (`ACFCharacterInitializerComponent.cpp:59-77`). Earlier still,
`InitializeComponent` applies mesh data before BeginPlay on the same condition (`cpp:40-52`). In the
editor, `PostEditChangeProperty` applies appearance and calls `Owner->SetActorLabel(charData->ChatacterName)`
the moment the field is assigned (`cpp:363-381`). `bAutoInit` gates only the branch at
`ACFCharacter.cpp:198` (`HasAuthority() && !bInitialized && bAutoInit`).

**Why this matters:** flipping `SetAutoInit(false)` as a bisection step — "turn ACF's init off and see
if the capsule is still wrong" — changes **nothing** while the Blueprints still carry a Data Asset, and
the conclusion drawn ("so it isn't ACF") is false. The switch that stops it is clearing
`CharacterInitDataAsset` on the Blueprint. `acf-core` states the wrong mechanism in three places
(`:59`, `:146`, `:157`) — see its Goblin Siege addendum. `GSCharacterBase.cpp:100-118` carries the same
wrong reasoning from Phase 2a; it happened to hold only because no Blueprint had a DA assigned yet.

---

## 6 — The five primary attributes are inert; ACF's answer is three shipped GEs

`UACFGASStatisticsComponent::InitializeAttributeSet` is `InitAttributesValue()` → `ApplyPermanentEffects()`
→ `BindHealthDelegate()` (`ACFGASStatisticsComponent.cpp:52-58`). `InitAttributesFromDT` writes only the
attributes literally listed in the row (`:100-127`); `ApplyPermanentEffects` applies the component's
`StartingEffects` array as infinite GEs (`:173-187`).

Critically, **`StartingEffects` lives on the component and is `EditAnywhere`**
(`ACFGASStatisticsComponent.h:145`) — the initializer does **not** set it. It overwrites only
`LevelingType`, `CharacterRow`, `AttributesByLevelCurve`, `bAffectedByDifficultyLevel` and the level
(`ACFCharacterInitializerComponent.cpp:134-143`). Attribute setup is therefore split: half Data Asset,
half Blueprint component.

| | FullExample | Goblin Siege |
|---|---|---|
| Attribute init table | `ACF_SampleAttributesInit_DT` — **only** the five primaries (`ACFPrimaryAttributeSet.Strength/Dexterity/Constitution/Intelligence/Endurance`); no Health, no MaxHealth, no Stamina | `Content/Data/DT_GSAttributeInits.uasset` — `ACFStatisticsSet.Health/.MaxHealth/.Stamina/.MaxStamina` and `ACFAttributeSet.PhysicalDefense` as raw InitValue rows **alongside** the primaries |
| Derivation | `StartingEffects` on `ACFCharacterBP`: `ACF_AttDependencies_GE`, `ACF_StatInit_GE`, `ACF_Regen_GE` (plugin `Content/GASRuntime/GameplayEffects/`) | none — grep over `BP_GSPlayerCharacter`, `BP_HordeGoblin`, `Adversaries/BP_CastleGuard01` for `StartingEffects` / those GE names returns only `StatModifierGE` |

`ACF_AttDependencies_GE` maps `Constitution` and `Endurance` as `BackingAttribute` onto
`ACFStatisticsSet.MaxHealth`, `.MaxStamina`, `.MaxMana`, `.MaxEquilibrium`, `.StaminaRegen`, `.ManaRegen`
plus `ACFAttributeSet.MeleeDamage/.RangedDamage/.SpellDamage/.CritChance/.PhysicalDefense`;
`ACF_StatInit_GE` fills Health/Stamina/Mana/Equilibrium from their Max counterparts.

So in GS the five primaries **drive nothing**: raising a knight's Constitution changes no health, and
any future buff, perk (`AssignPerkToPrimaryAttribute`, `ACFGASStatisticsComponent.cpp:26-43`),
difficulty multiplier or attribute-set modifier that moves a primary produces zero observable change.
There is also **no health or mana regen anywhere**, because `ACF_Regen_GE` is the mechanism and it is
not applied. Nothing logs — the values read back correctly. Same shape as the bug at
`GSCharacterBase.cpp:198-208`, where the whole DT was read and discarded because the attribute sets
were unregistered, and only `GS.Stats.Dump` printing "ARS (absent)" revealed it. Stamina is currently a
hand-written non-GAS `UGSStaminaComponent` (`GSStaminaComponent.h:7-9`).

If primaries are meant to matter, the ACF answer is `StartingEffects` on the statistics component
authored as ACF's own three GEs — not another GS-side derivation. (`rpg-system`'s
`UARSGenerationRulesDataAsset` curves are a *different* mechanism; do not build that instead.)
