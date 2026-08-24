---
id: 278
title: Finite arrows stage 1 - the arrow item and the player quiver
agent: claude-warren
status: done
claimed: 2026-08-24T04:04Z
build: none
waiting_on:
evaluated: 2026-08-24T04:08:08Z
observed: 2026-08-24T04:08:09Z | The player spawned carrying thirty arrows as a single stack weighing 1.5 of his 180 capacity, none of them equipped - and the bow still loosed an arrow into the world with the count staying at thirty, which is the point of this stage. Before this the inventory was empty and nothing in the game had a count of anything.
scenario: Fresh PIE on L_CombatArena, reading the live player pawn rather than the CDO, then swapping to the bow slot and firing GSGA_BowShot on the live ability system.
files: 
  - Content/Items/BP_Item_Arrow.uasset
  - Content/Data/Characters/DA_Char_Player.uasset
---

## Goal

Stage 1 of the finite-arrows plan (ruling 46, #277). Author the arrow item and give the player a
quiver. **Nothing consumes it yet** - the bow must behave exactly as it does today. Data only, no
build.

## Generate

**`Content/Items/BP_Item_Arrow`** - a Blueprint subclass of **`UACFItem`**, deliberately not
`UACFProjectile`. `UACFProjectile` derives from `UACFEquippableItem` and drags in the equip path -
`CanBeEquipped`, `WeaponType`, `AllowedWeaponTypes` - every one of which fails **silently** when
unsatisfied (#270's Fault 2). An arrow that is never equipped should never touch that code.

Per #270, ACF items are Blueprint **subclasses**, not asset instances: `UACFItem` is a plain
`UObject`, `DataAssetFactory` refuses it, and `FStartingItem::ItemClass` is a `TSubclassOf`.

**The two `FItemDescriptor` defaults that would have silently eaten arrows.** Both were confirmed
live on the freshly created asset before being changed:

| Field | ACF default | Set to | What the default does |
|---|---|---|---|
| `ItemWeight` | **5.0** | `0.05` | `MaxInventoryWeight` is 180, so ~36 arrows exhaust the budget and every further add returns `-1` **with no log** |
| `MaxInventoryStack` | **1** | `120` | `MaxInventorySlots` is 40, so 30 arrows would take 30 slots and the excess is clamped **with no log** |

Also set: `Name` "Arrows", a description, `Droppable = false` (we do not use ACF's death-drop path)
and `Sellable = false` (no vendor exists). `ItemSlots` left empty and `ItemType` left `Other` - #270
established `ItemType` is not consulted on any path this touches.

**`DA_Char_Player.StartingItems`** = `[BP_Item_Arrow x 30, AutoEquip = false]`.

**On the DATA ASSET, not the component.** #270's Fault 1: `ApplyEquipData` calls
`SetStartingItems(CharacterDataAsset->StartingItems)`, which **overwrites** whatever the equipment
component held. Authoring on `BP_GSPlayerCharacter`'s component instead yields zero arrows and no
log, which is exactly the failure that cost #270 a full diagnostic pass.

`AutoEquip = false` is load-bearing, not tidiness: `true` routes the add into `HandleItemAdded` and
the `CanBeEquipped` gate, which returns false with no log. Passing false explicitly means a *real*
future equip failure will not be mistaken for this one.

## Evaluate

**Runtime, fresh PIE on `L_CombatArena`, reading the live pawn** - not the CDO, because #270 proved a
template can read correctly while the live pawn reads zero:

```
live starting_items       1     <- 0 would mean it was authored on the component (#270 Fault 1)
TOTAL ARROWS             30
inventory entries         1     <- 30 would mean MaxInventoryStack was still 1
  entry: count=30, is_equipped=False
inventory total weight  1.5 / 180   <- 150 would mean ItemWeight was still 5.0
```

Every one of those four is a check on a failure mode that produces **no log line**. The plan called
them out in advance and each was measured rather than assumed.

**The regression check for this stage was performed, not argued.** The temptation was to reason "no
C++ and no ability asset was touched, so the bow cannot have changed". Instead the bow was fired:
swapped the player to `WeaponSlot.Bow`, activated `UGSGA_BowShot` on the live ASC, and confirmed

- an arrow actor **spawned** in the world (`GSArrowProjectile_0`), and
- the count **stayed at 30**.

That is precisely Stage 1's target state - a quiver that exists and a bow that does not yet know
about it.

No ACF inventory complaints in the log. The only errors in the run were my own Python probe's
traceback from a mis-named property, which is not the game.

**Not established:** that the count survives a respawn, and that the numbers are right. 30 arrows,
120 per stack and 0.05 weight are all first guesses - the plan says so. Whether 30 is the right
quiver is a play question that only lands once Stage 3 makes it cost something.

## Refine

**Two property names differed from the plan and were found by reading rather than guessing.**
`FItemDescriptor` exposes `droppable` / `sellable`, not `b_droppable` / `b_sellable`, and
`UACFEquipmentComponent` has no `current_inventory_weight` property - the accessor is
`GetCurrentInventoryTotalWeight()`. Both surfaced as immediate exceptions rather than silent no-ops,
which is the good kind of failure.

**Deliberately not done:** nothing consumes, nothing displays, and no C++ was touched. `ArrowItemClass`
on `UGSWeaponDataAsset` and the gate in `UGSGA_BowShot` are Stage 2 and need a build; the HUD count is
Stage 4. This stage is independently closeable with the game playing exactly as it did before it.
