---
id: 368
title: Debris damage: released crumble pieces can kill on high-impulse collision
agent: claude-fire
status: done
claimed: 2026-08-30T05:04Z
build: none
waiting_on:
evaluated: 2026-08-30T05:11:48Z
observed: 2026-08-30T05:11:49Z | Player killed outright by falling debris on first live test
scenario: Live PIE, L_Tutorial_Island, standing near falling mill/building debris
files: 
  - Source/GoblinSiege/Destruction/GSCrumbleComponent.h
  - Source/GoblinSiege/Destruction/GSCrumbleComponent.cpp
---

## Goal

Debris damage: released crumble pieces can kill on high-impulse collision

## Generate

Michael: "Can we have it so the flying geometry causes death?" - a new capability, not a bug fix.
`UGSCrumbleComponent`'s own class comment already named this ("on impact, through
bEnableDamageFromCollision") as an intended-but-unimplemented flag, so this is finishing something
the component was designed for, not bolting on something foreign.

- New opt-in bool `bEnableDamageFromCollision` (default off, same convention as the whole collapse
  shape below it - a monument's rubble has never hurt anyone and must not start silently). Tunables:
  `DebrisDamage` (500), `MinImpulseToDamage` (20,000 - a floor so rubble settling against its own
  neighbours doesn't tick damage forever), `DebrisDamageEffectClass` (defaults to
  `UGSGE_WeaponDamage` in the constructor - a deliberately generic instant-damage GE that bakes no
  `Damage.*` tag of its own, exactly the shape a hazard supplying its own tag needs).
- In `ApplyRelease()`, right after `SetSimulatePhysics(true)`: if the flag is on,
  `Collection->SetNotifyRigidBodyCollision(true)` and bind `OnChaosPhysicsCollision` to a new
  `HandlePieceCollision`.
- `HandlePieceCollision` mirrors `AGSFireVolume::ApplyFireDamageTo`'s exact pattern (the only other
  hazard-damage call site in the project): authority-only, resolves the other actor's ASC via
  `UAbilitySystemGlobals::GetAbilitySystemComponentFromActor`, skips `State_Dead`/`State_Invulnerable`,
  tags the spec `Damage.Blast` (already armor-bypassing in `GSDamageExecCalculation` - right call for
  blunt masonry, not a slash/puncture), `SetSetByCallerMagnitude`, `ApplyGameplayEffectSpecToSelf`.
  Armor mitigation, race matchup, and death-at-zero-health all fall out of the existing damage
  pipeline for free - no bespoke kill path needed, confirmed by the research pass before writing any
  code (`AGSCharacterBase::HandleHealthChanged` already clamps to 0 and drives ACF's death flow for
  any lethal hit, fire or otherwise).

Rebuilt clean, no warnings. See #369 for wiring the flag onto actual falling debris (mill cap/sails,
building collapse) - this ticket is the mechanism, #369 is turning it on anywhere.

## Evaluate

**Verified live, first try, no iteration needed.** Michael: "Yay, I died, I think that's the first
time you oneshot a feature." Stood near falling debris (wired on in #369) and the hit killed him
outright - `DebrisDamage=500` against default character health is comfortably lethal, matching the
"a real tower coming down on you" framing from the request itself.

**Not separately tested:** the `MinImpulseToDamage` floor (does gentle rubble settling correctly NOT
tick damage) and the friendly-fire-adjacent question of whether goblin/defender AI takes debris
damage the same way a player does (the exec calc doesn't distinguish, so it should, but nobody stood
an NPC under a falling cap to confirm). Both are low-risk given the pattern is copied from
already-proven fire damage, not flagged as broken - just not independently observed yet.

## Refine

Closing `done`. The mechanism works as designed and was confirmed by the most direct evidence
available (the person testing it died). `MinImpulseToDamage` tuning and NPC-debris interaction are
left as open, low-priority follow-ups rather than blocking this ticket.

## Generate

<!-- REPLACE: what you produced. Files touched, what each change does, the calls
you made. Delete this comment when you write the section. -->

## Evaluate

<!-- REPLACE: judge your own output against the goal, adversarially. What is
verified and by what evidence (a log line, a PIE observation, a compile result -
not "should work"); what is written but has never run; what you touched outside
the goal; the DECISION or FAILED line this owes AGENT_STATE.md. -->

## Refine

<!-- REPLACE: what you changed in response to your own evaluation, and what you
are deliberately leaving undone. "Nothing changed, and here is why the first pass
survives scrutiny" is a valid answer; silence is not. -->
