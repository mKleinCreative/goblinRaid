---
id: 054
title: Stamina to C++, swimming and drowning, climb refinements (build 1: foundation)
agent: claude-stamina
status: done
claimed: 2026-08-07T00:17Z
build: required
waiting_on:
evaluated: 2026-08-07T00:41:54Z
files: 
  - Source/GoblinSiege/Characters/GSStaminaComponent.h
  - Source/GoblinSiege/Characters/GSStaminaComponent.cpp
  - Source/GoblinSiege/Characters/GSPlayerCharacter.h
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - Source/GoblinSiege/Characters/GSCharacterBase.h
  - Source/GoblinSiege/Characters/GSCharacterBase.cpp
  - Source/GoblinSiege/World/GSWaterVolume.h
  - Source/GoblinSiege/World/GSWaterVolume.cpp
  - Source/GoblinSiege/Interaction/GSCarryComponent.h
  - Source/GoblinSiege/Interaction/GSCarryComponent.cpp
  - Source/GoblinSiege/Raid/GSRaidLibrary.h
  - Source/GoblinSiege/Raid/GSRaidLibrary.cpp
  - Source/GoblinSiege/Raid/GSRunicSite.h
  - Source/GoblinSiege/Raid/GSRunicSite.cpp
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.h
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.cpp
---

## Goal

Stamina to C++, swimming and drowning, climb refinements (build 1: foundation)

## Generate

Build 1 of the approved stamina/climb/swim plan: the C++ foundation. **Nothing here changes what a
player experiences** - the component exists and nothing calls it. Deliberate, so the Blueprint
migration lands against a compiled, verified backend.

**`Characters/GSStaminaComponent.h/.cpp` (new)** - the pool, replacing a float that lived on
`BP_GSPlayerCharacter` where only Blueprint could read it.
- `TryConsume(Cost)` is **all-or-nothing**: a vault that takes your last 3 stamina and then fails to
  clear the wall is worse than a vault that refuses.
- `SetDrainRate(PerSecond)` is a RATE, not a per-tick subtraction. That is what makes the GDD's
  "idle hold drains zero" fall out for free - hanging calls `SetDrainRate(0)` through the same path.
  Last writer wins; sprint/climb/swim are mutually exclusive, so stacking would only mask a bug.
- Exhaustion is a **latch** clearing at `RecoverFraction` (0.25), preserving the BP behaviour that
  stops sprint-stutter at 1 stamina. `TryConsume` refuses while exhausted, so the latch governs
  instant costs too, not only sustained drains.
- Replicated; `OnRep` broadcasts the value but NOT the transitions, because `OnExhausted` drives
  gameplay decisions that belong to the server.

**NOT a GAS attribute**, deliberately. Health/Armor/MoveSpeedMultiplier are attributes because OTHER
actors modify them through GameplayEffects. Stamina is spent by its own owner every frame from input;
routing that through the effect pipeline buys the mitigation machinery nothing and pays per-tick
GameplayEffect churn.

**`World/GSWaterVolume.h/.cpp` (new)** - `APhysicsVolume` with `bWaterVolume = true`. That single
flag is the whole swimming mechanism: `UCharacterMovementComponent` switches to `MOVE_Swimming` with
no custom movement mode and no network-prediction work. A subclass rather than a bare volume so the
swim tuning lives with the thing it describes, and so it is greppable - this project has twice been
bitten by settings that existed only inside a `.umap`.

**`AGSPlayerCharacter`** - adds `StaminaComponent`; `NavAgentProps.bCanSwim = true` (the half that is
easy to miss - without it the movement component refuses the mode and the pawn falls through water as
though it were air); `MaxSwimSpeed 300` (slower than walking, per the GDD); `Buoyancy 1`;
`OutofWaterZ 420` so a goblin at the bank can climb out. `HandleStaminaExhausted` kills **only while
`IsSwimming()`** - the GDD's failure state differs by medium, and this is the fork. `Drown()`
destroys cargo first, then dies.

**`AGSCharacterBase::KillOutright()`** - extracted from `DebugKill`, so drowning reuses the one death
path rather than inventing a second. Zeroes Health rather than applying damage: no instigator, no
damage type, and routing it through damage would let a raised shield survive a drowning.

