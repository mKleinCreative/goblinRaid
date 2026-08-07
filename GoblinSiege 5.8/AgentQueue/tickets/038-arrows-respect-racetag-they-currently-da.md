---
id: 038
title: Arrows respect RaceTag (they currently damage allied goblins); document why melee AttackCooldownSeconds must stay unwired
agent: claude-fixes
status: done
claimed: 2026-08-06T18:19Z
build: required
waiting_on:
evaluated: 2026-08-06T18:21:53Z
files: 
  - Source/GoblinSiege/Weapons/GSArrowProjectile.cpp
  - Source/GoblinSiege/Weapons/GSWeaponDataAsset.h
  - decision-queue-kanban.html
---

## Goal

Arrows respect RaceTag (they currently damage allied goblins); document why melee AttackCooldownSeconds must stay unwired

## Generate

Two items off the NEXT unranked list. One turned out to be a fix; the other turned out to be a
reason NOT to fix.

**1. Arrows ignored `RaceTag` - `GSArrowProjectile.cpp`.** `GSCharacterBase::IsHostileTo` has existed
since the friendly-fire pass and `UGSGA_SwordLight` calls it at line 271 ("a patrol of militia
crowding a doorway cuts itself down - two swings kills at 30 HP", observed 2026-08-04). Ranged was
never brought in line: zero occurrences of `RaceTag` in the arrow. The hit path now computes
`bMayDamage = !ShooterChar || ShooterChar->IsHostileTo(OtherActor)` and adds it to the condition that
guards the effect application.

`IsHostileTo` returns **true** for anything that is not an `AGSCharacterBase` - walls, barrels,
burnables - and true when either race is unset, so this narrows nothing except character-on-character
friendly fire. That "no opinion must not make a pawn immune" rule is the base class's, not mine.

**2. Melee `AttackCooldownSeconds` - investigated and deliberately NOT wired.** It looked like the
exact twin of the bow bug #034 fixed: a designer-facing field with a sensible default and no reader
anywhere. It is not. `UGSGA_SwordLight` is `InstancedPerActor` and owns a combo stage machine, so GAS
refuses re-activation while a swing is running - melee already has a rate limit, and what a player
feels is the stage timings and `MontagePlayRate`. A cooldown bolted on top would fight the combo
rather than tune it. Documented in place, with a pointer to where melee pacing actually lives and to
`GSRaceDataAsset::AttackCooldownSeconds`, which AI melee genuinely does read.

Board updated: the friendly-fire card became a design question about pass-through, the melee card
became a keep-or-delete call for Michael.

## Evaluate

**NOT COMPILED, NOT RUN at the time of writing** - the gate is closed by this ticket.

**The fix is half a fix, and the missing half is the half that matters in a fight.** An arrow no
longer damages an ally, but it still stops dead and sticks in one: `bHasHit`,
`StopMovementImmediately` and `SetActorEnableCollision(false)` all run before the damage block is
reached. So once the horde exists - `AGSHordeGoblin` sets `Race_Goblin` in its constructor - firing
past your own line means every shot is eaten by a friendly instead of killing one. **That is
arguably worse than the bug I fixed**, because it is silent: no damage number, no death, just an
arrow that went nowhere.

I did not fix it because it is not a damage change. Passing through needs
`CollisionSphere->IgnoreActorWhenMoving(Ally, true)` at a moment when the ally is not yet known, and
by the time `OnProjectileHit` fires the projectile movement component has already stopped simulating.
The options - ignore the Pawn channel wholesale, re-activate movement after a friendly hit, or a
custom collision channel per race - are all design decisions with different costs, and guessing at
one is how the earlier `MaxSimSeconds` change broke the decal hit test. Raised on the board as a
decision rather than silently picked.

**Verified only by reading.** `IsHostileTo` is `public` on `AGSCharacterBase` (line 84) and the
include was added. `Shooter` is `GetInstigator()` falling back to `GetOwner()`, both of which can be
null - hence `!ShooterChar ||`, which preserves today's behaviour (damage everything) for an arrow
with no character shooter rather than silently making such arrows harmless.

**Written but never run:** the friendly-fire branch. No PIE, and there is no allied archer in the
level to test it with today - `BP_ErikaArcher` exists but nothing gives it a bow.

**Item 2 produced no code change at all**, which is the correct outcome but means there is nothing to
verify. The claim "melee is already rate-limited" rests on reading `InstancingPolicy` and the stage
machine, **not** on trying to spam a sword in PIE. That test is worth thirty seconds next time the
editor is open, and I have not done it.

**Owed AGENT_STATE.md** - nothing new. The unread-UPROPERTY FAILED entry written in #037 already
covers this class of bug, and item 2 is a qualification of it rather than a new lesson: *a field with
no reader is not automatically a missing feature - check whether the behaviour is already provided
somewhere better first.* Added to that entry's spirit rather than duplicating it.

**Touched outside the goal:** none; all three files were claimed.

## Refine

- **Changed an early `return` into a `bMayDamage` flag.** My first version returned from inside the
  `HasAuthority()` block on a friendly hit - which also skipped `SetLifeSpan(5.f)` and the
  `AttachToComponent` below it, so friendly-struck arrows would have kept the full 8s flight lifespan
  and hung unattached in the air. A guard placed for one reason quietly changing two others.
- **Investigated melee before wiring it, and the investigation is the deliverable.** The board card
  I wrote earlier today said to check whether the montage already rate-limits melee first; doing that
  check turned a planned fix into a documented non-fix. Wiring it would have produced a plausible
  diff that made the combo worse, and nothing in review would have flagged it.
- **Left the dead field in place rather than deleting it.** Deleting a `BlueprintReadOnly` field
  from a data asset touches every asset that serialises it and is a designer's call. Documenting why
  it does nothing costs nothing and removes the trap for the next person who greps it.
- **Wrote the pass-through gap into the board as a question, not a TODO in a comment.** A TODO in
  `GSArrowProjectile.cpp` is exactly the kind of finding this session has twice found rotting in a
  closed ticket that nothing reads at run start.

**Deliberately left undone:** arrow pass-through (needs Michael's call); deleting
`AttackCooldownSeconds`; the thirty-second PIE test that would turn "melee is already gated" from a
reading into an observation.
