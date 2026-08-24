---
id: 274
title: "PLAN ONLY: ACF Phase 3 stage 3 - retire EGSWeaponSlot in favour of gameplay tags"
agent: claude-warren
status: review
claimed: 2026-08-24T02:55Z
build: none
waiting_on: "Michael: one design decision - mirror the enum as WeaponSlot.* tags (stage 3A), or go straight to modelling wheel slots as ACF equipment slots (stage 3B). See The decision."
evaluated: 2026-08-24T03:02:37Z
observed:
scenario:
files: 
  - AgentQueue/tickets/SCOPING-ONLY-274
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
