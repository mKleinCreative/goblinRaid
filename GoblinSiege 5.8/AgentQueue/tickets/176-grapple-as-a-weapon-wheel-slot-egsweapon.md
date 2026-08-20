---
id: 176
title: Grapple as a weapon-wheel slot: EGSWeaponSlot::Grapple, 4 sectors, UGSGA_GrappleThrow (WRITTEN, gate closed - not compiled)
agent: claude-grapple
status: done
claimed: 2026-08-18T00:39Z
build: required
waiting_on:
evaluated: 2026-08-18T00:45:13Z
observed: 2026-08-18T01:37:41Z | Michael selected the Grapple slot from the Q weapon wheel and threw the hook - the fourth sector is reachable, the ability fires from the attack key, and a hook leaves the hand. He reported it renders red and looks weird, which is a cosmetic follow-up, not a failure of the slot or the ability
scenario: Michael playing in PIE, pressing Q, dragging to the new bottom sector, releasing, then pressing attack
files: 
  - Source/GoblinSiege/Weapons/GSWeaponComponent.h
  - Source/GoblinSiege/Weapons/GSWeaponComponent.cpp
  - Source/GoblinSiege/UI/GSWeaponWheelWidget.h
  - Source/GoblinSiege/UI/GSWeaponWheelWidget.cpp
  - Source/GoblinSiege/Characters/GSPlayerCharacter.h
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_GrappleThrow.h
  - Source/GoblinSiege/Weapons/Abilities/GSGA_GrappleThrow.cpp
---

## Goal

Grapple as a weapon-wheel slot: EGSWeaponSlot::Grapple, 4 sectors, UGSGA_GrappleThrow (WRITTEN, gate closed - not compiled)

## Generate
The grapple becomes the wheel's fourth slot. **No new input action** — the torch already established
that ATTACK reinterprets itself by slot (`GSPlayerCharacter.cpp`, "one key whose meaning follows what
is in your hand"), so the grapple copies that shape exactly.

**New:** `Weapons/Abilities/GSGA_GrappleThrow.h/.cpp` — shaped on `UGSGA_TorchToss`:
`InstancedPerActor` + `ServerInitiated`, soft-defaulted hook class, `UAbilityTask_WaitDelay` wind-up
(cancelled with the ability, unlike a world timer), muzzle from `UGSAimComponent::GetMuzzleTransform`
so the arc and the spawn cannot disagree, and teardown in the `EndAbility` override.

**Modified:**
- `Weapons/GSWeaponComponent.h` — `EGSWeaponSlot::Grapple`, **appended** (its integer values reach
  the BP CDO and saved widget bindings; inserting would repoint them, same rule `EGSHordeOrder` carries).
- `Weapons/GSWeaponComponent.cpp` — `SlotForDirection` goes from three 120-degree sectors to four
  90-degree ones: Torch top, Sword left, Grapple bottom, Bow right. Bow and Sword each shift 30
  degrees rather than the wheel rotating, so each stays on the side a player already reaches for.
  Also `SlotName` gained a Grapple case — **it would otherwise have logged Grapple as "Sword"**,
  which is #081's lying instrument.
- `UI/GSWeaponWheelWidget.h/.cpp` — `Label_Grapple` (BindWidgetOptional, so an un-re-authored WBP
  still compiles), a `LabelFor` case (its `default:` would have returned `Label_Sword`), and the
  `All[]` array in `Repaint`. That array's own comment predicted this fourth slot; keeping it in
  step is what it bought.
- `Characters/GSPlayerCharacter.h/.cpp` — `GrappleThrowAbilityClass`, C++-defaulted, granted beside
  the other abilities, and a Grapple branch at the head of `Input_AttackPressed` that suppresses the
  heavy-charge timer exactly as the torch does.

## Evaluate
**NOT COMPILED.** The build gate was CLOSED for this entire ticket (#169 #171 #172 #173 #174 #175
open). This is the same written-but-unbuilt state #141 was in; that worked out, but it is stated here
rather than left to be discovered.

Checked by hand rather than by compiler: every `EGSWeaponSlot::` site in the module was grepped, and
the only two outside the changed files (`GSEnemyCharacter.h:89`, `GSHordeGoblin.cpp:63`) both name
`Sword` explicitly and are unaffected. Both `switch` statements on the enum were found and both
needed a case — neither would have failed to compile, and both would have been quietly wrong.

**Known gaps, all deliberate:**
- **No self-block tag.** That wants `State.Grappling` in `GSGameplayTags`, which this ticket does not
  claim, so a fast double-press can throw twice. Recorded rather than reaching into an unclaimed file.
- **No held grapple prop** — `RefreshWeaponMeshPlacement` has nothing to show for the slot.
- **`WBP_WeaponWheel` needs a `Label_Grapple` text widget authored** or the slot is selectable but
  unlabelled. BindWidgetOptional means that degrades rather than breaks.
- The throw shares `AM_GS_ThrowTorch` until `AM_GS_GrappleThrow` is built, so it looks like a torch throw.

## Refine
- `GrappleThrowAbilityClass` is C++-defaulted where `BowShotAbilityClass` deliberately is not: the bow
  fails loudly into the sword, whereas an unset grapple makes the newest wheel slot do nothing at all.
  That is #048 and #088 twice over, and it is the cheapest of the four assignment failure points to remove.
- The hook class is a `TSoftClassPtr<AActor>` rather than a concrete type because the prototype hook is
  a Blueprint. When `AGSGrappleHookProjectile` lands in C++ this should be re-typed to it.
- Grapple is tested first in `Input_AttackPressed` so every branch below stays byte-identical.
