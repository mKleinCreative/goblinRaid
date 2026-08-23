---
id: 259
title: Bow montages: draw, hold loop and release on both skeletons, fired from the timing component
agent: claude-acf
status: done
claimed: 2026-08-23T19:08Z
build: none
waiting_on: BUILT. Watch a draw: nock, then hold looping, then recoil on release. Judge whether the goblin's arms sit right on the bow - that is #241's unreviewed retarget showing itself.
evaluated: 2026-08-23T21:42:24Z
observed: 2026-08-23T21:41:43Z | Michael watched the bow draw play through: nock, hold looping while drawn, recoil on release, on the goblin
scenario: ranged mode in play, drawing and loosing the bow
files: 
  - Content/Characters/ScoutV2/Montages/AM_Bow_Draw_Gob.uasset
  - Content/Characters/ScoutV2/Montages/AM_Bow_Hold_Gob.uasset
  - Content/Characters/ScoutV2/Montages/AM_Bow_Release_Gob.uasset
## Goal

The bow had no animation at all - three retargeted clips existed, no montages, and zero montage code
in the ability.

## Generate

Six montages, `DefaultSlot`, one Default section each, created via `AnimMontageService`:
`AM_Bow_Draw/Hold/Release` for `_Gob` (1.03 / 3.77 / 0.70s) and `_Hum`. Hold's section is self-looped
so it runs for as long as the draw is held.

Three assets rather than three sections of one, matching `UGSGA_Horn` - the same event shape
(attack, sustain, release), and drift between the two would be visible.

Playback is split the same structural way as the damage multiplier, with no `IsPlayerControlled()`
anywhere: the PLAYER is always the goblin, so `UGSBowTimingComponent` owns the `_Gob` set and drives
Draw on `BeginDraw`, hands off to the looping Hold on a timer, and plays Release only on a loosed
shot. Erika has no timing component, so `UGSGA_BowShot` plays her `_Hum` recoil gated on that
component's ABSENCE.

`StopBowMontage` stops the tracked montage by name rather than calling `StopAnimMontage(nullptr)`, so
a hit reaction or dodge that started mid-draw survives a bow release.

## Evaluate

Observed by Michael: nock, hold looping, recoil on release, all playing on the goblin. Confirmed
"looked good".

This shipped on top of a dead retarget - all three `_Gob` clips measured 0.00 degrees of motion, and
the montages were authored before anyone checked. Fixed under #241 by re-retargeting through the
project's existing `RTG_MixamoToGoblin_InPlace`; the montages needed no change because the clips were
overwritten in place.

## Refine

Deliberately not done: **Erika gets a recoil but no draw.** An AI archer's draw window is
`UBTTask_RangedAttack::DrawSeconds` (0.8s), which is where a draw animation has to start to line up
with it - by the time the ability activates the arrow is already leaving. That belongs in the BT task
and is its own ticket.
