---
id: 104
title: Erika finally holds a bow
agent: claude-gobkit
status: done
claimed: 2026-08-09T20:01Z
build: none
waiting_on:
evaluated: 2026-08-09T20:06:00Z
files: 
  - Content/Data/Weapons/DA_Weapon_Erika.uasset
  - Content/Blueprints/Adversaries/BP_ErikaArcher.uasset
  - Source/GoblinSiege/Characters/GSEnemyCharacter.h
  - Source/GoblinSiege/Characters/GSEnemyCharacter.cpp
---

## Goal

Erika finally holds a bow

## Generate

The last combatant with empty hands. She could already shoot - `RangedAttackAbilityClass` is
`GSGA_BowShot` and #094 verified archers drawing and landing arrows - she simply had no bow to hold.

**`AGSEnemyCharacter::DefaultSlot`** (`EGSWeaponSlot`, defaults to `Sword`). `BeginPlay` hardcoded
`SetSlot(Sword)`, and its own comment predicted this: *"an archer overrides this to Bow once it has
bow content of its own"*. Left as it was, Erika's bow would have gone to `RangedHolsterSocket` -
`back_bow`, **which the human skeleton does not have** - and attached at the actor root, floating at
her feet. `GSEnemyCharacter.h` now includes `GSWeaponComponent.h`, because a UENUM held by value in a
UPROPERTY cannot be forward-declared.

**`DA_Weapon_Erika`**, built fresh (the #097 rule): `GS_Bow_Only` on `hand_l_weapon` at scale 1.25,
matching the player's bow after Michael's +25%.

- **No melee mesh.** A sword welded to an archer's hand while she draws would look worse than nothing.
- **No quiver.** `spine_quiver` is not authored on `SK_Human_Skeleton`, so it would attach at the
  actor root - the exact floating-bow failure this ticket exists to avoid.

## Evaluate

**`GS_Bow_Only`'s pivot is at a limb TIP, not the grip** - the same trap as the guard sword. Measured
rather than assumed: z spans 0..98.3, and the vertex X-width profile is narrow at both ends (9.2 and
6.7) and widest at mid-height (25.6), which is the bow's belly. So the offset carries a translation of
`-(bounds centre x scale)` to centre the bow on her hand instead of hanging it by one tip.

**That translation is an ESTIMATE and I am not going to dress it up.** The bounds centre is not
exactly the riser, and the rotation is identity - a starting point, not a tuned value. It is deliberate:
Michael now has live socket preview, and `hand_l_weapon` is the right place to finish this by eye in
seconds. What matters is that she is holding a bow roughly in her hand rather than dragging it by a
limb.

**Consequence worth stating: the socket preview will LIE for the bow**, the same way it would have for
the sword before #103, because the data asset is again compensating for a bad pivot. The fix is the
same bake - and this time it needs the player's `DA_Weapon_Scout` compensated in the same pass, since
it shares `GS_Bow_Only` with an identity offset. Not done here: Michael asked for Erika's bow, not for
his own bow to be moved.

**BUILT AND VERIFIED.** `BUILD SUCCEEDED in 01:57`, no new warnings. Dirty-package check first: zero
maps, zero content, so nothing unsaved was at risk in closing the editor.

```
Erika default_slot = BOW      BP_CastleGuard01 = SWORD   BP_KnightDPelegrini = SWORD
BP_ErikaArcher_C_0  GS_Bow_Only  centre 0.0uu from hand_l_weapon  scale 1.25  socket='hand_l_weapon'
bow world extent (100.0, 105.9, 83.6) -> 105.9uu, against Erika's ~110uu actor-to-head
```

The garrison check matters as much as Erika's: a new defaulted property is exactly the kind of change
that can silently re-slot every other defender, and it did not.

**A second bug, found only because the first fix did not work.** With `DefaultSlot = Bow` correctly set
and `hand_l_weapon` confirmed to exist at runtime, the bow still attached at the actor root with
`socket='None'`. Cause: **`bHasRangedMode` defaults to false**, and `RefreshWeaponMeshPlacement` gates
on `IsInRangedMode() && EquippedWeapon->bHasRangedMode` - so the ranged half was routed to the holster
socket `back_bow`, which humans do not have. `DA_Weapon_Scout` has had it true all along; a
freshly-created asset does not. Set to true, and the attach resolved.

**Worth noting how close this came to being reported as working:** `default_slot = BOW` read back
correctly, the socket existed, the weapon was equipped - three green checks and the bow was still on
the floor. The attach-socket name is the only field that showed it.

## Refine

**Made the slot a property rather than special-casing the archer class.** A `bIsArcher` bool or an
`AGSArcherCharacter` subclass would both have been narrower answers to the same question; a slot
enum on the defender is the one that also covers the torch-carrier that will want it next.

**Deliberately left undone:** `spine_quiver` on the human skeleton (she will carry no arrows until it
exists), the bow pivot bake plus player compensation, and the bow's resting rotation in her hand.
