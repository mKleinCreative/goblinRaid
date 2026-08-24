---
id: 286
title: "ACF wheel migration stage 1: the WeaponSlot to ItemSlot mapping and the choke point, flag off"
agent: claude-warren
status: done
claimed: 2026-08-24T20:45Z
build: required
waiting_on:
evaluated: 2026-08-24T20:40:37Z
observed: 2026-08-24T20:40:38Z | The game plays exactly as it did: the wheel picks the same four slots in the same directions, swapping to the bow still works and still puts the player in ranged mode, the axe hangs hidden on his back while the bow is out and the quiver stays put and visible. ACF was told nothing - no main weapon, no weapon actors in the world - which is the point, because the flag is off.
scenario: PIE on L_CombatArena after an editor-closed rebuild, swapping slots on the live player and reading both our mesh path and ACF equipment side by side.
files: 
  - Source/GoblinSiege/Weapons/GSWeaponComponent.h
  - Source/GoblinSiege/Weapons/GSWeaponComponent.cpp
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
---

## Goal

Stage 1 of the ACF weapon-wheel migration (ruling 53, #283). Build the seam and **change nothing on
screen**. The flag stays off; the whole point of this stage is that it is invisible.

## Generate

**`bUseACFEquipment`**, `EditDefaultsOnly`, **default false**. While it is off, nothing in
`UGSWeaponComponent` talks to `UACFEquipmentComponent` and the game behaves exactly as before -
which is what makes every later stage independently closeable and independently revertable.

**`WeaponSlotToItemSlot`** - a `TMap<FGameplayTag, FGameplayTag>` seeded in the constructor:

```
WeaponSlot.Primary -> ItemSlot.RightHand      (hand_r_weapon, the melee socket)
WeaponSlot.Bow     -> ItemSlot.LeftHand       (hand_l_weapon, the ranged socket)
```

**Torch and Grapple are deliberately ABSENT rather than mapped to something harmless.** The torch is
a held prop driven by `SetTorchReadied` and must never be modelled as a `UACFConsumable` - ACF's
consumable path *uses and destroys* the item. The grapple is a verb with no weapon at all.

**`SyncACFEquippedSlot()`**, called from `SetSlot` and only from `SetSlot`, after the hands are right
and before the broadcasts - the same ordering `RefreshWeaponMeshPlacement` already uses, so a listener
never sees a state its hands disagree with.

Three things it does that the research made necessary:

- **An unmapped slot sheathes EXPLICITLY** rather than asking ACF. `UseEquippedItemBySlot` on a slot
  ACF has no equipped item for does **nothing at all** - no sheathe, no broadcast, no log - which
  would leave the sword in hand while the player holds a torch, with nothing anywhere to notice.
- **`UseEquippedItemBySlot` is never reachable from input.** It has no anti-cancel lock of its own,
  where `SetSlot` has the 0.15s `SwapInputLockSeconds`.
- **A same-slot call can never reach ACF**, because `SetSlot` already refuses one. That matters: ACF
  reads re-selecting the slot you already hold as **"sheathe everything"**, not as a no-op.

`BP_GSPlayerCharacter`'s equipment component gains `AvailableEquipmentSlot =
[ItemSlot.RightHand, ItemSlot.LeftHand]` and `AllowedWeaponTypes = [WeaponType.Axe]`. Both were empty,
which #269 recorded as the reason nothing could be equipped through ACF at all.

## Evaluate

**Build succeeded in 47s**, editor closed, only the two pre-existing `C4996` warnings.

**Runtime, PIE on `L_CombatArena`. The check that IS this stage:**

```
flag on the live pawn          False
ACF available slots            ItemSlot.RightHand, ItemSlot.LeftHand   (ready for stage 2)
ACF main weapon                None
wheel sectors                  Torch / Primary / Grapple / Bow          (unchanged)
swap to bow                    True, slot = WeaponSlot.Bow, ranged = True
ACF main weapon AFTER the swap None
ACF weapon actors in world     0
```

**The swap happened and ACF was not told.** That is the whole verification: it proves the flag
actually gates the new path rather than being decorative, which a compile cannot show.

### The baseline for stage 2, captured deliberately

With the bow drawn, our mesh path reports:

```
SM_WoodcutterAxe   socket=back_sword      visible=False    <- holstered melee HIDDEN while bow is out
GS_Bow_Only        socket=hand_l_weapon   visible=True
GS_Quiver          socket=spine_quiver    visible=True     <- quiver never moves, never hidden
```

Those are the two project-policy visual rules ACF has no equivalent for, and the ledger says a
migration that silently changes them **has failed even if it compiles and equips**. Recording them now
means stage 2 is compared against a measured before, not a remembered one.

**Not established:** anything about ACF actually holding a weapon. Nothing has been equipped through
it and the flag has never been on. That is stage 2, and it is the first stage Michael has to look at.

## Refine

**The mapping is data, not a switch statement**, deliberately. A `switch` over four tags would have
been shorter and would have made a fifth slot a C++ change - the same trap #274 removed from
`SlotForDirection` when it made the wheel's contents an array.

**Absent beats mapped-to-nothing.** The first draft mapped Torch and Grapple to an invalid tag and let
`SyncACFEquippedSlot` reject it. That is worse: it puts a `WeaponSlot` tag where an `ItemSlot` belongs,
and ACF validates item slots against the `Itemslot` root and early-returns at **Log** verbosity on a
miss - invisible at default levels. A missing key cannot be mistaken for a working one.

**Property naming caught me out once, harmlessly:** UHT strips the leading `b`, so `bUseACFEquipment`
is `use_acf_equipment` from Python, not `b_use_acf_equipment`. It failed loudly rather than reading a
default, which is the good kind of wrong.

**Deliberately not done:** no weapon moved, no `UACFWeapon` authored, no `AACFWeaponActor` subclass,
and `UGSWeaponComponent`'s mesh path is untouched. Stage 5 retires whatever of it is provably dead -
**not before**, because the torch, horn and quiver still use it.
