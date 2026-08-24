---
id: 270
title: ACF Phase 3 stage 2: equip the goblin axe through ACF on one character, alongside the existing path
agent: claude-warren
status: done
claimed: 2026-08-24T00:24Z
build: none
waiting_on: Michael: adding ItemSlot tags means editing GSGameplayTags (shared C++) and taking a build. Confirm before proceeding.
evaluated: 2026-08-24T02:09:06Z
observed: 2026-08-24T01:32:20Z | 10 of 10 horde goblins summoned by GS.Horde.SpawnTest came out of spawn with the axe equipped through ACF - is_equipped true, BP_ACFWeapon_GoblinAxe actor spawned and attached at the back_sword socket. Before the fix the same summon produced an empty inventory and no equip, and granting the weapon type on a live pawn flipped it.
scenario: Fresh PIE on L_CombatArena, horde summoned via GS.Horde.SpawnTest 4, live pawns read directly - not the player pawn and not a duel.
files: 
  - Content/Blueprints/BP_HordeGoblin.uasset
  - Content/Items/DA_ACFWeapon_GoblinAxe.uasset
  - Content/Data/Characters/DA_Char_HordeGoblin.uasset
  - Content/Items/BP_Item_GoblinAxe.uasset
  - Config/DefaultGameplayTags.ini
---

## Goal

ACF Phase 3 stage 2: equip the goblin axe through ACF on one character, alongside the existing path

## Generate

**Discovery only so far - nothing authored yet.** Two findings changed the shape of the job.

### What a UACFWeapon actually needs

Read off the live CDOs, since `/Game/FullSample/` is still absent and there is no reference asset to
copy anywhere in the project (zero `UACFWeapon`, zero `UACFArmor`).

`UACFItem` exposes almost nothing directly - everything lives in an `item_info` struct of type
`FItemDescriptor`, which carries `item_slots` (the GameplayTags the item fits), `item_type`, `name`,
`item_weight`, `max_inventory_stack` and `world_mesh`. `UACFWeapon` adds `hands_socket_name`,
`on_body_socket_name`, `item_actor_class`, `handle_type` and three moveset tags.

### Blocker 1: the ItemSlot tags do not exist, and adding them is a C++ change

The skill assumes `ItemSlot.RightHand` and friends. **They do not exist in this install.** ACF's own
config declares only `ItemSlot.ItemSlot`, and this project has **no gameplay tag ini at all** - all
45 of its tags are declared in C++ in `GSGameplayTags.{h,cpp}` via `UE_DECLARE_GAMEPLAY_TAG_EXTERN`.

So authoring a slot tag means editing a widely-shared C++ file and taking a build, not ticking a box.

