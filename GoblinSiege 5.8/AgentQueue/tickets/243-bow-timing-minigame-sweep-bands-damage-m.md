---
id: 243
title: Bow timing minigame: sweep, bands, damage multiplier and aim sway
agent: claude-acf
status: done
claimed: 2026-08-21T19:42Z
build: none
waiting_on: Built. Inert until #236 frees GSPlayerCharacter for the component and the two input call sites.
evaluated: 2026-08-21T21:32:42Z
observed: 2026-08-23T18:29:44Z | Michael played the bow: release timing changes the shot and the mechanic reads as intended - 'works great'
scenario: ranged mode in a live fight, drawing and releasing at different points in the sweep
files: 
  - Source/GoblinSiege/Weapons/GSBowTimingComponent.h
  - Source/GoblinSiege/Weapons/GSBowTimingComponent.cpp
  - Source/GoblinSiege/Weapons/GSArrowProjectile.h
  - Source/GoblinSiege/Weapons/GSArrowProjectile.cpp
  - Source/GoblinSiege/Combat/GSAimComponent.h
  - Source/GoblinSiege/Combat/GSAimComponent.cpp
---

## Goal

Bow timing minigame: sweep, bands, damage multiplier and aim sway

## Generate

The bow timing minigame, everything except the two input call sites.

**`UGSBowTimingComponent`** (new, `Weapons/`) — owns the sweep, the bounces, the sway and the last
verdict.
- 5s end-to-end, **ping-pong**. Reflection is a `while` loop, not a single clamp: a hitch can carry
  the indicator past an end and out the far side, and bounces are what drive the sway, so the COUNT
  has to stay honest.
- Bands derive from three numbers (`RedCentre 0.54`, `RedHalfWidth 0.025`, `OrangeFraction 0.40`);
  yellow is never authored, it is whatever is left, so the three cannot drift apart.
- Yellow's falloff is measured against the distance to the NEARER END of the bar rather than a fixed
  width — red sits at 0.54, so the run below it is longer than the run above, and a shared width
  would make one side steeper for no reason the player could see.
- Ticks only while drawing (`bStartWithTickEnabled = false`).

**Damage** reaches the arrow via `DrawQualityMultiplier` on `AGSArrowProjectile`, folded in exactly
where `HeadshotMultiplier` already is — it MULTIPLIES with the headshot bonus, so a perfect draw into
a head is the best shot in the game.

**Sway** is applied in `UGSAimComponent::GetAimRotation()` and nowhere else, because the muzzle
transform, the predicted arc and the landing decal all read that one function. The sway is therefore
VISIBLE — the arc wanders — rather than an invisible accuracy penalty. It does not move the camera
or reticle.

**`UGSGA_BowShot::GetFireCooldownRemaining(Avatar)`** — new, and it closes the risk the plan flagged.
The rate limit is a raw timestamp inside `CanActivateAbility`, invisible from outside, so a draw
started during the interval would run the whole bar and then be silently refused. It resolves the
interval the same way `GetFireIntervalSeconds` does (equipped weapon, falling back), because a second
copy of that logic would drift.

## Evaluate

**NOT BUILT, NOT RUN.**

**Two call sites are missing and cannot be written yet:** `BeginDraw()` in the ranged branch of
`Input_AttackPressed` and `ConsumeReleaseQuality()` in `Input_AttackReleased`, plus `CancelDraw()` in
the grapple/torch/light/heavy branches. `Characters/GSPlayerCharacter.h/.cpp` belong to
**claude-warren's #236**, which has right of way. Until those land, this feature is inert: nothing
calls `BeginDraw`, so the component never ticks, the multiplier stays 1.0 and sway stays zero.
**Existing bow behaviour is completely unchanged.**

**The build gate is closed** (#236 active), so this has not been compiled. Expect the usual first-pass
compile errors.

**The AI separation is structural, not conditional.** There is no `IsPlayerControlled()` anywhere in
this feature. `FireArrow` asks the avatar for a timing component; Erika has none, so her arrows keep
`DrawQualityMultiplier = 1.f`. `GetAimRotation` asks the same way, and AI has no `UGSAimComponent`
either. That must stay true — the moment someone adds this component to an AI pawn, defender archers
inherit a minigame nobody wrote for them.

**One correction made mid-implementation, worth recording:** the first version of
`GetFireCooldownRemaining` used `FallbackFireIntervalSeconds` unconditionally while claiming in its
comment that it "can never disagree" with `CanActivateAbility`. It would have disagreed for any
weapon setting `RangedAttackCooldownSeconds`. Both the code and the comment were fixed.

## Refine

`LastReleaseQuality` is reset in `BeginDraw` AND `CancelDraw`, not just consumed — a shot the ability
refuses (fire interval, a blocking tag) must not leave a perfect score lying around for the next
arrow to collect.

Deliberately not done: the HUD bar (phase 3) and the montages (phase 4). Phase 1 is verifiable with
`GS.Combat.LogDamage 1` alone, which is why it is first.
