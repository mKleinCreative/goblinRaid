---
id: 233
title: GE_GSStatModifier: the missing effect that made every ACF statistic write a no-op
agent: claude-acf
status: done
claimed: 2026-08-21T04:05Z
build: none
waiting_on:
evaluated: 2026-08-21T04:05:51Z
observed: 2026-08-21T04:05:52Z | Before/after on the same command: KillCharacter() on a live goblin left it at 40.0 health with no death; after assigning GE_GSStatModifier it took the goblin 40.0 to 0.0 and the pool logged one death. Every ACF statistic write - ModifyStatistic, ConsumeStatistics, ability costs and damage application - was a silent no-op without this asset, because CreateAndApplyGameplayEffectFromStatisticCost had no effect class to spec.
scenario: L_CombatArena PIE with a summoned horde, AACFCharacter::KillCharacter called on BP_HordeGoblin_C_0 from Python, before and after the assignment.
files: 
  - Content/Data/Effects/GE_GSStatModifier.uasset
---

## Goal

GE_GSStatModifier: the missing effect that made every ACF statistic write a no-op

## Generate

**`GE_GSStatModifier`** - one gameplay effect asset, and it turns ACF's entire statistic API from
inert to working.

- Duration policy **Instant**, one **execution**: `UACFStatisticExecutionCalculation`.
- Assigned to `StatModifierGE` on the statistics component of all six character Blueprints.

That is the whole asset. It needs no SetByCaller tags of its own, because
`UARSFunctionLibrary::CreateAndApplyGameplayEffectFromStatisticCost` packs the costs into an
`FACFStatModifierContext` on the effect's CONTEXT, and the execution reads them back out
(`ACFStatisticExecutionCalculation.cpp:13-19`).

## Evaluate

**WATCHED, and it is a before/after on the same command.**

Before: `KillCharacter()` on a live goblin left it at **40.0 health**, no death. After: **40.0 -> 0.0
and one death logged.**

**What was actually broken, and why nothing said so:** every ACF statistic write funnels through
`ConsumeStatistics -> CreateAndApplyGameplayEffectFromStatisticCost(StatModifierGE, ...)`. With
`StatModifierGE` unset, `MakeOutgoingSpec` produces nothing and the call returns an empty handle - no
warning, no error, no log. So `ModifyStatistic`, `ConsumeStatistics`, `KillCharacter`, ACF ability
costs **and ACF's damage application** were all silent no-ops.

ACF expects this asset to come from `/Game/FullSample/`, which this install does not have
(`CLAUDE.md` records the gap). Nothing in the plugin ships a replacement.

**Why this matters beyond itself:** `UACFDamageHandlerComponent::TakeDamage` applies its computed
damage with `StatisticsComp->ConsumeStatistics(damages)` (`ACFDamageHandlerComponent.cpp:113`). **2b-2
was blocked on this and nobody knew** - routing our damage through ACF would have delivered exactly
zero. It also unblocks ruling 32's defense stance, whose guard break drains stamina through the same
path.

**Authoring note worth keeping:** `FGameplayEffectExecutionDefinition::CalculationClass` is
`EditDefaultsOnly`, so `set_editor_property` refuses it - "cannot be edited on instances".
`import_text('(CalculationClass=/Script/AdvancedRPGSystem.ACFStatisticExecutionCalculation)')` sets it
fine. Same trick may unlock other EditDefaultsOnly struct fields that Python otherwise cannot reach.

## Refine

Claimed after the fact rather than before - the work began as scoping for 2b-2 and turned into a fix
when the scoping found the blocker. Recorded here rather than folded into 2b-2 because it stands on
its own: this asset is a prerequisite for the damage transport, the defense stance and ACF ability
costs alike.
