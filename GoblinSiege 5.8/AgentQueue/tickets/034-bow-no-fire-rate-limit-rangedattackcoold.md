---
id: 034
title: Bow: no fire-rate limit (RangedAttackCooldownSeconds is dead), and the arrow is invisible in flight
agent: claude-ranged2
status: done
claimed: 2026-08-06T17:43Z
build: required
waiting_on:
evaluated: 2026-08-06T17:49Z
files: 
  - Source/GoblinSiege/Weapons/Abilities/GSGA_BowShot.h
  - Source/GoblinSiege/Weapons/Abilities/GSGA_BowShot.cpp
  - Source/GoblinSiege/Weapons/GSArrowProjectile.h
  - Source/GoblinSiege/Weapons/GSArrowProjectile.cpp
---

## Goal

Bow: no fire-rate limit (RangedAttackCooldownSeconds is dead), and the arrow is invisible in flight

## Generate

Michael's report from the first PIE session that ever ran the ranged work: "I can shoot arrows as
fast as I can click right now, which is not ok, and there's no arrow mesh being shown to see where
the arrow went."

**1. The bow had no rate of fire.** `UGSGA_BowShot` never assigned `CooldownGameplayEffectClass`,
and its own header said cooldown was "editor-authored ... data, not code" - but no such asset was
ever authored, so `CommitAbility` applied nothing and every click fired. Meanwhile
`UGSWeaponDataAsset::RangedAttackCooldownSeconds` has existed since the data asset was written,
defaulted to 1.5s, carrying the comment *"ranged trades damage-per-second for range/safety"* - with
**zero readers anywhere in the project**. (`AttackCooldownSeconds`, its melee twin, is also unread;
see "outside the goal".)

`GSGA_BowShot.h/.cpp` now:
- override `CanActivateAbility` to refuse while `World->GetTimeSeconds() - LastFireTimeSeconds` is
  under the interval. In `CanActivateAbility`, not `ActivateAbility`, so a too-early click costs no
  commit, no instance and no wind-up task started-then-cancelled.
