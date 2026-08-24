---
id: 271
title: ItemSlot gameplay tags so ACF equipment has slots to equip into
agent: claude-warren
status: review
claimed: 2026-08-24T00:27Z
build: required
waiting_on:
evaluated: 2026-08-24T00:28:54Z
observed:
scenario:
files: 
  - Source/GoblinSiege/Combat/GSGameplayTags.h
  - Source/GoblinSiege/Combat/GSGameplayTags.cpp
---

## Goal

ItemSlot gameplay tags so ACF equipment has slots to equip into

## Generate

Three tags in `GSGameplayTags.{h,cpp}`, following the file's existing convention exactly
(`UE_DECLARE_GAMEPLAY_TAG_EXTERN` in the header namespace, `UE_DEFINE_GAMEPLAY_TAG_COMMENT` with a
dotted name and an explanation in the cpp):

| tag | socket it corresponds to |
|-----|--------------------------|
| `ItemSlot.RightHand` | `hand_r_weapon` - the axe and the sword |
| `ItemSlot.LeftHand` | `hand_l_weapon` - a drawn bow |
| `ItemSlot.Back` | `back_sword` / `back_bow` holsters |

**Named for the sockets deliberately.** ACF addresses slots by tag where this project has always used
the `EGSWeaponSlot` enum, and the two vocabularies have to be reconciled by hand eventually
(Phase 3 stage 3). Naming the tags after sockets keeps that mapping obvious instead of inventing a
third naming scheme nobody can line up.

**Declared in C++, not an ini,** because that is how all 45 tags in this project are declared - there
is no `DefaultGameplayTags.ini` here at all. ACF's own config ships only `ItemSlot.ItemSlot`, which
is a category root rather than a usable slot.

**Only three.** Enough for the axe (stage 2) and for the bow and holsters when they follow. No
armour slots: nothing in this project has armour as an equippable item, and inventing tags for it
would be guessing at a design that does not exist yet.

## Evaluate

**NOT COMPILED.** Gameplay tag macros are reflection-adjacent and this project's own notes say Live
Coding cannot register new reflection - so these need a full build with the editor closed before they
exist to anything.

**Verified:** three declarations and three definitions, counted in both files, matching the
surrounding style.

**Not verified, and it is the whole point of the next step:** that `UACFEquipmentComponent` does
anything useful once `AvailableEquipmentSlot` contains one of these. The tags are a precondition, not
a result.

**Scope note:** `GSGameplayTags.{h,cpp}` is included by most of the project, so this forces a full
rebuild for everyone. Additive only - no existing tag was touched, renamed or reordered - so nothing
that compiles today should stop compiling.

## Refine

Nothing changed on review.

**Deliberately not done:** no ACF-facing code consumes these yet. Declaring the tags and wiring the
equipment component are separate steps on purpose, so that if the equipment path turns out not to fit
this project, the cost of backing out is three lines rather than a half-migrated character.

> 2026-08-24T00:28Z Three ItemSlot tags declared. Needs a full build - new tags are reflection-adjacent and Live Coding cannot register them.
