---
id: 284
title: The wheel slot is Primary, not Sword - the goblin holds an axe
agent: claude-warren
status: done
claimed: 2026-08-24T17:45Z
build: required
waiting_on:
evaluated: 2026-08-24T20:06:32Z
observed: 2026-08-24T20:06:33Z | The wheel picks the same weapon in the same direction it always did - up torch, left primary, down grapple, right bow, with every boundary from 44 to 316 degrees landing where it did before. The player, both castle guards and the summoned goblins all stand in the Primary slot holding three different weapons between them, and Erika is still on the bow. Every adversary kept its serialized slot through the rename without anyone re-authoring it.
scenario: PIE on L_CombatArena after an editor-closed rebuild, driving every wheel sector and boundary, then summoning the horde alongside the level defenders.
files: 
  - Source/GoblinSiege/Combat/GSGameplayTags.h
  - Source/GoblinSiege/Combat/GSGameplayTags.cpp
  - Source/GoblinSiege/Weapons/GSWeaponComponent.cpp
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - Source/GoblinSiege/Characters/GSEnemyCharacter.cpp
  - Source/GoblinSiege/Horde/GSHordeGoblin.cpp
  - Source/GoblinSiege/UI/GSWeaponWheelWidget.h
  - Source/GoblinSiege/UI/GSWeaponWheelWidget.cpp
  - Config/DefaultGameplayTags.ini
  - Content/UI/WBP_WeaponWheel.uasset
---

## Goal

Michael: *"can we change references to the 'Sword' to 'Primary' weapon... as far as the weapon wheel
goes."*

The old name described **the player's kit** rather than **the slot's job**. The horde's primary is an
**axe** (#270), so `WeaponSlot.Sword` meant the log said a goblin "is now holding Sword" while it held
an axe - a name that reads as a bug every time it is correct.

## Generate

`WeaponSlot.Sword` -> **`WeaponSlot.Primary`**, and `Label_Sword` -> `Label_Primary`. 16 references
across 8 files, plus the tag definition, the ini and the wheel widget.

**Scoped to the wheel, deliberately.** Actual swords are still swords: `DA_Weapon_ArmingSword`, the
`back_sword` socket, `SM_Sword_Arming01` and `UGSGA_SwordLight` are all untouched. The slot is
"whatever your main melee weapon is"; the asset is a sword. Renaming those too would have been a
different and much larger change, and Michael scoped this himself with "as far as the weapon wheel
goes".

**A `GameplayTagRedirect` carries the serialized values**, and this is the interesting part:

```ini
+GameplayTagRedirects=(OldTagName="WeaponSlot.Sword",NewTagName="WeaponSlot.Primary")
```

`AGSEnemyCharacter::DefaultSlot` is a tag written into every adversary CDO. **#274 could not redirect
its equivalent** - that change was `EGSWeaponSlot` (an enum) to `FGameplayTag` (a struct), a *type*
change, and CoreRedirects rewrite names rather than types, so Erika's `Bow` was lost and had to be
re-authored by hand. This is a *rename within one type*, which is exactly what tag redirects are for.

## Evaluate

**Build succeeded in 1:41**, editor closed, only the two pre-existing `C4996` warnings.

**The redirect worked, and it is the check that mattered.** Every serialized value survived with no
hand re-authoring:

```
BP_KnightDPelegrini  WeaponSlot.Primary     BP_ErikaArcher     WeaponSlot.Bow
BP_CastleGuard01     WeaponSlot.Primary     BP_PeasantMan      WeaponSlot.Primary
BP_CastleGuard02     WeaponSlot.Primary     BP_UrielAPlotexia  WeaponSlot.Primary
```

Registered tags are now `WeaponSlot.Bow | Grapple | Primary | Torch` - the old tag is **gone**, not
merely shadowed, so nothing can quietly keep using it.

**The wheel is byte-for-byte the same wheel**, checked at every sector and every boundary rather than
at the four cardinals, because a rename that shifted a sector would be invisible in a diff:

```
up -> Torch   left -> Primary   down -> Grapple   right -> Bow
44=Bow  46=Torch  134=Torch  136=Primary  224=Primary  226=Grapple  314=Grapple  316=Bow
```

Identical to what #274 recorded. **A player who has learned this wheel does not have to relearn it.**

**Runtime, with the horde summoned:** player, both castle guards and the goblins all in
`WeaponSlot.Primary` holding their own weapons - `DA_Weapon_Scout`, `DA_Weapon_ArmingSword`,
`DA_Weapon_HordeGoblin` - and Erika in `WeaponSlot.Bow`, ranged. Which is the point: one slot name,
three different weapons in it.

`WBP_WeaponWheel` renamed its label to `Label_Primary` with text "PRIMARY" and still compiles
(`BS_UP_TO_DATE`).

**Not established:** what the wheel LOOKS like with the new label. The text was set programmatically
and the widget compiles, but nobody has opened the wheel in play to see "PRIMARY" sitting where
"SWORD" used to.

## Refine

**The widget binding being `BindWidgetOptional` rather than `BindWidget` is what made this safe**, and
the header says why it was chosen: `BindWidget` makes a missing widget a **compile error on the
Blueprint**. Renaming a required binding would have broken `WBP_WeaponWheel` at the moment the C++
changed and before the widget could be renamed to match. With Optional the two halves could move
independently, and the worst case was a null label for one build.

**Deliberately not renamed:** `DA_Weapon_ArmingSword`, `back_sword`, `SM_Sword_Arming01`,
`UGSGA_SwordLight`, `GSGA_SwordHeavy` and the human `GA_HU_Sword*` abilities. Those describe a sword,
which is a real thing that still exists. Only the *slot* was misnamed.

**One thing to watch in the next migration:** ruling 53 moves the wheel onto ACF equipment, where a
slot becomes an `ItemSlot.*` tag. `WeaponSlot.Primary` and `ItemSlot.RightHand` are still different
axes (#274), and this rename makes the distinction easier to hold rather than harder - "Primary" is
plainly a loadout choice, where "Sword" could be misread as a thing that hangs somewhere.