- `GetFireIntervalSeconds()` reads the equipped weapon's `RangedAttackCooldownSeconds` via
  `FindComponentByClass<UGSWeaponComponent>` (not a cast to `AGSPlayerCharacter` - an AI archer
  should obey the same limit, matching `FireArrow`'s existing reasoning), falling back to a new
  `FallbackFireIntervalSeconds = 1.5f` when there is no weapon to ask.
- `<= 0` in the data asset is treated as *not configured*, not *no limit*. Unlimited fire is the bug
  being fixed; a blank field must not be able to request it by accident.
- `LastFireTimeSeconds` stamped in `ActivateAbility` after commit, so the interval measures
  shot-to-shot rather than arrow-to-press (otherwise `ReleaseDelaySeconds` is silently added to every
  gap and the number in the data asset is not the number the player feels).

**2. The arrow was invisible.** `ArrowMesh` was a `TSoftObjectPtr` that nothing ever assigned, while
`/Game/_Import/Weapons/GS_Arrow` sat unreferenced in the same import folder as `GS_Bow_Only`,
`GS_Quiver`, `GS_Sword` and `GS_Torch`. `GSArrowProjectile.h/.cpp` now C++-defaults it in the
constructor, and applies a new `ArrowMeshOffset` transform after the mesh resolves.

**3. The warning that should have found this could not fire.** The guard was
`if (!ArrowMesh.IsNull() && !bArrowMeshResolveFailed && ...)`, so an **unset** reference skipped the
block entirely - and unset was the shipping state. Now the empty case is reported as loudly as a
broken one, naming which of the two it is.

**4. Third twin of the #030 latch bug.** `bArrowMeshResolveFailed` was a member on an actor spawned
fresh per shot, so it started false on every arrow and could never suppress a repeat - identical to
the two latches #030 made file-scope on `AGSTorchProjectile`. Now `GArrowMeshResolveFailed`.

## Evaluate

**NOT COMPILED, NOT RUN.** Written with the editor open and the gate closed by this very ticket.
Everything below is reasoning and grep, not evidence, and the next section of this file is the only
place that will be true until a build says otherwise.

**The failure this really exposes is not the bow.** Two designer-facing tuning fields sat on
`GSWeaponDataAsset` with sensible defaults and considered comments and **nothing ever read either
one**. That is invisible from the data side - `DA_Weapon_Scout` looks correctly configured, because
it is; the value simply goes nowhere. Nothing in the project would have flagged it, and the way it
surfaced was a human playing the game and saying it felt wrong. `AttackCooldownSeconds` (melee) is
still dead right now.

**Unverified and load-bearing:**
- **`GS_Arrow`'s asset class.** I could not confirm from disk that it is a `UStaticMesh` and not a
  `USkeletalMesh` - the byte scan showed the same import table on every asset in the folder. Its four
  neighbours are all static meshes referenced as `TSoftObjectPtr<UStaticMesh>` by
  `DA_Weapon_Scout`, which is strong but not proof. If it is skeletal, `LoadSynchronous()` returns
  null and the arrow stays invisible - but it will now SAY so in the log, which is the whole point of
  fix 3.
- **The arrow's orientation in flight.** `bRotationFollowsVelocity` aligns the ACTOR's +X with the
  path; whether the mesh's long axis agrees is a property of the FBX. `ArrowMeshOffset` defaults to
  identity and I have not seen an arrow fly.
- **That 1.5s is the right feel.** It is the pre-existing default, not a judgement.
- **`CanActivateAbility` and held-button autofire.** Refusing activation is the correct shape for a
  re-trying input, but the input path has not been read end to end.

**`ArrowMeshOffset` is barely editable and I should say so plainly.** It is `EditDefaultsOnly` on a
C++ class that is also the class actually spawned (`ArrowProjectileClass` C++-defaults to
`AGSArrowProjectile::StaticClass()`), so with no Blueprint subclass in existence there is no CDO to
edit. Today it is a rebuild-to-change knob wearing an editable property's clothes. It costs nothing
and becomes real the moment a `BP_GS_Arrow` exists.

**Owed AGENT_STATE.md** - FAILED: *a UPROPERTY with a sensible default and no reader is invisible
from both sides.* The data looks configured and the code looks complete; only playing it reveals the
value goes nowhere. Grep every tuning field on a data asset for at least one reader.

**Touched outside the goal:** nothing. `AttackCooldownSeconds` (melee) is dead in the same way and I
did not fix it - out of scope, unasked for, and it needs someone to check whether melee is already
rate-limited by its montage before adding a second gate on top.

## Refine

- **Read the data asset instead of inventing a constant.** My first instinct was
  `MinFireIntervalSeconds` on the ability. That would have been a third number describing rate of
  fire, next to two that already existed - and it would have left `RangedAttackCooldownSeconds` dead,
  so the next person to tune the bow in `DA_Weapon_Scout` would change nothing and have no way to
  learn why. The ability keeps only a fallback for the no-weapon case.
- **Rejected a cooldown GameplayEffect, and wrote down why.** A GE is the GAS-idiomatic answer and is
  correct the day the HUD draws a cooldown sweep, because a tag on the ASC is the only form UI can
  observe. It would cost a new `UGameplayEffect` class and a new `Cooldown.*` tag in the shared
  `GSGameplayTags` for a rule nothing currently watches. A timestamp is the pattern
  `AGSCharacterBase::HitReactCooldownSeconds` and `UBTTask_MeleeAttack` already use, so this is
  consistent rather than novel. The header records the trigger for revisiting it.
- **Fixed the third latch twin rather than only the two bugs asked for**, on #030's own reasoning:
  an identical bug left beside a fixed one guarantees someone re-reports it.
- **Widened the mesh warning instead of only assigning the mesh.** Assigning `GS_Arrow` fixes today's
  invisible arrow; the silent-on-unset guard is what let it ship invisible in the first place and
  would hide the next one just as well.

**Deliberately left undone:** the melee `AttackCooldownSeconds`; a `BP_GS_Arrow` subclass to make
the offset real (premature until an arrow is seen flying wrong); tuning 1.5s.
