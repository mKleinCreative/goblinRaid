---
id: 407
title: ACF conversion: ApplyRespawnState must revive ACFs damage handler
agent: claude-acf-revive
status: done
claimed: 2026-09-10T00:27Z
build: none
waiting_on:
evaluated: 2026-09-10T00:33:44Z
observed: 2026-09-10T00:33:45Z | Killed a live castle guard in PIE and revived it in place: ACF IsAlive read True, then False after DebugKill, then True again after ApplyRespawnState with health restored to 30/30. Before this change the third read stayed False and the character would have been permanently invulnerable
scenario: Live PIE on L_Tutorial_Island, BP_CastleGuard01_C_1, driven through the exact kill-then-revive-in-place sequence the bug required
files: 
  - Source/GoblinSiege/Characters/GSCharacterBase.cpp
---

## Goal

Start the ACF conversion. First slice: close the half-migrated death/revive path named by the
project's own `gs-teams-damage-spawning` skill, trap 7.

## Generate

One call in `AGSCharacterBase::ApplyRespawnState`:

```cpp
if (HasAuthority())
{
    ReviveCharacter(FMath::Clamp(HealthFraction, 0.f, 1.f));
}
```

Placed first, before the existing health write and state restoration.

`AACFCharacter::ReviveCharacter` is ACF's own entry point and already does `DamageHandler->Revive()`
plus a health write, unlocking actions and `MOVE_Walking`. Deliberately NOT hand-rolling `Revive()`
here: the skill is explicit that calling it alone leaves a living character at 0 HP that can never
die again, because ACF's death test fires only on a CHANGE to exactly `0.f` and the health is
already sitting there.

## Evaluate

**The gap was real and verified before writing anything**, not taken from the skill on trust:
`grep -rn "Revive" Source/GoblinSiege` returned **zero hits**, and
`AACFCharacter::ReviveCharacter(float normalizedHealthToGrant = 1.f)` exists as a Server Reliable
BlueprintCallable.

Phase 2A migrated the death half - `AGSCharacterBase` binds its consequences to
`DamageHandler->OnOwnerDeath` and drains ACF's health attribute clamped to zero to fire it - but
`UACFDamageHandlerComponent::Revive()` is the ONLY writer of `bIsAlive` back to true, and nothing
called it.

**Verified live in PIE on a real ACF character** (`BP_CastleGuard01_C_1`), the full round trip:

```
1. alive before        : True
2. alive after DebugKill : False
3. alive after ApplyRespawnState : True
   health              : 30.0 / 30.0
```

Step 3 is the fix. Before it, `bIsAlive` stayed false forever.

**Why this was worth doing first even though nothing is visibly broken today.** It is masked:
`AGSGameMode::RespawnPlayer` leaves the corpse standing and `RestartPlayer` hands out a brand-new
pawn with a fresh `bIsAlive`. But `ApplyRespawnState` is virtual and BlueprintCallable and reads as
the project's general "bring this character back" verb. Used IN PLACE on an existing corpse - a
downed ally picked up, a healer, a debug un-kill, a boss phase reset - it produced a character alive
to every GS check and dead to every ACF one: full health, walking, `State.Dead` cleared, absent from
every `UACFGroupAIComponent` roster, and **permanently invulnerable**, because
`UACFDamageHandlerComponent::TakeDamage` early-returns while `!GetIsAlive()` and the ACF damage path
is live behind `GSUseACFDamage`. That is a bug that would have been found by a designer building
the first revive verb, in a file nobody had edited.

**Not covered:** the client half. `OnRep_IsAlive` broadcasts `OnOwnerDeath` unconditionally on any
replication of that bool (`ACFDamageHandlerComponent.cpp:228-231`), so clients run the death handler
again on a revive. Single-player is unaffected and this is not worth fixing in ACF's code until
co-op is real - recorded so it is not rediscovered.

**Built with `-IgnoreQueue`** while #406 (another agent's ACF FullSample install) was open. That
ticket claims no C++ source - `.gitignore`, `CLAUDE.md`, two skill files and `Content/FullSample` -
so nothing of theirs was compiled.

## Refine

Left for the next slices of the conversion, in the order the gap skills argue for. All six remaining
traps are documented in `.claude/skills/gs-teams-damage-spawning`; this ticket closed trap 7 only.
