---
id: 237
title: ACF Phase 2b-2c: the axe swing delivers damage through ACF
agent: claude-acf
status: done
claimed: 2026-08-21T18:23Z
build: none
waiting_on:
evaluated: 2026-08-21T18:27:36Z
observed: 2026-08-21T18:53:33Z | Watched and reverted: with GS.Combat.ACFDamage 1 the ACF path computed damage correctly and applied none of it, because UGSACFDamageCalculation returned the float without filling the out-array ACF actually applies. Michael flipped the cvar to 0 and damage worked again. Fix is in (FAttributeData on the out-array) and the cvar now defaults to 0.
scenario: L_CombatArena, player-driven fight, 2026-08-21 18:32 - the same session that logged Add Ragdoll Master and DefaultHitCue warnings once damage began reaching ACF.
files: 
  - Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.cpp
  - Source/GoblinSiege/Characters/GSCharacterBase.cpp
---

## Goal

ACF Phase 2b-2c: the axe swing delivers damage through ACF

## Generate

**The axe swing now delivers through `UACFDamageHandlerComponent::TakeDamage`**, behind
`GS.Combat.ACFDamage` (default **1**, on).

- The swing builds an `FPointDamageEvent` carrying `UGSDamageType_Axe`. The sweep is an OVERLAP, so
  there is no real impact point - one is synthesised from the target's location with the normal
  pointing back at the attacker. ACF reads `hitDirection` for the ragdoll impulse and for which hit
  reaction to play, so a zero vector would make every corpse fall the same way.
- `AGSCharacterBase::PostInitializeComponents` points the handler at `UGSACFDamageCalculation`.
  Without that line ACF would deliver ACF's numbers rather than this game's - its default
  `UACFGASDamageCalculator` knows nothing about the directional plate, the minimum-damage floor or
  the NPC-vs-NPC scalar.
- Guard break still resolves BEFORE damage, unchanged: break first and the kick lands on an open
  target; break after and it is politely blocked by the guard it just destroyed.
- `GS.Combat.ACFDamage 0` restores the gameplay-effect path exactly as it was.

## Evaluate

**BUILT, NOT WATCHED.** This is the change that alters how every axe hit resolves, and the person who
can see it is not the person who wrote it.

**Why the cvar exists:** if damage misbehaves mid-playtest it flips back without a rebuild. **It is
temporary.** Two live damage paths is two combat systems to keep in step, and the second one will rot.
Delete it once the ACF path has been watched and trusted.

**What should now start working, and is the reason for the whole exercise:** ACF's ragdoll finally has
a real `LastDamageReceived` to read, so `GoRagdollFromDamage` stops returning early - which means our
own ragdoll block in `HandleDeath` becomes the redundant one and should be removed. ACF hit-response
actions and ruling 32's defense stance also read this event.

**What to watch, in order of how loudly it would fail:**
- Hits land at all, and a goblin still dies in about the same number of swings.
- A knight still shrugs off a straight-on hit and still takes full damage from behind - that is the
  directional plate surviving the port.
- Nothing takes exactly 0 damage - the minimum floor.
- Defender-on-defender fights are not suddenly twice as lethal - the 0.55 NPC scalar.
- Corpses ragdoll from the direction they were hit.

**Only the axe swing is switched.** Arrows, the torch, fire volumes and fall damage all still use the
gameplay-effect path, deliberately - one source at a time, watched, before the rest follow.

## Refine

The synthesised hit result is the weakest part. A real sweep with `FHitResult` per contact would give
ACF a true impact point and bone name, which matter for damage zones (head multipliers) later. Not
worth changing the sweep for that today, but it is why `DamageZone` will read `ENormal` for every hit
until it is.

## Addendum - watched, and it applied no damage

Michael: *"I turned GS.Combat.ACFDamage 0 and damage started to work again."*

**Cause, and it was mine.** `UACFDamageHandlerComponent`:

```cpp
tempDamageEvent.FinalDamage = DamageCalculator->CalculateFinalDamage(tempDamageEvent,
                                  tempDamageEvent.AttributesData);          // :196
...
for (const auto& damageData : LastDamageReceived.AttributesData) { ... }    // :113 - APPLIED HERE
```

The returned float is only RECORDED on the event. What is actually dealt comes entirely from the
out-array, and `UGSACFDamageCalculation` returned the right number while leaving that array empty. So
ACF computed a correct hit and applied nothing.

Fixed: the calculator now adds `FAttributeData(UACFFunctionLibrary::GetHealthTag(), Damage)`. The
value is a COST, not a delta - `ConsumeStatistics` subtracts what it is given, so a positive number
removes health.

**`GS.Combat.ACFDamage` now defaults to 0.** Working combat is the baseline; the ACF path stays
opt-in until the fix has been watched.

**Two more unconfigured ACF pieces surfaced the moment damage flowed through it**, both new in that
session's log and neither fatal:
- `Add Ragdoll Master to your Game Mode!` - same family as the Collisions Master added in #229. ACF's
  ragdoll is being REACHED now, which is the point, and still cannot complete.
- `DefaultHitCue is invalid - UACFEffectsManagerComponent::PlayHitReactionEffect` - ACF's effects
  manager firing hit reactions with no cue configured.

**What this ticket did NOT cause:** the twitching. Michael reports the same twitch in archer MOVEMENT,
which never touched the damage path. That is a separate problem - see #238 - and the reaction delay
built there only gates swinging, so it does not address archers either.
