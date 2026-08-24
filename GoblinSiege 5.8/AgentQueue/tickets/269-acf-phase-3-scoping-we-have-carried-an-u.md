---
id: 269
title: ACF Phase 3 scoping: we have carried an unused UACFEquipmentComponent since Phase 2a while UGSWeaponComponent does the same job
agent: claude-warren
status: review
claimed: 2026-08-24T00:05Z
build: none
waiting_on:
evaluated: 2026-08-24T00:20:34Z
observed:
scenario:
files: 
  - AgentQueue/tickets/SCOPING-ONLY
---

## Goal

ACF Phase 3 scoping: we have carried an unused UACFEquipmentComponent since Phase 2a while UGSWeaponComponent does the same job

## Generate

**Scoping only. No code, no assets, nothing edited.**

### Stage 1 result: the component is there, live, and empty

`AACFCharacter` creates a `UACFEquipmentComponent` at construction (`ACFCharacter.cpp:74`) and every
GS character has carried one since Phase 2a reparented `AGSCharacterBase` onto it. **Our source never
references it** - zero mentions of `EquipmentComp` or `GetEquipmentComponent` anywhere in
`Source/GoblinSiege`.

It is NOT inert. `AACFCharacter` actively:

- destroys equipped items on death (`:124`)
- calls `InitializeStartingItems()` when uninitialised (`:213-214`)
- binds `OnEquipmentChanged` (`:221-222`)
- **reads `GetCurrentDesiredMovesetActionTag()` and `GetCurrentDesiredOverlayTag()` off it to drive
  animation state** (`:248-249`)

Read off the live CDOs of `BP_GSPlayerCharacter` and `BP_HordeGoblin`, both identical:

| property | value |
|---|---|
| `AvailableEquipmentSlot` | **0** - nothing can be equipped |
| `AllowedWeaponTypes` | **0** |
| `StartingItems` | **0** |
| `MaxInventoryWeight` | 180 (ACF default, never set by us) |

So ACF has been consulting an empty equipment component for moveset and overlay tags on every
character in the project, while `UGSWeaponComponent` did the real work beside it.

### What overlaps, and what does not

| `UGSWeaponComponent` responsibility | ACF equivalent |
|---|---|
| `EquipWeapon`, mesh + socket attach | `UACFEquipmentComponent` + `AACFWeaponActor` - **direct overlap** |
| `SetSlot` / `EGSWeaponSlot` **enum** | ACF slots are **`FGameplayTag`** - a type change, not a rename |
| Weapon wheel (open, drag, highlight, commit) | **none** - ours to keep |
| Torch readied / horn raised props | ACF would model these as items in slots |
| Blood orbs | **none** - ours to keep |

Roughly half the component duplicates ACF; the other half has no ACF counterpart. This is not a swap.

### BACKLOG (Michael, 2026-08-23): the `Locomotion State inexistent` warnings

`AACFCharacter` reads moveset and overlay tags off the equipment component, and ours returns nothing
because it has no slots and no items. That is a plausible cause of the repeated
`LogTemp: Warning: Locomotion State inexistent` in the logs - **and it is a hypothesis from reading
call sites, not a verified finding.** Explicitly not claimed as diagnosed: earlier the same day this
agent read a log warning, declared a regression, and was wrong. Testable once Stage 2 puts one real
item in one real slot.

### Reference content

`/Game/FullSample/` is still absent - the asset registry reports **zero** `UACFWeapon` and zero
`UACFArmor` assets project-wide, and there is no `Content/FullSample` on disk. Michael has the
archive; CLAUDE.md's note that it comes from the ACF Discord can be updated with the actual location
once it lands. Until it does, Stage 2 authors its first weapon DataAsset with no reference
implementation to copy.

## Evaluate

Nothing was built, so there is nothing to compile and nothing to watch. The findings are read
directly off ACF source at file:line and off the live CDOs, not inferred.

**The one thing NOT established:** whether `UACFEquipmentComponent` actually functions once given
slots and items in this install. Everything above says it is wired; none of it says it works. That is
exactly what Stage 2 exists to find out, on one weapon, changing nothing else.

## Refine

**Staged deliberately, and the staging is the point.** Stage 3 - moving `EGSWeaponSlot` from an enum
to `FGameplayTag` - reaches the weapon wheel, the HUD widget, input routing and the abilities. That
is a wide mechanical change with no way back if the ACF seam turns out not to fit. It stays behind a
working axe equipped through ACF on one character.

> 2026-08-24T00:20Z Stage 1 complete: ACF equipment present on every character since Phase 2a, entirely unconfigured. Stage 2 blocked on FullSample reference content.
