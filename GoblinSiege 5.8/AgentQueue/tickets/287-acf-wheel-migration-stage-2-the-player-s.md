---
id: 287
title: "ACF wheel migration stage 2: the player's primary weapon moves onto ACF"
agent: claude-warren
status: done
claimed: 2026-08-24T21:00Z
build: required
waiting_on: "Michael: re-check after the camera fix. Attack and watch the camera - it should no longer pull in mid-swing. Then the four visual checks in Evaluate."
evaluated: 2026-08-24T21:19:04Z
observed: 2026-08-24T21:18:56Z | Michael swung the axe and the camera held its distance through the whole animation, where before it pulled in on every swing. The axe sits in his hand at the size it always did, vanishes off his back while the bow is out and comes back when he swaps - all of it now driven by ACF equipment rather than our mesh path, with nothing about the swap or the placement changing under him.
scenario: Michael playing in PIE: attacking with the primary weapon, then swapping between primary, bow and torch.
files: 
  - Source/GoblinSiege/Weapons/GSWeaponComponent.h
  - Source/GoblinSiege/Weapons/GSWeaponComponent.cpp
  - Content/Items/BP_Item_ScoutPrimary.uasset
  - Content/Items/BP_ACFWeapon_ScoutPrimary.uasset
  - Content/Items/BP_ACFWeapon_GoblinAxe.uasset
  - Content/Data/Characters/DA_Char_Player.uasset
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
---

## Goal

Stage 2 of ruling 53. Move **one** weapon - the player's primary - onto ACF, with the flag on, and
prove the two visual rules survive.

**The player's primary is an AXE**, not a sword: `DA_Weapon_Scout.MeleeMesh` is `SM_WoodcutterAxe` at
scale 1.6. The claim was made as "ScoutSword" and renamed before anything was authored - naming it
Sword would have been exactly the mistake #284 just fixed.

## Generate

**`BP_ACFWeapon_ScoutPrimary`** - duplicated from #270's working `BP_ACFWeapon_GoblinAxe`, with the
static mesh component at **scale 1.6**.

**The scale goes on that component, and this is the trap the research named.** `AttachmentOffset` only
moves ACF's own **skeletal** `Mesh` via `AlignWeapon`; our mesh is static, so it lives on a component
added in the Blueprint that `AlignWeapon` never touches. Put the scale on `AttachmentOffset` and it
does nothing at all, silently, and the axe is 1.0 where it should be 1.6.

**Worth flagging for stage 4:** `BP_ACFWeapon_GoblinAxe`'s static mesh is still at **scale 1.0**.
#270 verified that axe equipped and attached; it never verified how it *looked*.

**`BP_Item_ScoutPrimary`** - `UACFWeapon`, `ItemSlots = [ItemSlot.RightHand]`,
`WeaponType = WeaponType.Axe` (the tag #270 already created, and already in the player's
`AllowedWeaponTypes`), `OnBodySocketName = back_sword`, `HandsSocketName = hand_r_weapon` - the same
two sockets `DA_Weapon_Scout` has always used.

**`DA_Char_Player.StartingItems`** gains it with **`AutoEquip = true`** - unlike the arrows, this one
must be worn.

### Three things the stage forced that stage 1 did not need

1. **Mapped-but-EMPTY has to sheathe too.** The Bow slot has been mapped since stage 1, but no bow is
   equipped through ACF until stage 3 - and `UseEquippedItemBySlot` on a slot holding nothing does
   **nothing at all**. Without this, swapping to the bow left the ACF axe in the player's hand,
   silently. `SyncACFEquippedSlot` now checks `GetEquippedItemSlot` and sheathes on a miss.
2. **Our melee mesh must not be built when ACF owns the slot**, or the player carries **two identical
   axes** on one socket - which reads as a rendering bug, not a migration mistake. `IsSlotOwnedByACF`
   gates it, and *destroys* an existing component rather than merely skipping, so toggling the flag
   mid-PIE cleans up after itself.
3. **Equipped is not drawn.** ACF's initializer equips the starting item **sheathed**, and our startup
   sets `CurrentSlot` directly rather than through `SetSlot`, so nothing ever drew it - the player
   spawned with the axe on his back and empty hands. Fixed from **two directions**, because component
   `BeginPlay` order is not guaranteed: we bind `OnEquipmentChanged` to catch equipment that settles
   *later*, **and** draw on the next tick to catch equipment that settled *earlier*. The first attempt
   used only the binding and failed - `UACFCharacterInitializerComponent` had already equipped and
   broadcast before we subscribed.

**The holstered-melee rule now drives ACF's actor**, using the *same expression* as our own mesh
rather than a second copy, and is **re-applied on every `OnEquipmentChanged`**. ACF's
`RefreshEquipment` re-attaches every non-drawn weapon on each of the nine paths that broadcast it, and
`AttachWeaponOnBody` **shows** the actor - so a hide applied once is undone on the next replication
tick and the axe pops back mid-bow.

## Evaluate

