---
id: 253
title: Bow timing hookup: the component on the pawn, draw on press, quality on release, cancel everywhere else
agent: claude-acf
status: done
claimed: 2026-08-22T03:57Z
build: none
waiting_on: BUILT. Needs Michael in ranged mode: hold to draw, release at different moments, GS.Combat.LogDamage 1 should show clearly different numbers.
evaluated: 2026-08-23T18:30:21Z
observed: 2026-08-23T18:29:44Z | Draw starts on press and the release is scored: the timing component drives real shots from the player pawn, with AI archers unaffected
scenario: ranged mode in a live fight, press-hold-release on the attack key
files: 
  - Source/GoblinSiege/Characters/GSPlayerCharacter.h
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
---
## Goal

Wire `UGSBowTimingComponent` into the player pawn. Everything was built and inert until #236 released
`GSPlayerCharacter`.

## Generate

Component added to the pawn (constructor + header), plus six call sites in
`Characters/GSPlayerCharacter.cpp`:

- **`BeginDraw`** in the ranged branch of `Input_AttackPressed`, **gated on
  `UGSGA_BowShot::GetFireCooldownRemaining`**. The ability refuses inside the fire interval using a
  raw timestamp rather than a cooldown GE, and it refuses SILENTLY - running the bar anyway would let
  the player judge a perfect release and then quietly eat the shot, which reads as the game stealing
  it. No bar means the refusal is visible.
- **`ConsumeReleaseQuality`** in `Input_AttackReleased`, **before** `TryActivateAbilityByClass`. The
  honest moment is the one the player judged, not 80ms later when the arrow spawns; and `FireArrow`
  reads the verdict through `GetLastReleaseQuality`, which only holds a value once this has run. The
  return value is discarded deliberately - the component keeps it, and a refused shot resets it.
- **`CancelDraw`** on grapple, torch, the melee fall-through, and putting the bow away. The swap case
  does not cancel the SHOT - the release still resolves it, as that code intended - but it resolves
  as an ordinary arrow, because the bow is no longer in hand.

## Evaluate

Observed by Michael in a live fight: the draw drives real shots and the release is scored. Confirmed
"works great" alongside #256.

AI untouched, structurally: `UGSGA_BowShot` is shared with `BP_ErikaArcher` and the gate is the
component's ABSENCE on AI, not an `IsPlayerControlled()` branch - there is no condition here to get
wrong later.

Two builds were needed because the module was broken by other agents' in-flight work:
`GSRaidLibrary.cpp` (fixed under #254) and `GSWarrenPlacementComponent.cpp:251` (fixed at Michael's
explicit instruction, noted on #245, their ticket left open).

**Known gap, deliberately not closed:** dying mid-draw fires no `OnBowDrawEnded`, so a HUD bar would
linger until respawn rebinds the widget. Harmless today because #244's bar does not exist yet; it
needs a `CancelDraw` on the death path before that bar ships.

## Refine

Nothing changed on review. The cooldown gate was the one design call not in the original plan - the
plan flagged it as a risk to decide before shipping, and refusing the draw is the option that keeps
the refusal visible.