*(Recorded because I nearly got this wrong: an earlier grep appeared to show `ItemSlot.ItemSlot` in
OUR config. It was ACF's - I ran two greps and read the combined output as one.)*

### Blocker 2: the weapon actor wants a skeletal mesh, our axe is static

`AACFWeaponActor`'s default components are SceneComponent, **SkeletalMeshComponent**,
`ACMCollisionManagerComponent`, SceneComponent. The goblin axe is `SM_Axe_DA`, a STATIC mesh, and so
is every weapon this project owns. Normally the weapon actor is a Blueprint from FullSample; here it
has to be authored, and it has to carry a static mesh component ACF does not put there by default.

Whether ACF's collision manager and moveset switching care about that is unknown and is exactly the
kind of thing Stage 2 exists to find out - but it means the first weapon actor is a custom Blueprint,
not a duplicate of a working sample.

## Evaluate

Nothing authored, nothing compiled, nothing run.

**Established by direct inspection**, not inference: the property shape of `UACFItem` / `FItemDescriptor`
/ `UACFWeapon`, the default component set of `AACFWeaponActor`, the absence of slot tags, and this
project's C++-only tag convention.

**Not established:** whether ACF's equipment path works at all in this install once given a slot and
an item. That is still the whole question, and it is now one C++ tag declaration, one Blueprint and
one DataAsset away rather than one DataAsset away.

## Refine

Stopped at the decision point rather than pushing through it. Declaring gameplay tags touches
`GSGameplayTags.{h,cpp}` - a file every system in the project includes - and costs a build. That is
not a change to make silently inside a ticket scoped to "one weapon on one character".


### Stage 2 authored and tested, 2026-08-24

Built: `BP_ACFWeapon_GoblinAxe` (an `AACFWeaponActor` subclass with a STATIC `AxeMesh` carrying
`SM_WoodcutterAxe`, since ACF ships a skeletal one), `BP_Item_GoblinAxe` (a `UACFWeapon` **Blueprint
subclass**), and `BP_HordeGoblin` configured with slot `ItemSlot.RightHand` + the axe as a starting
item. Nothing existing was changed - `UGSWeaponComponent` still equips the axe as before.

**Three places the skill does not match ACF 4.4.2:**

1. `UACFWeapon` is **not** a `UDataAsset` - it is a plain `UObject`, so "right-click - Data Asset"
   does not apply and `DataAssetFactory` refuses it.
2. Items are Blueprint **subclasses**, not asset instances. `FStartingItem.ItemClass` is a
   `TSubclassOf`, so a `UACFWeapon` instance can never be referenced by it. The first attempt made
   an instance (`DA_ACFWeapon_GoblinAxe`) which read back as the base class - it is still on disk,
   is the wrong shape, and should be deleted deliberately.
3. `AACFWeaponActor` ships a skeletal mesh; every weapon this project owns is static.

**RESULT: one axe, not two. ACF is not equipping.** Runtime read off a live summoned goblin:

| | |
|---|---|
| `available_equipment_slot` | `['ItemSlot.RightHand']` - **reached the pawn** |
| `starting_items` | **0** - did NOT reach the pawn |
| inventory | 0 |

**The save-gate hypothesis is disproved.** `InitializeStartingItems` sits behind an ALS
save/new-game check, and that looked like the cause from reading source - but the runtime read shows
the component simply has no starting items to initialise. Something between the Blueprint template
and the spawned pawn drops that array while keeping the slots.

**Suspect, NOT a finding:** `AGSHordeGoblin::InitializeFromArchetype()` runs in BeginPlay and may
overwrite equipment config. Untested. Recording it as a lead because this session has twice produced
a confident wrong diagnosis from static reading.

**Next probe:** read `starting_items` immediately before and after `InitializeFromArchetype` on a
live pawn. That distinguishes "never serialised" from "set then cleared" in one step.

> 2026-08-24T01:11Z Stage 2 authored and tested. ACF does not equip: slots reach the pawn, starting items do not.


### Root cause found and fixed, 2026-08-24 (second pass)

The axe was authored, in the inventory, in a valid slot - and never equipped. **Two independent
faults, stacked**, each of which fails in complete silence.

**Fault 1 - the starting item was written to the wrong owner.** ACF's
`UACFCharacterInitializerComponent::ApplyEquipData` calls
`InventoryComp->SetStartingItems(CharacterDataAsset->StartingItems)`, which **overwrites** whatever
the equipment component's own `StartingItems` array held. `BP_HordeGoblin` carries
`DA_Char_HordeGoblin`, whose `StartingItems` was empty, so my entry on the component was replaced by
an empty list at init. This is why the probe read `SCS template starting_items=1, live pawn
starting_items=0` while `AvailableEquipmentSlot` survived: slots are a component property nothing
overwrites; starting items are owned by the data asset. **The data asset is ACF's authoritative home
for starting items on a character - the component's array is a default that init discards.**

Note this is the ALS save-gate from the first pass, read the wrong way round earlier. The gate
(`!ShouldSaveActor || IsNewGame`) *passing* is what causes the overwrite; it was never blocking.

**Fault 2 - `AllowedWeaponTypes` was empty.** `UACFWeapon::CanBeEquipped` is one line:

```cpp
return equipComp->GetAllowedWeaponTypes().Contains(GetWeaponType());
```

`EquipItemFromInventoryInSlot` calls it and, on false, **returns with no log**. An empty
`AllowedWeaponTypes` therefore blocks every weapon on the character and produces not one line in
`ACFInventoryLog`. Grepping the log for a reason finds nothing, because there is nothing.

This project had **no `WeaponType.*` tags at all** - ACF's taxonomy lives in `/Game/FullSample/`,
which is not installed here (CLAUDE.md 3a). `WeaponType.Axe` was created rather than leaning on the
empty-tag match: an empty `WeaponType` tag does satisfy `Contains(empty)`, so leaving it blank
"works" while silently permitting every untyped weapon onto every character.

**Changed (all data, no C++, no build):**

- `DA_Char_HordeGoblin.StartingItems` = [`BP_Item_GoblinAxe`, count 1, auto-equip]
- `Config/DefaultGameplayTags.ini` - new tag `WeaponType.Axe`
- `BP_Item_GoblinAxe.WeaponType` = `WeaponType.Axe`
- `BP_HordeGoblin` equipment component `AllowedWeaponTypes` = [`WeaponType.Axe`]

**Not changed, and deliberately so:** `item_type` is still `OTHER`. I suspected it was the blocker
and checked before touching it - `CanBeEquipped` gates on slots and weapon type only, and `ItemType`
is not consulted anywhere on the equip path. Changing it would have been a guess that happened to sit
next to the real fault.

## Evaluate (second pass)

Runtime, cold, from a fresh PIE with nothing set by hand:

- **10/10 summoned goblins have the axe equipped straight out of spawn** (`is_equipped == True`).
- The weapon actor `BP_ACFWeapon_GoblinAxe_C` spawns and attaches to the goblin at socket
  `back_sword`; `does_socket_exist('back_sword')` on `GOB_Scout_v3` returns True, so it is a real
  attach and not a silent fallback to the mesh origin.
- `GetCurrentMainWeapon()` is null, **and that is correct** - `MainWeapon` is only ever assigned by
  `UseEquippedItemBySlot`, i.e. by *drawing*. Equipped means sheathed on the back. Calling
  `use_equipped_item_by_slot(ItemSlot.RightHand)` on a live goblin moved the same actor to the main
  weapon slot, so the draw path works when something asks for it.

The diagnosis was proved before any asset was written: on a live pawn, granting the weapon type and
retrying the equip flipped `is_equipped` False -> True with nothing else changed.

**Not established:** what the axe looks like on the goblin's back, and whether anything in the AI
ever draws it. Both are Michael's to judge - PIE has been left running for that.

## Refine (second pass)

Both faults are silent by construction, so the lesson is not "check `AllowedWeaponTypes`" but that
**ACF's equip path fails without logging** - a missing log line is not evidence that a stage was
reached. The first pass read the log, found nothing, and concluded the equip had not been attempted;
it had been attempted and refused.

The earlier note in this ticket about needing a build for slot tags stands, but the second fault
needed no C++ at all - it was two empty arrays in data.

Follow-ups, none of them blocking:

- Nothing draws the axe. Stage 2 was scoped to equipping; whether the horde AI should unsheathe on
  engagement is a design question, not a bug.
- `DA_ACFWeapon_GoblinAxe` is still the wrong shape and should be deleted.
- `item_type = OTHER` on the axe is harmless on the equip path but is probably wrong for inventory
  UI later.
