---
id: 071
title: Supplement to #069 - GSArrowProjectile Frenzy hook (files missed in that claim, same body of work)
agent: claude-horde
status: done
claimed: 2026-08-07T17:53Z
build: none
waiting_on:
evaluated: 2026-08-07T18:44:52Z
files: 
  - Source/GoblinSiege/Weapons/GSArrowProjectile.cpp
---

## Goal

Supplement to #069 - GSArrowProjectile Frenzy hook (files missed in that claim, same body of work)

## Generate

One change. `Weapons/GSArrowProjectile.cpp` calls `AGSCharacterBase::NotifyDealtDamage(OtherActor)`
on the SHOOTER immediately after `ApplyGameplayEffectSpecToTarget`, inside the existing
`SourceASC && TargetASC && OtherActor != Shooter && bMayDamage` guard.

This is the ranged half of #069's Frenzy rule — GDD §2.5, "anything you attack ... gets swarmed
automatically". #069 hooked the melee path (`GSGA_SwordLight`); without this one, shooting a guard
with the bow would not bring the horde, and the inconsistency would read in playtest as Frenzy being
unreliable rather than as a missing call site.

Announced on the shooter rather than on the arrow deliberately: the horde follows a summoner, and an
arrow in flight is nobody's summoner.

## Evaluate

**Not compiled, not run.** The build gate was closed for this ticket's whole life. Same caveat as
#069 — this is reasoning, not evidence.

Why a separate ticket at all: `GSArrowProjectile.cpp` was not in #069's claim, `gsqueue.ps1` has no
verb for adding files to an open ticket, and editing an unclaimed file is the exact thing the queue
exists to prevent. #005 ("files missed in that claim, same body of work") is the established
precedent for this shape.

Placement is inside the existing `bMayDamage` guard, which matters: that flag is what #038 added so
arrows respect `RaceTag`. Sitting inside it means a shot that hits your own goblin cannot register
that goblin as a threat. Putting the call outside the guard would have made the player's own horde
a Frenzy target every time he fired past his own line — and since arrows deliberately STICK in
allies rather than passing through (settled 2026-08-06, with the horde case explicitly on the
table), that would have happened constantly.

No behaviour changes for anything that is not a horde goblin: `NotifyDealtDamage` only broadcasts a
delegate, and nothing outside `AGSHordeGoblin` binds it today.

## Refine

Nothing changed on a second pass, and here is why the first survives scrutiny: it is one call at one
site, inside a guard that was already doing the friend/foe work, on a code path that #038 has
already reasoned about carefully.

Deliberately NOT extended to `GSGA_TorchToss`, which #069 claimed but did not touch. Fire damage
does not go through the thrower — it is applied by `AGSFireVolume`, which deliberately does not
check race (`GSFireVolume.cpp:391`, "the torch is the goblin equalizer and burns its owner too").
Hooking Frenzy to fire would mean a goblin standing in the player's own fire registers the player
as a threat. If torch-lit enemies should draw the horde, that is a design question, not a missing
call — raise it rather than adding the hook.
