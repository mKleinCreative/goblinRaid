---
id: 235
title: ACF Phase 2b-2b: GS damage types carrying our damage tags
agent: claude-acf
status: done
claimed: 2026-08-21T17:47Z
build: none
waiting_on:
evaluated: 2026-08-21T18:18:37Z
observed: UNOBSERVED 2026-08-21T18:53:32Z - Damage types and the axe ability copies are built and the axe abilities were watched working; the damage types themselves stay inert until the transport is enabled (GS.Combat.ACFDamage, now defaulting to 0).
scenario: none - never run
files: 
  - Source/GoblinSiege/Combat/GSDamageTypes.h
  - Source/GoblinSiege/Combat/GSDamageTypes.cpp
  - Source/GoblinSiege/Combat/GSACFDamageCalculation.cpp
---

## Goal

ACF Phase 2b-2b: GS damage types carrying our damage tags

## Generate

**Damage types.** `UGSDamageType_Axe : UMeleeDamageType`, `UGSDamageType_Bow : URangedDamageType`,
`UGSDamageType_Fire : UACFDamageType`. Each carries the tag this project already uses in its
`DamageTags` container, so `UGSACFDamageCalculation` keeps asking the question it always asked
instead of maintaining a parallel vocabulary. Deriving from ACF's melee/ranged types where they fit,
so anything in ACF reasoning about melee-vs-ranged still gets the right answer.

**The bow gap from #234 is closed.** The plate check now skips armour for `Damage.Bow`,
`Damage.IgnoresArmor` and `Damage.Blast`, read off the damage class's tags rather than hardcoded
against one class.

**Axe abilities, by COPY rather than rename** (Michael: *"why don't we just keep sword for later, when
we add the weapon back in... then just copy what we have for that and name it axe"*).
`GA_GS_SwordLight` / `GA_GS_SwordHeavy` duplicated to `GA_GS_AxeLight` / `GA_GS_AxeHeavy` and wired
to the player and the horde goblin. The sword assets are untouched, the defenders keep
`GA_HU_Sword*`, and no redirectors were created.

Build succeeded.

## Evaluate

**BUILT. NOT WATCHED, and mostly not yet reachable.**

- The axe abilities ARE live - the player and goblin now activate `GA_GS_Axe*`. They are byte
  duplicates of the sword ones, so combat should be identical; that is the thing to confirm.
- The damage types and the calculator are **still inert**. Nothing assigns
  `DamageCalculatorClass`, and no damage source constructs a `UGSDamageType_*` yet. Both wait on the
  transport switch.

**Two discoveries worth carrying forward:**

1. **The player and the goblin use DIFFERENT ability slots.** The goblin uses
   `AGSCharacterBase::LightAttackAbilityClass`; the player has its own `SwordLightAbilityClass` /
   `SwordHeavyAbilityClass` on `AGSPlayerCharacter`. The base slots read `None` on the player, which
   looks broken and is not. Anything that "grants the melee ability" has to know about both.
2. **Those property names still say Sword while holding an axe.** Deliberate - renaming a
   BP-exposed C++ property risks the Blueprint overrides. When the sword returns the player needs
   both anyway, which is the honest moment to split them into `MeleeLight` / `MeleeHeavy`.

**Naming note:** `UGSDamageType_Axe` carries `Damage.Dagger`, the existing melee tag. The mesh became
an axe; the tag did not, and renaming it would touch every ability, projectile and fire volume that
references it for no behavioural gain.

## Refine

Copying rather than renaming was Michael's call and it is the better one: no CoreRedirects, nothing
to break, and the sword survives intact for the weapon that is coming back.
