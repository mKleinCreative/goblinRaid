---
id: 260
title: The holstered axe sits in the aim sightline: hide it while aiming
agent: claude-acf
status: done
claimed: 2026-08-23T21:36Z
build: done
waiting_on: Rebuilt: the axe now stays hidden for the whole time the bow is out, not just during the aim. Michael to confirm the pop is gone.
evaluated: 2026-08-23T22:02:58Z
observed: 2026-08-23T22:02:27Z | Michael confirmed the axe no longer sits in the sightline and no longer pops back on release - 'works great'
scenario: ranged mode in play, drawing and loosing the bow with the axe holstered
files: 
  - Source/GoblinSiege/Weapons/GSWeaponComponent.h
  - Source/GoblinSiege/Weapons/GSWeaponComponent.cpp
  - Source/GoblinSiege/Combat/GSAimComponent.cpp
## Goal

Michael: *"the axe is in the field of view of what we're aiming at"*, then after the first fix:
*"the axe pops into place so you can't see what you hit."*

## Generate

Measured first: nothing was misconfigured. `back_sword` resolves to Spine02 at (12, 6, 30), the axe
wears a 1.6 scale on `SM_WoodcutterAxe`, and the over-the-shoulder aim camera points at that exact
spot. The holster and the camera simply want the same space.

`UGSWeaponComponent::SetAimActive` plus a `bAimActive` flag; the holstered melee mesh is now shown
only when `bShowHolsteredWeapon && !bAimActive && !bRangedActive`. `bActive` is still ORed in front,
so a weapon actually IN HAND is never hidden by either condition.

Driven from `UGSAimComponent::BeginAim`/`EndAim` rather than the character's input handlers, so every
route into an aim - bow, torch, anything added later - gets it without anyone having to remember.

**`!bRangedActive` is the half that came from Michael's second report.** Hiding on aim alone made the
axe reappear the instant the shot was loosed, landing the pop exactly on the moment the player is
trying to read. `!bAimActive` is kept as well because the torch is not covered by `bRangedActive` and
has the same camera problem.

## Evaluate

Observed by Michael: "works great" - the axe is gone from the sightline and no longer pops back on
release.

One build failure on the way, worth recording as a trap: the new declaration was inserted between a
`UPROPERTY()` and the variable it decorated, so the macro attached to a function. UHT caught it with
"void type is only valid as a return type", which does not obviously point at the real cause.

## Refine

Michael asked for this "for now" and the code says so. The honest fix is a holster position that
clears the aim camera, at which point BOTH conditions can be deleted. Until then a weapon holstered
on the back is invisible whenever the bow is out, which is a real behaviour change and not just a
framing tweak.
