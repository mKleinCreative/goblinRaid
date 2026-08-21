---
id: 228
title: ACF Phase 2b-1b: health consumers move to ARS, death routed through ACF
agent: claude-acf
status: done
claimed: 2026-08-21T02:21Z
build: none
waiting_on:
evaluated: 2026-08-21T02:29:27Z
observed: 2026-08-21T02:49:47Z | Health lives in ARS and death runs through ACF. Five goblin deaths in one fight, each decrementing the pool EXACTLY once - active 9,8,7,6,5 with Reserve 10 unchanged - so HandleDeath fires only from OnOwnerDeath and not also from the health delegate, which was the double-trigger risk. Corpses ragdoll. GS.Stats.Dump reads 17 characters with 0 MaxHealth mismatches, each archetype on its own DT_GSAttributeInits row. Michael watched it and reports one cosmetic issue: corpses pop up before falling.
scenario: L_CombatArena, player-driven fight against the arena defenders with a summoned horde, 2026-08-21 02:45.
files: 
  - Source/GoblinSiege/Attributes/GSAttributeSetBase.cpp
  - Source/GoblinSiege/Attributes/GSAttributeSetBase.h
  - Source/GoblinSiege/Characters/GSCharacterBase.cpp
  - Source/GoblinSiege/Characters/GSCharacterBase.h
  - Source/GoblinSiege/Missions/GSObjective_KillLandlord.cpp
  - Source/GoblinSiege/Combat/GSDebugCommands.cpp
---

## Goal

ACF Phase 2b-1b: health consumers move to ARS, death routed through ACF

## Generate

**ARS is now the source of truth for health, and ACF owns death.**

- `UGSAttributeSetBase::PostGameplayEffectExecute` drains `UACFStatisticsSet::Health` instead of its
  own. **Clamped to EXACTLY zero**: ACF's death trigger tests `Data.NewValue == 0.f`
  (`ACFGASStatisticsComponent.cpp:496`), so 0.0001 health would leave a character alive at zero HP
  with nothing in any log to explain it.
- `AGSCharacterBase::GetHealth()/GetMaxHealth()` read ARS. That one edit moved the HUD, the
  field-fire objective and the burn debug command, which all go through those accessors.
- The health-changed delegate, `KillOutright`, the health-fraction setter and
  `GSObjective_KillLandlord` all point at ARS.
- **Death:** `HandleDeath` is now a `UFUNCTION` bound to `UACFDamageHandlerComponent::OnOwnerDeath`
  and is no longer called from the health delegate - calling it both ways would decrement the horde
  pool twice and broadcast `OnDied` twice. It keeps the dead latch, `State.Dead`, `OnDied` and the
  game mode's accounting, and **sheds ragdoll, movement lock, capsule collision and corpse lifespan**
  to `AACFCharacter::HandleCharacterDeath`.
- `DeathType = EGoRagdoll` in the constructor. ACF defaults to `EDeathAction`, which forces a death
  MONTAGE we have never authored - the default would have produced a corpse that just stands there.
- `UGSAttributeSetBase::Health/MaxHealth` survive as a **mirror**, updated from ARS in
  `HandleHealthChanged`. Nine character Blueprints reference the set and a binary grep cannot say
  which touch Health specifically, so deleting the attributes tonight would have been a guess.

## Evaluate

**PARTLY WATCHED.** `GS.Stats.Dump` on `L_CombatArena` with a summoned horde: **17 characters, 0
MaxHealth mismatches**, HordeGoblin 40/40, Militia 30/30, Archer 20/20, Player 100/100. So health
reads from ARS and the mirror tracks it.

**DEATH IS NOT WATCHED, and it is the risky half.** Nothing in this ticket's evidence shows a
character dying. What must be watched, in one fight:

- A goblin dies **once** - the pool logs `Reserve N (unchanged)` exactly one time. Two decrements
  means the double-trigger this ticket was written to avoid is happening anyway.
- The corpse **ragdolls** rather than standing still (proves `EGoRagdoll` took and that ACF's death
  path is running at all).
- The corpse stops being targeted - `BTService_AcquireTarget`'s "a corpse is not a target" rule keys
  off `bIsDead`, which now gets set from ACF's delegate rather than from the health write.
- The player's HUD health bar still moves.

