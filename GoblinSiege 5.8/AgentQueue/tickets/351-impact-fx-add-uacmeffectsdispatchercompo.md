---
id: 351
title: Impact FX: add UACMEffectsDispatcherComponent to AGSGameState and author an impacts FX data asset so PlayImpactEffect has something to play through
agent: claude-combat
status: done
claimed: 2026-08-29T04:41Z
build: none
waiting_on:
evaluated: 2026-08-29T05:18:57Z
observed: 2026-08-29T05:18:13Z | Confirmed the dispatcher is live on the running GameState: GSEffectsDispatcherComponent found by FindComponentByClass, and the 'Missing Effects Dispatcher Component in GAME STATE!' error that fired on every melee hit is completely absent from the log after a swing that otherwise logged normally (bone='RightLeg', damage 25.0). PlayImpactEffect now has a dispatcher and an FX table to play through.
scenario: PIE on L_CombatArena, player swing driven from Python against a guard placed 120uu ahead with GS.Combat.Debug 1; log checked for the dispatcher error before and after the build.
files: 
  - Source/GoblinSiege/Core/GSGameState.h
  - Source/GoblinSiege/Core/GSGameState.cpp
  - Content/Data/Effects/DA_GS_ImpactFX.uasset
---

## Goal

Impact FX: add UACMEffectsDispatcherComponent to AGSGameState and author an impacts FX data asset so PlayImpactEffect has something to play through

## Generate

#349 gave melee a real contact point, normal, bone and physical material - and every
`PlayImpactEffect` call still played nothing, logging
`Missing Effects Dispatcher Component in GAME STATE!` on each hit.
`UACMCollisionsFunctionLibrary::GetEffectDispatcher` (`ACMCollisionsFunctionLibrary.cpp:15-29`) does
`GameState->FindComponentByClass<UACMEffectsDispatcherComponent>()` and gives up when there is none.
The data was right; there was no consumer.

**`UGSEffectsDispatcherComponent`** (`Core/GSGameState.h/.cpp`) - a three-line subclass of ACF's
dispatcher whose constructor assigns the FX table. **Why a subclass:**
`UACMEffectsDispatcherComponent::ImpactFXs` is `EditDefaultsOnly` and **protected** with no setter,
and there is no GameState Blueprint in this project to author it as data. A subclass reaches a
protected member with ordinary C++; the alternative was a reflection lookup by property name that
would break silently the day ACF renames the field. ACF finds the dispatcher by class, and that
matches a subclass, so nothing on ACF's side needs to know.

**`DA_GS_ImpactFX`** (`Content/Data/Effects/`) - `UACMImpactsFXDataAsset` mapping
`UGSDamageType_Axe` and `UGSDamageType_Bow` to one `FMaterialImpactFX` with **no physical material
set**, which ACF treats as the default for every unlisted surface. So a hit on anything makes a
sound immediately, and per-material entries can be layered on later without touching code. Sound is
`SC_Sword_Hit_1-1_Cue` from the bundle already in the project.

The component is created in `AGSGameState`'s constructor beside the team manager, following the same
pattern: hardcoded asset path with a LOUD failure, so a renamed asset says so at startup rather than
degrading quietly into a combat system with no impacts.

## Evaluate

**Observed:** `GSEffectsDispatcherComponent` present on the live GameState via
`FindComponentByClass`, and the `Missing Effects Dispatcher Component` error - which fired on every
hit before - is entirely absent after a swing that otherwise logged normally (`bone='RightLeg'`,
damage 25.0). That error is unambiguous and machine-checkable, so its disappearance is real evidence
rather than an inference.

**NOT observed: that anything is audible.** I can prove the dispatcher exists, that it is found, and
that the call reaches it with real hit data. Whether a sound actually plays needs a human with
speakers. Stated plainly because "the error stopped" and "I heard it" are different claims and only
one of them is mine to make.

**A build error worth recording:** the first version called `SetImpactsFX`, a setter that does not
exist. The compiler caught it. That is the second time this session a plausible-looking ACF API was
invented rather than verified - the lesson is to grep the header before writing the call, not after.

**AGENT_STATE.md owes:** DECISION - *impact FX dispatch through a GS subclass of ACF's effects
dispatcher on the GameState, with the FX table assigned in C++ because ACF's field is protected.*

## Refine

**Changed in response to my own evaluation:** the assignment moved from a non-existent setter on the
GameState into a dispatcher subclass, once the compiler proved the setter was imaginary and the
header showed the field was protected.

**Deliberately left undone:**

1. **No particle.** `NiagaraParticle` is unset on the FX entry - impacts are audible, not visible.
   Deliberate scope: one honest impact sound beats none, and the visual half wants the same
   treatment once the audio is judged.
2. **One sound for every surface and every weapon.** The entry has no physical material, which is
   what makes it a universal default. Per-material entries (stone, wood, flesh) are pure data on top.
3. **The damage tag is still hardcoded.** `GSGA_SwordLight.cpp` writes `Damage_Dagger` regardless of
   the weapon's `DamageTypeTag`, and passes `UGSDamageType_Axe` to `PlayImpactEffect` - so every
   impact currently requests the axe entry whatever the weapon. Fixing the tag fixes the FX
   selection for free.
