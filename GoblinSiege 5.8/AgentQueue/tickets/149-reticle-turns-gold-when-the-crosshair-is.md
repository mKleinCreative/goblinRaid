---
id: 149
title: Reticle turns gold when the crosshair is on a valid order target
agent: claude-reticle2
status: done
claimed: 2026-08-13T06:45Z
build: required
waiting_on:
evaluated: 2026-08-13T22:50:11Z
observed: 2026-08-13T22:50:11Z | Michael played it and said it all works - the reticle turns gold on a valid target, the order wheel labels are readable around the crosshair, and the camera now clears the goblin so you can see what you are aiming at
scenario: PIE in L_CombatArena, player stood 750uu off six live defenders with a horn-summoned warband, sweeping the crosshair on and off guards and holding R to open the order wheel
files: 
  - Source/GoblinSiege/Horde/GSHordeCommandComponent.h
  - Source/GoblinSiege/Horde/GSHordeCommandComponent.cpp
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.h
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.cpp
  - Content/UI/M_UI_ReticleRune.uasset
---

## Goal

Michael: *"make the reticle turn gold on a valid target"*, following *"it's hard to attempt to aim a
command at something."*

Then, on seeing it: *"the crosshair doesn't appear at all"*, *"the cross hair is now over the goblin,
so it's not very useful"*, and *"the HUD when you hold down R doesn't show up anymore"* - three
follow-ups that turned this into the whole aiming-feedback loop rather than one tint.

## Generate

### The live crosshair - `UGSHordeCommandComponent`

`TickComponent` now runs `TickCrosshair` at **10Hz** (`CrosshairScanIntervalSeconds`), locally
controlled pawns only, calling the **same `TraceForOrder`** the order itself uses. Publishes
`HasCrosshairTarget()` / `GetCrosshairTarget()` and a `FGSOnCrosshairTargetChanged` that fires **only
on change**.

Sharing the trace is the design, not a shortcut: two answers to "is this orderable" is how a reticle
starts promising targets the order then refuses, and this session has already been burned twice by an
instrument that lied. **The scan pauses while the wheel is open** - the latch is frozen on the press
by design, so a live scan would have the reticle disagree with the order about to be issued.

### The tint - `UGSPlayerHUDWidget`

Binds the `Reticle` image (`BindWidgetOptional` + `BlueprintReadOnly`, per the stamina hazard that
class documents) and paints it **gold `(1, 0.82, 0.25)`** on a target, cool grey at rest. The gold is
deliberately the weapon wheel's committed-highlight gold, so "gold means this will happen" is one
language across the UI.

### Three follow-ups from watching it, all of which were my own regressions

1. **"The crosshair doesn't appear at all."** It was drawing at roughly 2% opacity. The material
   derives opacity from the texture's luminance, and `T_RuneOfFate` is a thin bright ring on a large
   black field - mipping 4096 to 72px averaged it toward black, then the idle tint's 0.5 alpha halved
   what was left. `ReticleOpacity` default 1.0 -> **8.0** (Opacity clamps at 1, so black stays 0) and
   draw size 72 -> **128**. I should have checked the opacity maths against the actual texture rather
   than assuming luminance carries a mostly-black source.
2. **"The HUD when you hold down R doesn't show up anymore."** It did. Read off the live widget:
   `WBP_HordeOrderWheel_C_0 | visibility: HIT_TEST_INVISIBLE | in viewport: True`, all four labels
   white at **alpha 0.45**. #146 moved them from a dark screen corner to centre over a bright arena,
   where 45% white with no outline is invisible. Fixed with `IdleColour` 0.45 -> **0.85**,
   `UnavailableColour` alpha 0.35 -> **0.55**, font 24 -> **30**, and a **black drop shadow** on all
   five labels. The shadow is the real fix - white-on-anything needs an outline, and it lives on the
   asset where `Repaint` (which only writes `ColorAndOpacity`) can never overwrite it.
3. **"The crosshair is now over the goblin."** Not a reticle fault - the reticle correctly marks where
   the trace goes. The camera was aimed at the player's own character: `SocketOffset (0, 55, 65)` is
   level with a short goblin, so screen centre landed on his head. Now **`(0, 70, 105)`**.
   **Raised rather than pitched**, deliberately: pitching the boom would fight the player's look input
   AND tilt the aim down with it, because the reticle follows camera forward. Lifting the socket
   clears the centre without moving where you are aiming.

## Evaluate

**Verified, and by a human this time:** Michael, after the final build - *"it all works now"*. That
covers the gold tint, the wheel labels being legible, and the camera clearing the goblin.

**Verified by instrument beforehand:** `HasCrosshairTarget` reads `True` with
`GetCrosshairTarget() -> BP_CastleGuard02_C_1` when aimed at a guard and `False`/`None` at the sky, so
the state genuinely changes rather than being stuck. Live `SocketOffset` read back as `(0, 70, 105)`.
Live wheel widget read back in-viewport with its labels' colours, which is what disproved "it doesn't
show up".

**FILES TOUCHED OUTSIDE THIS TICKET'S CLAIM, and that is a protocol breach I own:**
`Content/UI/WBP_HordeOrderWheel.uasset` (font + shadow), `Content/UI/WBP_GSPlayerHUD.uasset` (reticle
size), `Content/Blueprints/BP_GSPlayerCharacter.uasset` (camera socket) and
`Source/GoblinSiege/UI/GSHordeOrderWheelWidget.h` (label colours). The first two belonged to #146 and
#148, both already closed; the last two were never claimed by anything. Nobody else was working, so
no conflict occurred - but the claim is supposed to precede the edit and did not, and a second agent
would have been editing those files blind.

**Owed to AGENT_STATE:** the reticle exists and is target-aware; a VFX texture needs its luminance
boosted hard before it survives being mipped into a UI element; UI text at <0.5 alpha with no shadow
is legible only against the corner it was authored in; and the third-person camera aims at the
character unless the socket is lifted clear.

**Not measured:** the cost of the 10Hz sweep. Argued as negligible against the 4Hz broadphase every AI
already runs, never profiled.

## Refine

**Changed on self-review, three times, each after being wrong in public:** the reticle "missing" was an
opacity assumption; the wheel "not appearing" was my own legibility regression from #146 reported as a
new bug; and the crosshair "over the goblin" was a camera problem I would have chased in the reticle.
The pattern across all three is the same - a symptom described at the UI, a cause one layer down, and
my first instinct pointing at the wrong layer. Reading the live object first is what settled each one,
and it is the only reason the third took minutes rather than another build cycle.

**Deliberately left undone:**

- **`AGSHUD::HUDWidgetClass` is `None`.** Found while diagnosing the missing reticle. The HUD only
  exists because `BP_GSPlayerCharacter` creates it from its own graph; the `AGSHUD` path that
  `BP_GSGameMode` points at would create nothing, silently. Dormant, and a trap for whoever next moves
  HUD creation to where it looks like it belongs. Its own ticket.
- **Reticle size and camera height are opening guesses** that happened to land. 128px was deliberately
  overshot after it was invisible; `(0, 70, 105)` was a first try. Both are single fields.
- **Contextual reticle behaviour** - fading when no weapon is readied, tightening while aiming the bow.
  "Basic crosshair" was the ask.