**Known and deliberate:** two health attributes still exist. ARS is authoritative, ours is a mirror.
Deleting the mirror is a follow-up once Blueprint usage is confirmed clear, and until then
`GS.Stats.Dump`'s mismatch column is a live check that the mirror works.

**Unrelated fix, folded in because it wasted time three times:** `EditorStartupMap` and
`GameDefaultMap` pointed at the engine's `Lvl_ThirdPerson` template, so every editor relaunch landed
in an empty map and every verification run had to load `L_CombatArena` by hand. Both now point at the
arena.

## Refine

Chose the mirror over deleting our attributes. A migration that also breaks nine Blueprints in a way
no compiler catches is not a migration anyone can review, and the mirror costs two lines in a handler
that already runs on every health change.

## Addendum - why nothing died, and it was not the switch

Michael: *"peoples legs were going through the floor and I didn't die"*.

**Root cause, measured rather than reasoned:** the CDO of `UACFGASDeveloperSettings` reported
`HealthAttribute` with `AttributeName=""` and an empty `HealthTag`.

`UACFGASStatisticsComponent::BindHealthDelegate` binds ACF's death watcher to
`UARSFunctionLibrary::GetDefaultHealthAttribute()`, which reads that setting. **An empty attribute
binds the delegate to nothing**, so health reaching zero was never noticed and nothing in the game
could die. `AACFCharacter::KillCharacter` was inert for the same family of reason - it routes through
`HealthTag`.

Proved before fixing: `KillCharacter()` was called on a live goblin from Python and the goblin stayed
at 40/40 with no death logged.

ACF ships these values in its own `Config/DefaultPlugins.ini`, but they do not reach a project's
config hierarchy. Copied into the project's `Config/DefaultPlugins.ini`; after a restart the CDO
reports `AttributeName="Health"`.

**`HealthTag` is STILL empty** and was not chased - the death chain uses the attribute, not the tag.
It matters for `KillCharacter` and `ModifyStatistic`, so ACF's own statistic API remains unusable
until it is resolved. Its intended value is `RPG.Statistic**s**.Health`, plural; a singular
`RPG.Statistic.Health` also exists in ACF's tag list and is not the one this wants.

**Still unverified, and not verifiable without a human playing:** whether damage now reaches zero and
kills. Four automated PIE runs produced **zero telegraph events** - AI does not engage without a
player driving, so "nobody died" in those runs proves nothing at all about damage. This needs one
watched fight.

**`legs going through the floor` is unexplained.** It cannot be death ragdoll, because nothing was
dying. Candidates not yet investigated: the fourteen unconfigured ACF components added in Phase 2a,
or capsule/mesh handling. Do not assume the config fix addresses it.

## Addendum 2 - the ragdoll came back, and why ACF could not keep it

After the config fix things DID start dying. Michael: *"feet are still through the floor and they are
in their idle pose for death"*.

`UACFRagdollComponent::GoRagdollFromDamage` opens with:

```cpp
if (!damageEvent.DamageClass) { return; }        // ACFRagdollComponent.cpp:92
```

The event is ACF's `LastDamageReceived`, populated **only** by
`UACFDamageHandlerComponent::TakeDamage`. Our damage does not go through ACF's pipeline, so
`DamageClass` is null, the ragdoll returns immediately - and `bDisableCapsuleOnDeath` has already
switched the capsule off, so the corpse holds its idle pose and sinks. Both reported symptoms, one
cause.

**Confirmed `DeathType` was NOT the problem:** all six Blueprint CDOs read `E_GO_RAGDOLL`, so the
constructor value took. ACF was asking for a ragdoll; its own component refused.

**Partial walk-back of the "ACF owns death entirely" ruling, stated plainly:** ACF cannot own the
ragdoll until it owns the DAMAGE, because its ragdoll reads the damage event. ACF keeps the trigger,
the movement lock, the capsule and the lifespan; **the ragdoll is ours again** until Phase 2b-2 routes
damage through `UACFDamageHandlerComponent`. At that point `LastDamageReceived` becomes real and this
block should be deleted rather than left as a second ragdoll authority.

## A correction about the logs, worth recording

Twice I read "0 `telegraph SEEN` lines" as "no combat happened" and said so. **`telegraph SEEN` only
logs while `GS.Combat.LogAI` is on**, and it is off by default every session - so its absence says
nothing whatsoever about whether a fight occurred. The same log carried a real death the whole time.
Combat activity must be judged by something that logs unconditionally.
