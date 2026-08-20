---
id: 169
title: Channel progress ring around the reticle: the interact feedback nothing was bound to
agent: claude-channelring
status: done
claimed: 2026-08-17T22:06Z
build: none
waiting_on:
evaluated: 2026-08-17T22:15:11Z
observed: 2026-08-18T01:32:00Z | Michael watched the ring fill around the reticle on the smashed crate, the pig and the chest, and confirmed it reads correctly. One chest attempt showed no ring and worked on the retry; he attributed it to a misclick, not a defect.
scenario: PIE in L_CombatArena: hold F on BP_LootChest (1.5s), BP_Livestock_Pig (1.0s, carry and put down) and the smashed BP_LootCrate (1.0s), ring authored at 144px concentric with the 128px reticle
files: 
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.h
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.cpp
  - Content/UI/WBP_GSPlayerHUD.uasset
  - Content/UI/Materials/M_GS_ChannelRing.uasset
---

## Goal

Channel progress ring around the reticle: the interact feedback nothing was bound to

## Generate

Michael: *"we need to create a visual indicator ... a circle around that reticule we have that slowly
fills based on the percentage done with the interaction."*

`UGSInteractionComponent` has published `OnChannelStarted` / `OnChannelProgress` / `OnChannelEnded`
since it was written and **nothing had ever bound to them**. The plumbing existed; the display did
not. Three pieces:

**1. `M_GS_ChannelRing`** (new) - UI domain, unlit, translucent. Two `Custom` HLSL nodes rather than a
node-graph puzzle: `RingMask` returns an annulus (`smoothstep` band at 0.68-1.0 of the half-extent),
`FillMask` returns `atan2(D.x, -D.y)` normalised and compared against `Percent`, which puts angle zero
at 12 o'clock and sweeps clockwise like a clock. Emissive is `lerp(TrackColor, FillColor, FillMask)`;
Opacity is the ring mask. Parameters `Percent`, `FillColor` (the same 1.0/0.82/0.25 gold as the
reticle's valid-target tint and the weapon wheel's commit highlight), `TrackColor`.

**2. `WBP_GSPlayerHUD`** - an `InteractRing` Image, 144x144, anchored (0.5,0.5) with alignment
(0.5,0.5) so it is concentric with the 128x128 `Reticle`, z-order 0 against the reticle's -1. Ships
`Collapsed`.

**3. `UGSPlayerHUDWidget`** - binds the three delegates in `NativeConstruct` next to the reticle's,
unbinds in `NativeDestruct`, and pushes `Percent` into a lazily-created MID from
`UImage::GetDynamicMaterial()`.

`ChannelCompleteHoldSeconds` (0.18) is the one non-obvious part and it is the whole reason a completed
channel reads as success. `TickComponent` broadcasts a clamped `1.0` and calls `CompleteChannel` on the
**same tick** (`GSInteractionComponent.cpp:284-294`), so the ring receives a full value but never gets
a frame to draw it in - `OnChannelEnded` hides it before the next present. Without the hold the player
watches the ring vanish just shy of closing every single time. That is the same one-frame illusion that
made the loot chest look broken in #163, where the debug text died on the same frame for the same
reason; this is that lesson turned into a fix rather than a comment.

## Evaluate

**The material is OBSERVED, by rendering it rather than by trusting the compile.** `capture_preview`
at 420px with `Percent` temporarily at 0.35 shows a clean annulus, dark track, gold fill sweeping
clockwise from 12 o'clock and stopping about 35% round. Defaults were restored afterwards and read
back at `0.000000`. That is the shape and the sweep direction confirmed as pixels.

**Built and OBSERVED.** The build succeeded (218s, only the two known C4996 `AbilityTags` warnings), and
Michael watched the ring fill around the reticle on the smashed crate, the pig and the chest.

**The one reported miss was user error, by Michael's own account** - the chest showed no ring on his
first attempt and worked on the second; he put it down to a misclick. Recorded because the reasoning
that followed it was wrong in an instructive way: the chest happened to be the first channel of the
session, so I built a tidy theory about the material instance being created lazily on a widget Slate
had not yet rendered. It fitted the evidence perfectly and it was not what happened. **The ordering in
`HandleChannelStarted` was left exactly as written** - no fix was shipped against a symptom that never
reproduced.

## Refine

**Changed after self-review:** the header comment first claimed the last progress tick "lands short of
1.0 and completion happens on the next frame". Reading `TickComponent` proved that wrong - progress
reaches exactly 1.0 and completion fires on the *same* tick. The fix was identical either way, but a
comment that misstates the mechanism is how the next person removes the hold as redundant.

**Deliberately not doing: the interact PROMPT.** `OnFocusChanged` is the fourth delegate and is still
unbound, so nothing on screen names the verb - the ring answers "how far through am I" but not "what
will F do". That is the other half of #163's gap and it wants its own ticket, because it is text
layout and localisation rather than a material.

**Deliberately not animating the ring's appearance.** A fade-in would compete with the fill for the
player's attention during the first few frames of a short 1.0s channel. Worth revisiting only if the
snap-on reads harshly.
