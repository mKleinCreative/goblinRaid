---
id: 040
title: Weapon wheel input + ATTACK routes by slot; torch throw moves onto the montage notify
agent: claude-wheel
status: done
claimed: 2026-08-06T18:34Z
build: required
waiting_on:
evaluated: 2026-08-06T18:39:25Z
files: 
  - Source/GoblinSiege/Characters/GSPlayerCharacter.h
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_TorchToss.h
  - Source/GoblinSiege/Weapons/Abilities/GSGA_TorchToss.cpp
  - decision-queue-kanban.html
---

## Goal

Weapon wheel input + ATTACK routes by slot; torch throw moves onto the montage notify

## Generate

The half of the wheel #039 deliberately stopped short of: something now calls it, and ATTACK routes
by slot. Michael: "do the weapon wheel first, it subsumes the torch stuff."

**1. Wheel input - one new action, not two.** `WeaponWheelAction` (Q) bound Started ->
`Input_WheelOpen`, Completed AND Canceled -> `Input_WheelClose`. Canceled matters: losing focus
mid-drag otherwise leaves the wheel stuck open eating every mouse movement.

The **direction comes from the existing Look axis**. While `IsWheelOpen()`, `Input_Look` feeds
`AddWheelInput` and returns *without* touching the controller. Swallowing the camera is the point -
a wheel you aim at while the world spins under you is unusable - and it means the wheel ships with no
new 2D input asset, no cursor, and no warping. The delta is already there and already frame-scaled by
Enhanced Input.

**2. ATTACK throws what you're holding.** `Input_AttackPressed` gains a Torch branch ahead of the bow
one, suppressing the heavy-charge timer exactly as the bow does (otherwise the heavy fires a sword
swing out of a raised torch at 1.5s), then delegating to `Input_ThrowTorchStart`.
`Input_AttackReleased` gains the matching Torch branch ahead of the Bow branch, delegating to
`Input_ThrowTorchRelease`.

Both **delegate rather than duplicate**, so there is one torch aim path: the fallback when
`bTorchAimEnabled` is off, and reading the projectile class off the ability CDO, both stay in one
place. The bow branch is byte-identical to what it was.

**3. The throw finally has an animation.** `AM_GS_ThrowTorch` was authored and referenced by nothing
in C++ - the throw was a 0.25s timer and a spawn with the goblin standing perfectly still.
`UGSGA_TorchToss` now carries `ThrowMontage` (soft, C++-defaulted to
`/Game/Characters/ScoutV2/Montages/AM_GS_ThrowTorch`) plus `ThrowMontagePlayRate = 1.5`, resolved on
first throw and cached on the instance, warn-once via a file-scope latch. `PlayAnimMontage` on the
character, matching `UGSGA_SwordLight`'s stage montages rather than an AbilityTask - the wind-up task
already owns this ability's lifetime, and a second task that also wants to own it is how an ability
ends twice.

## Evaluate

**NOT COMPILED - the editor is open.** Together with #039 this is now two uncompiled tickets of
interlocking change (a type change plus its input layer), which is the largest unverified stack of
the session. It compiles or it does not; the risk is that both land at once.

**The wheel still cannot be reached without editor work.** `WeaponWheelAction` is a `TObjectPtr` that
is null until someone assigns it: an `IA_WeaponWheel` asset has to exist, be mapped to Q in
`IMC_Default`, and be set on `BP_GSPlayerCharacter`. Until then Q does nothing and **every other
input is unaffected** - that is the deliberate degradation, but it means "the wheel is done" is not
true from a player's seat yet.

**`IA_ThrowTorch` has NOT been retired**, and that is a live inconsistency rather than a tidy
leftover. The torch key still throws a torch regardless of slot, so today there are two ways to throw
with different rules: ATTACK (slot-gated) and the old key (always). Retiring it means removing the
mapping in `IMC_Default` - editor work I cannot do from here. Leaving the C++ handlers is correct
either way; they are what ATTACK delegates to.

**The montage and the spawn are coupled by a number, not an event.** The torch still leaves at
`TorchWindupSeconds` (0.25s) while the animation plays at rate 1.5 - so if the montage's release
point is not ~0.25s in, the torch will leave the hand at the wrong moment and look wrong. **I have
not opened the montage and do not know its length.** The right end state is an AnimNotify sending a
gameplay event, which survives a retime; I did not invent that contract because no asset sends the
event yet and a C++ handler waiting on one nothing fires is worse than an honest coupling. First
thing to check in PIE, and `TorchWindupSeconds` is the dial.

**Unverified, by reading only:** that `PlayAnimMontage` on the default slot does not fight
locomotion; that swallowing Look while the wheel is open does not strand the camera if the wheel is
somehow left open; that `WheelDeadZone = 40` is a sane magnitude against real mouse deltas; that the
+Y negation puts Torch at the top rather than the bottom.

**One thing I can claim with evidence:** `AGSPlayerCharacter` needed no changes in #039 and only
additive ones here, and `bRangedMode` survives only as a delegate parameter name - both confirmed by
grep across `Source/`. The compatibility shim is holding.

**Touched outside the goal:** none. All five files claimed.

**Owed AGENT_STATE.md** - nothing yet. The DECISIONS entries from #039 already cover the settled
design; there is no lesson here until it runs.

## Refine

- **Routed the wheel through the existing Look axis instead of adding a second 2D input action.**
  The obvious build was `IA_WeaponWheelDirection` with its own binding - one more asset to author,
  one more thing to leave unassigned, and it would have fought the camera unless I also suppressed
  Look. Suppressing Look was needed anyway, so the axis was already in hand.
- **Delegated to `Input_ThrowTorchStart` / `Release` rather than copying their bodies into the
  attack path.** The copy is four lines and would have quietly forked the `bTorchAimEnabled`
  fallback and the CDO projectile-class lookup - exactly the drift `GetMuzzleTransform()` was
  centralised to stop.
- **Made the montage soft and resolved-once rather than a `ConstructorHelpers` hard reference.** A
  hard reference loads the montage at module load for every session, thrown or not. The soft path is
  the shape `AGSArrowProjectile::ArrowMesh` already uses, and the resolve is cached on the instance
  so it happens once and the asset cannot be collected between throws.
- **File-scope warn latch, not a member.** `UGSGA_TorchToss` is `InstancedPerActor`, so a member
  would warn once per goblin. That is the same bug as #030/#034's projectile latches arrived at from
  the opposite direction - and the opposite answer from #037's per-building latch. Which one is right
  depends entirely on how many instances exist, so all three now say so at the declaration.
- **Bound Canceled as well as Completed.** Only binding Completed leaves the wheel open - and the
  camera dead - if focus is lost mid-drag.

**Deliberately left undone:** the build (editor is open); `IA_WeaponWheel` and its Q mapping;
retiring `IA_ThrowTorch`'s mapping; `WBP_WeaponWheel`; moving the torch spawn onto an AnimNotify;
tuning `WheelDeadZone` and `TorchWindupSeconds` against the actual montage.
