---
id: 274
title: "Stage 3 - EGSWeaponSlot retired: the weapon wheel is four gameplay tags and its contents are data"
agent: claude-warren
status: done
claimed: 2026-08-24T02:55Z
build: none
waiting_on:
evaluated: 2026-08-24T03:18:35Z
observed: 2026-08-24T03:18:46Z | Dragging the wheel in each direction picked the same weapon it always did - up torch, left sword, down grapple, right bow - and every sector boundary from 44 through 316 degrees landed where the old enum put it. Swapping to the bow put the player in ranged mode. Sixteen NPCs came up holding the right thing, both Erikas with bows drawn and ranged, the guards and all ten goblins with swords.
scenario: PIE on L_CombatArena after a full rebuild, driving SlotForDirection through every sector and boundary angle on the live player component, then summoning the horde alongside the level defenders.
files: 
  - Source/GoblinSiege/Weapons/GSWeaponComponent.h
  - Source/GoblinSiege/Weapons/GSWeaponComponent.cpp
  - Source/GoblinSiege/UI/GSWeaponWheelWidget.h
  - Source/GoblinSiege/UI/GSWeaponWheelWidget.cpp
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - Source/GoblinSiege/Characters/GSEnemyCharacter.h
  - Source/GoblinSiege/Combat/GSGameplayTags.h
  - Source/GoblinSiege/Combat/GSGameplayTags.cpp
  - Source/GoblinSiege/Horde/GSHordeGoblin.cpp
  - Source/GoblinSiege/Horde/GSHordeOrderTypes.h
  - Content/Blueprints/Adversaries/BP_ErikaArcher.uasset
---

## Goal

Plan the retirement of `EGSWeaponSlot`. **No code is written by this ticket.**

## Generate

### The surface, measured

**48 references across 9 files**, and they are concentrated:

| File | refs |
|---|---|
| `Weapons/GSWeaponComponent.cpp` | 17 |
| `Weapons/GSWeaponComponent.h` | 10 |
| `UI/GSWeaponWheelWidget.cpp` | 9 |
| `UI/GSWeaponWheelWidget.h` | 4 |
| `Characters/GSPlayerCharacter.cpp` | 3 |
| `Characters/GSEnemyCharacter.h` | 2 |
| `Combat/GSGameplayTags.h`, `Horde/GSHordeGoblin.cpp`, `Horde/GSHordeOrderTypes.h` | 1 each |

Two thirds of the work is in one component and its widget.

### The two things that could have made this expensive, and neither does

**Blueprint graph logic: there is none.** `WBP_WeaponWheel` reports **0 variables and 0 functions** -
it is a skin over the C++ `UGSWeaponWheelWidget`, and its only referencer is
`BP_GSPlayerCharacter`. `WBP_GSPlayerHUD` has only an overridden `Tick`. Nothing switches on the
enum in Blueprint, so nothing has to be re-authored by hand in a graph.

**Serialised CDO values: exactly one is non-default.** `AGSEnemyCharacter::DefaultSlot` is an
`EGSWeaponSlot` written into every adversary CDO. Read live from all six:

```
BP_KnightDPelegrini  SWORD    BP_ErikaArcher     BOW   <-- the only one
BP_CastleGuard01     SWORD    BP_PeasantMan      SWORD
BP_CastleGuard02     SWORD    BP_UrielAPlotexia  SWORD
```