**Build clean.** Runtime, PIE on `L_CombatArena`, compared against the **measured** stage-1 baseline:

| | stage 1 (ours) | stage 2 (ACF) |
|---|---|---|
| primary slot | axe `hand_r_weapon` visible | **axe `hand_r_weapon`, not hidden, ACF `MainWeapon` set** |
| bow out | axe `back_sword` **visible=False** | **axe `back_sword` HIDDEN=True** |
| bow out | bow `hand_l_weapon` visible | bow `hand_l_weapon` visible *(unchanged, still ours)* |
| any slot | quiver `spine_quiver` visible | quiver `spine_quiver` visible *(unchanged, still ours)* |

**Torch slot** - the unmapped case: ACF sheathes, `MainWeapon` is None, the hand is free for the
torch, and the axe sits on `back_sword` **visible** - correct, because the original rule only hides it
for the bow or an aim, and the torch is neither.

**The swap lock still bites**, which matters because ACF has no equivalent and would let input spam it
every frame: three presses in a row give `True, False, False`.

**One test I got wrong and re-ran.** The first swap-lock check returned `False, False, False` and I
nearly recorded a bug. It was correct - I had swapped in the same script microseconds earlier, so all
three were legitimately locked out. **My expectation was wrong, not the code.** Re-run across a round
trip so the 0.15s had expired, it gave the right answer.

**NOT ESTABLISHED, AND IT IS THE POINT OF THIS STAGE: nobody has looked at the axe.** Every check above
confirms the right socket, the right owner and the right visibility flag. **None of them can see that
the axe is the right size, the right way up, or in the right place in the hand** - and the ledger says
a migration that silently changes placement has failed even if it equips. #270 shipped an ACF axe that
was never looked at and is, on this evidence, at the wrong scale.

**PIE is left running in the Primary slot.** Four things:

1. **The axe in hand** - same size and grip as before, not doubled, not tiny.
2. **The axe on the back** - swap to the bow; it should vanish, and stay gone the whole time the bow
   is out, not reappear when you loose.
3. **Swap back** - it returns to the hand.
4. **Torch** - the axe is visible on the back, the torch is in the off hand.

## Refine

**The initial-draw fix went in twice before it worked**, and the second version is the one that is
right for the reason rather than by luck. Binding alone missed it because the initializer had already
broadcast; a direct call in `BeginPlay` would have missed it the other way, because our own
`EquipWeapon` has not run yet at that point. The next-tick timer fires after the whole chain in either
order, which is the only version that does not depend on guessing an ordering the engine does not
promise.

**A re-entrancy guard was added with it.** `UseEquippedItemBySlot` broadcasts `OnEquipmentChanged`,
which we now listen to and which can draw - a direct route back into itself.

**Deliberately not done:** the bow, the quiver, the horn and the torch all stay on our mesh path. The
quiver especially must never become an ACF item - `RefreshEquipment` would re-attach and re-show it on
every equipment change, breaking "the quiver never moves and is never hidden".


---

### The camera lurch Michael found, and it was not the camera

**Reported:** *"something strange is happening as I attack, either the camera is briefly zooming in,
or I'm scaling really large and then small at the end of every animation sequence."*

**It was the first of the two, and stage 2 caused it.** The ACF weapon actor's static mesh was
`QUERY_AND_PHYSICS` on the `BlockAllDynamic` profile, so it **blocked `ECC_Camera`**. Swinging the axe
put a camera-blocking body through the line between the spring arm and the player, and
`bDoCollisionTest` yanked the camera in for the length of the swing.

This is #145 exactly - *"camera lurches in and out during a crowd fight: every character blocks the
spring arm's camera probe"* - returning through a new door. #145's fix was to make
`AGSCharacterBase` set its capsule and mesh to ignore `ECC_Camera`; every weapon mesh **our** path
creates is `NoCollision` for the same reason. **The ACF actor is a new actor nobody had told.**

**Both axes were fixed, not just the player's.** `BP_ACFWeapon_ScoutPrimary` was duplicated from
`BP_ACFWeapon_GoblinAxe`, which carried the same blocking mesh - so **since #270 every summoned goblin
has been carrying a camera-blocking axe**, and with ten of them around the player that is #145's
crowd lurch from a source nobody had connected to it. It was never noticed because #270 verified that
the axe equipped, never how it behaved.

Verified at runtime after the fix, with the horde summoned: **no primitive on either weapon actor
blocks the camera**, and the axe still draws into `hand_r_weapon` at scale 1.6.

**A near-miss worth recording.** The first fix attempt printed "saved both" and had changed **nothing**
- `EditorAssetSubsystem::load_asset` returns None immediately after `StopPIE`, so the loop iterated an
empty handle list and the success message was a lie. It was caught by re-reading the asset rather than
trusting the print, and the retry asserts on both the load and the component count so it cannot pass
silently again. Also: the property is not `collision_enabled` on the component but
`body_instance.collision_enabled`, with the profile set alongside it.