**`UGSCarryComponent::DestroyCarried()`** - `PutDown()` then destroy what it returns. Going straight
to `Destroy()` would leave the attach, the move-speed effect and the replicated `CarriedActor`
pointing at a dead actor.

**`UGSRaidLibrary::FindStandableSpotNear()`** - the runic site's private ring-fan search, extracted so
a drowning can wash the player ashore. Keeps the capsule FIT test, which is the half the first spawn
fix lacked in #009 and the only check that rejects a point inside a building.

**`UGSPlayerHUDWidget`** - `StaminaBar`/`StaminaText` as `BindWidgetOptional` driven by
`OnStaminaChanged`, replacing a per-widget-tick cast to `BP_GSPlayerCharacter_C`.

## Evaluate

**NOT COMPILED.** The gate was closed by #052 for this ticket's entire lifetime, so not one line has
been through a compiler. Everything below is reading, not evidence.

**This ticket changes nothing a player can do, by design - and that is also its main risk.** The
value is realised entirely by the Blueprint migration that follows, which the plan calls the whole
risk of the feature. A foundation nobody builds on is worse than no foundation.

**Specific things I cannot claim:**
- That `MOVE_Swimming` works at all. No volume is placed and `Plane2` still blocks, so swimming is
  unreachable in game even once this compiles.
- That `OutofWaterZ = 420` gets a goblin out at the bank. The engine default of 0 is certainly wrong;
  420 is a guess against a 96 capsule half-height.
- That `MaxSwimSpeed = 300` reads correctly against `BaseWalkSpeed` 600.
- That `RecoverFraction = 0.25` matches the old latch. The BP cleared "when the pool refills" and the
  threshold is compiled into the CDO where I could not read it - 0.25 is my reading of intent, not a
  measured match. **If exhaustion feels different after this lands, this is the number.**

**An inconsistency I introduced and left.** `FindStandableSpotNear` restates `AGSRunicSite`'s spawn
tuning as file-scope constants instead of sharing it, because the site's values are `EditAnywhere` on
a placed actor and a static helper has no actor to read. Two copies that can drift is a smell; I took
a visible restatement over a silent shared default. If they must ever agree it should be a shared
struct, not nobody noticing.

**Nothing calls `FindStandableSpotNear` yet.** The drown respawn still lands at the runic site,
because `AGSGameMode::ChoosePlayerStart_Implementation` returns it unconditionally; wiring the
override through the existing `DebugSpawnOverride` seam is the next ticket. **This is precisely the
dead-helper shape AGENT_STATE warns about**, and it is only acceptable because the reader is the
immediately following step rather than a someday.

**Owed AGENT_STATE.md** - after it compiles and runs, not before: that `NavAgentProps.bCanSwim` is
required alongside `bWaterVolume`, and that missing it looks exactly like the volume not existing.

**Touched outside the goal:** none. All sixteen files were claimed before editing.

## Refine

- **Made drain a rate rather than a per-tick cost.** The obvious port of the Blueprint was
  `Stamina -= Cost` each tick. Modelling it as a rate is what lets "idle hold drains zero" be
  `SetDrainRate(0)` instead of an "are we moving" branch in every caller - the GDD rule falls out of
  the shape rather than being special-cased.
- **Made `TryConsume` refuse while exhausted.** My first version gated only sustained drains, which
  would let a player at zero still vault - the meter visibly not mattering at the moment it should
  matter most.
- **Extracted `KillOutright` rather than calling `DebugKill` from the drown path.** Drowning is not a
  debug command, and a shipped death path named `DebugKill` is the kind of thing that survives to
  release.
- **`DestroyCarried` goes through `PutDown` first.** Destroying directly would leave the carrier's
  own state pointing at a corpse and rely on downstream null checks to hide it.
- **Put the component on `AGSPlayerCharacter`, not `AGSCharacterBase`.** Only the player spends
  stamina; a ticking component on every militiaman is per-frame work for a number nothing reads.
- **Left `RegenDelaySeconds` at 0.** It is the dial that closes the "sprint, tap the brakes, sprint
  again" exploit, and turning it is a balance decision. Shipping it non-zero would have changed feel
  under cover of a refactor.

**Deliberately left undone:** the entire Blueprint migration; placing the water volume and
un-blocking `Plane2`; the climb jump; wiring the drown respawn to `FindStandableSpotNear`; and the
AnimBP, which has no swim states - the goblin will look wrong in water until animation follows.