`SWORD` is also the C++ default, so five of six re-serialise correctly by accident. **Erika is the
single value that must be re-authored by hand** - and it must be by hand, because an enum to
`FGameplayTag` change is a *type* change and **CoreRedirects cannot carry it** (they rewrite names,
not types; #189 used them for a rename, which is a different thing).

The header's "APPEND ONLY - this enum's integer values are written to the CDO and to any saved
widget binding" warning is accurate about the hazard but overstates the blast radius as it stands
today: the widget bindings it worries about turned out to be C++-side.

### The one piece of real design work

`UGSWeaponComponent::SlotForDirection(FVector2D, EGSWeaponSlot)` maps a wheel drag direction to a
slot, and `IsInRangedMode()` is `CurrentSlot == EGSWeaponSlot::Bow`. Both depend on the enum being a
small closed set with a known order. With tags, the order has to come from somewhere explicit - an
ordered `TArray<FGameplayTag>` of wheel slots, authored per character - which is *the point* of the
change (slots become data) but is also the only part that is design rather than mechanical
substitution.

`IsInRangedMode()` should become a property of the slot's weapon, not a comparison against one
particular slot. It has callers throughout ranged combat.

## The decision - this is what the ticket is waiting on

`EGSWeaponSlot` (Torch / Bow / Sword / Grapple) and the `ItemSlot.RightHand|LeftHand|Back` tags added
in #271 are **not the same axis, and conflating them would be a real mistake**. The enum is a
*loadout selection* - which weapon the player has chosen. The ItemSlot tags are *attachment points* -
where an item physically hangs. A torch and a sword can both live in `ItemSlot.RightHand`.

**Option A - mirror the enum as a new `WeaponSlot.*` tag family.** A near-mechanical type swap.
Removes the append-only serialisation hazard, makes the wheel data-driven, one CDO to re-author.
Roughly one focused session plus a build. **But it delivers nothing the player can see, and it does
not move us any closer to ACF** - it is a better version of our own parallel system.

**Option B - model each wheel slot as an ACF equipment slot, so switching weapons becomes
`UACFEquipmentComponent::UseEquippedItemBySlot(tag)`.** This is the actual ACF migration: the wheel
becomes a view over ACF equipment rather than a system beside it, and it reuses the draw/sheathe path
already proven working in #270. Considerably larger, and it lands on top of `UGSWeaponComponent`,
which currently owns equipping, meshes, sockets, abilities and the wheel all at once.

**Recommendation: A first, then B - but only if B is genuinely the destination.** If the wheel is
going to stay ours indefinitely, A is churn for its own sake and the enum should simply be left
alone with its append-only comment. The question worth answering before any code is written is not
"tags or enum", it is **"is the weapon wheel migrating onto ACF at all?"** #269 already recorded that
`UGSWeaponComponent` and `UACFEquipmentComponent` do overlapping jobs and both are live.

## Evaluate

**Established by direct measurement:** the reference counts per file; that `WBP_WeaponWheel` carries
no Blueprint graph logic; that Erika's `DefaultSlot` is the only non-default serialised enum value;
that the two `BlueprintAssignable` delegates carry the enum in their signatures.

**Not established:** whether anything outside the project's own C++ binds
`FGSOnWeaponSlotChanged` / `FGSOnWheelHighlightChanged`. The widget has no graph, so a Blueprint
binding is unlikely, but "unlikely" is not "checked" - a grep of the C++ found the bindings; no one
has opened every graph in the project. **Do this check first if A or B is approved**, because a
Blueprint-side binding to a changed delegate signature fails at compile in a way that is easy to miss
in a project this size.

**Deliberately not planned:** the sequencing of B's interaction with `UGSWeaponComponent`'s other
responsibilities. That is a decomposition exercise worth its own ticket, and doing it now would be
planning work that a "no" to the decision above would throw away.

## Refine

**Scope held to a plan.** The instruction was to plan stage 3, and the temptation was to start on
option A immediately since it is mechanical and the blast radius turned out small. It is not started,
because A's entire value is as a stepping stone to B, and whether B happens is Michael's call - doing
A first and hearing "no" afterwards would be churn nobody asked for.

**What changed while planning:** the job looked bigger than it is. The expectation from the parked
stage-3 note was "touches weapon wheel, HUD, input, abilities" and a hand re-authoring of Blueprint
graphs. Measuring first turned that into 48 references, no Blueprint logic and one CDO field. That
should raise the estimate's confidence, not lower the bar for deciding whether to do it.


---

### IMPLEMENTED, 2026-08-24

Michael: *"Let's do Stage 3."* Taken as **option A** - the enum-to-tag swap Stage 3 was always
scoped as. Option B (wheel slots become ACF equipment slots) was my own addition above and is a
later stage; it is NOT done here and the question it asks is still open.

**Changed:**

- `Combat/GSGameplayTags.{h,cpp}` - four new tags, `WeaponSlot.Torch|Bow|Sword|Grapple`, with a
  header comment stating plainly that they are a different axis from `ItemSlot.*`.
- `Weapons/GSWeaponComponent.h` - the `UENUM` is gone, replaced by a headstone comment saying where
  it went and why the append-only hazard died with it. `CurrentSlot`, `WheelHighlight`, `SetSlot`,
  `GetCurrentSlot`, `GetWheelHighlight`, `SlotForDirection` and both `BlueprintAssignable` delegates
  now carry `FGameplayTag`. `IsInRangedMode()` moved out of line.
- **`WheelSlots` added** - `TArray<FGameplayTag>`, `EditDefaultsOnly`, filtered to the `WeaponSlot`
  category. **This is the actual point of the ticket.** The wheel's arrangement used to be four
  hard-coded returns; it is now data, and `SlotForDirection` derives its sector size from the
  array's length, so a three- or five-slot wheel divides itself with no C++ change.
- `UI/GSWeaponWheelWidget.{h,cpp}` - the `switch` becomes a tag chain that returns **null** for an
  unknown slot rather than falling through to Sword. Lighting the wrong label is a worse lie than
  lighting none.
- `Characters/GSEnemyCharacter.{h,cpp}` - `DefaultSlot` is a tag; its default moves to the
  constructor because a native gameplay tag is not a constant expression.
- `Characters/GSPlayerCharacter.cpp`, `Horde/GSHordeGoblin.cpp` - call sites.

`SlotName()` no longer switches over cases - it returns the tag's leaf, so nothing has to be kept in
step with a list.

**Build:** editor closed, `-IgnoreQueue` (this was the only open ticket and was the one needing the
build). **Succeeded in 2:13**, only the two pre-existing `C4996 AbilityTags` warnings.

## Evaluate (implementation)

**The predicted CDO loss happened exactly as predicted, and was repaired.** After the build every
adversary read `WeaponSlot.Sword` - including `BP_ErikaArcher`, whose `Bow` did not survive, because
an enum-to-struct change is a type change and CoreRedirects cannot carry it. Five inherited the new
constructor default correctly. Erika was re-authored by hand and verified.

**Runtime, PIE on `L_CombatArena`. The wheel maths is unchanged, checked at every boundary:**

```
up    -> WeaponSlot.Torch     44.0 deg -> Bow       224.0 -> Sword
left  -> WeaponSlot.Sword     46.0 deg -> Torch     226.0 -> Grapple
down  -> WeaponSlot.Grapple  134.0 deg -> Torch     314.0 -> Grapple
right -> WeaponSlot.Bow      136.0 deg -> Sword     316.0 -> Bow
deadzone -> falls back to the current slot
```

Every one matches the old `>=45 Torch / >=135 Sword / >=225 Grapple / else Bow`. **A player who has
learned this wheel does not have to relearn it** - which was the risk worth testing, because a
silently mirrored or rotated wheel reads as "the sectors feel wrong" rather than as an error.

`SetSlot(Bow)` returned true and `IsInRangedMode()` flipped to true. Sixteen NPCs came up correct:
four castle guards and ten goblins on `WeaponSlot.Sword` not ranged, **both Erikas on
`WeaponSlot.Bow` and ranged**.

**Not established:** that no Blueprint anywhere binds the two changed delegate signatures. The plan
flagged this as the check to do first and the compile is the check - the build passed clean, and a
Blueprint bound to a changed signature would have failed to compile. But the build compiles C++, not
every Blueprint graph in the project; a stale binding would surface on opening that asset. Nothing
suggests one exists (`WBP_WeaponWheel` has no graph at all), but it has not been swept.

**Not done:** `IsInRangedMode()` is still `CurrentSlot == WeaponSlot.Bow`. The plan suggested it
should become a property of the slot's weapon. That is a behavioural change with callers throughout
ranged combat and it does not belong in a type swap.
