---
id: 376
title: On-fire status effect: slow burn that clings until you roll
agent: claude-fire
status: done
claimed: 2026-08-30T07:40Z
build: none
waiting_on:
evaluated: 2026-08-30T10:44:33Z
observed: 2026-08-30T10:44:32Z | Michael stood in a burning field, caught fire, and rolled; unprompted he said I remove damage on a roll, confirming the burn applies damage and the dodge roll clears State.Burning and stops it
scenario: PIE, standing in an active field-fire volume until ignited, then performing a dodge roll
files: 
  - Source/GoblinSiege/Combat/GSGE_Burning.h
  - Source/GoblinSiege/Combat/GSGE_Burning.cpp
  - Source/GoblinSiege/Combat/GSGameplayTags.h
  - Source/GoblinSiege/Combat/GSGameplayTags.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_DodgeRoll.cpp
---

## Goal

On-fire status effect: slow burn that clings until you roll

## Generate

Michael's ruling: ignition trigger is standing in fire - a field ablaze or a burning building - not
a specific attack. Both already route damage through `AGSFireVolume` (field cells via
`AGSFieldFireObjective::UpdateDamageVolumes`, buildings via `GSFlammableComponent`/
`GSCrumbleComponent`), so this hooks into that one class rather than either caller.

Checked ACF's own `StatusEffectSystem` module first (loaded the skill) and ruled it out: it's built
on `AACFCharacter::TakeDamage()`/ACF's attribute system, a separate pipeline from this project's
actual damage path (`UGSDamageExecCalculation`, `Damage.*` SetByCaller tags - see
`gs-abilities-outside-acf-asc`). Built this as a GameplayEffect instead, reusing the existing
pipeline end to end.

1. **`GSTags::State_Burning`** (`GSGameplayTags.h/.cpp`) - new tag, granted only by the burning GE.
2. **`UGSGE_Burning`** (new files) - `DurationPolicy = Infinite`, `Period = 1.f`,
   `bExecutePeriodicEffectOnApplication = false`, runs `UGSDamageExecCalculation` (same exec calc
   every damage source uses) each period. Grants `State.Burning` via
   `UTargetTagsGameplayEffectComponent::SetAndApplyTargetTagChanges` - NOT the legacy
   `InheritableOwnedTagsContainer` field, which is `UE_DEPRECATED(5.3)` (checked the 5.8 engine
   header directly; this project has already been bitten once carrying a deprecated GAS member -
   `UGameplayAbility::AbilityTags` - past an upgrade, see CLAUDE.md). No independent Duration/expiry
   - per Michael, "until we roll" is the entire lifetime; the roll is the only removal path.
3. **`AGSFireVolume`** (`GSFireVolume.h/.cpp`) - new `bCanIgniteBurningStatus`,
   `BurningStatusEffectClass` (defaults `UGSGE_Burning`), `BurnStatusIgniteSeconds` (1.5s dwell
   before catching, so running through a field doesn't ignite you), `BurnStatusDamagePerTick` (1.5,
   well under the 4.0 contact `DamagePerTick` - "slow burn", not a second copy of standing in fire).
   `DamageTick` now also tracks per-actor `BurningExposureSeconds` (a `TMap<TWeakObjectPtr<AActor>,
   float>`, pruned every tick for anyone no longer overlapping - stepping out even briefly loses
   progress, it does not pause) and calls the new `ApplyBurningStatus`, which applies the GE once
   exposure crosses the threshold, gated by the same dead/invulnerable checks `ApplyFireDamageTo`
   already uses, plus a `State.Burning` check so a pawn standing across two overlapping pooled
   volumes doesn't stack a second Infinite GE instance.
4. **`UGSGA_DodgeRoll::ActivateAbility`** - one new block right after `CommitAbility` succeeds:
   `ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(GSTags::State_Burning))`. No-op
   (and no-op safely - the engine function itself early-outs non-authoritative) when nothing is
   burning, so every other roll pays nothing for this.

## Evaluate

**NOT verified - written, not built or seen.** Same build-gate block as #370/#371/#375 - nobody has
compiled this session. Checked API surfaces directly against the 5.8 engine headers rather than
guessing (`RemoveActiveEffectsWithGrantedTags`'s exact name/signature in
`AbilitySystemComponent.h`, `Period`/`bExecutePeriodicEffectOnApplication` on `UGameplayEffect`,
`UTargetTagsGameplayEffectComponent::SetAndApplyTargetTagChanges`, `FScalableFloat`'s
non-explicit float constructor), but none of it has actually compiled.

Untested judgment calls a human needs to make once this builds: does 1.5s dwell feel right for
"you have to actually stand in it" vs. annoying on a quick pass-through; does 1.5 dmg/tick read as
a real cost or as background noise; does extinguishing read clearly (no VFX/SFX added for catching
fire or putting it out - `UGSGE_Burning` has no `GameplayCueTag` equivalent wired, this is pure
mechanic with whatever ambient fire-on-character visual, if any, already exists from being near
flame - there is currently NONE specific to the burning status itself, flagging as a likely
follow-up once the mechanic itself is confirmed to feel right).

## Refine

Handing back at `review` for the same reason as #370/#371/#375 - cannot self-satisfy the observed
gate without a build. Deliberately did NOT add a burning-character VFX/SFX cue - out of scope for
"can we create the effect", and Michael didn't ask for a visual yet.

**UPDATE 2026-08-30, after an actual build:** the first build DID compile clean, but the editor
crashed on launch - fatal `NewObject with empty name can't be used to create default subobjects...
inside of UObject derived class constructor`, stack pointing straight at
`UGSGE_Burning::UGSGE_Burning()` calling `AddComponent<UTargetTagsGameplayEffectComponent>()`.
`AddComponent<>()` calls `NewObject` internally, and the engine hard-crashes if that runs while the
GE is still under construction. Checked the engine's own GEComponent-upgrade code
(`GameplayEffect.cpp`) - it only ever calls `AddComponent`/`FindOrAddComponent` from PostLoad-time
conversion paths, never from a constructor, which is the pattern this ticket should have followed
the first time instead of guessing the constructor was safe because `GSGE_MoveSpeedScalar` does its
(non-NewObject) setup there.

**Fixed:** moved the `AddComponent` call out of the constructor into an overridden
`PostInitProperties()` (Super called first). Rebuilt - compiled clean, editor launched without
crashing this time. This is the first real signal for this ticket: "does not crash the editor on
boot" is a genuinely low bar, not the same as "the mechanic works", but it is more than this ticket
had before.

**UPDATE 2026-08-30, live test in PIE:** Michael stood in a burning field, caught fire, and rolled -
watching directly, unprompted, he said "I remove damage on a roll", confirming both halves of the
mechanic in one line: the burn actually applies (there is damage to remove) and `UGSGA_DodgeRoll`'s
`RemoveActiveEffectsWithGrantedTags` call actually clears `State.Burning` and stops it. No VFX/SFX
cue exists for catching or extinguishing (noted above as an out-of-scope follow-up), so this was
read purely off the damage numbers/HUD, not a visual - flagging that distinction rather than
overclaiming a look Michael didn't actually comment on. The dwell time and tick damage tuning were
not separately called out as wrong, so leaving both at their current values (1.5s dwell, 1.5 dmg/tick)
rather than guessing a retune he didn't ask for.
