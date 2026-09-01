---
id: 391
title: Enable loading screen for menu->raid transition (release day)
agent: claude-loadingscreen
status: done
claimed: 2026-08-31T19:53Z
build: none
waiting_on:
evaluated: 2026-08-31T19:56:01Z
observed: UNOBSERVED 2026-08-31T19:56:03Z - Config-only flip (EnableLoadingScreen=False->True), no code/UPROPERTY change so no rebuild needed to take effect, but nobody has watched the loading screen actually appear on a live New Raid click yet - needs one real PIE/packaged-build test.
scenario: none - never run
files: 
  - Config/DefaultPlugins.ini
---

## Goal

Enable loading screen for menu->raid transition (release day)

## Generate

Root-caused Michael's report ("there's no loading time after New Raid, so you can't tell if it's
broken") to `Config/DefaultPlugins.ini`'s `[/Script/AscentLoadingScreen.ALSLoadingScreenSettings]`
block: `EnableLoadingScreen=False`, set by ticket #335 and left there deliberately - the block's own
comment said it was disabled only because `L_MainMenu` and the New Raid button did not exist yet
("stage 8" was supposed to flip it once they did), and enabling it in the meantime would have put a
loading screen in front of every agent's PIE session for a transition that could not happen.

`L_MainMenu`, `WBP_MainMenu` and the New Raid button all shipped since (#385/#388) and stage 8 never
ran - the flip was a stale TODO. Flipped `EnableLoadingScreen` to `True`. Verified the widget the
setting already points at is real before flipping, not just present in the ini: `/Epic Games/UE_5.8/
Engine/Plugins/Marketplace/ACFUAsce.../Content/UITools/Widgets/ANS_LoadingScreen_WB.uasset` exists on
disk. Updated the block's comment to record why it's on now instead of leaving the old "deliberately
disabled" reasoning to read as still current.

## Evaluate

- The stale-TODO diagnosis and the widget asset's existence are both confirmed by direct inspection
  (grep on the ticket history, `find` on the plugin content folder) - not "should work".
- NOT verified: I did not see the loading screen actually appear on a live menu->raid transition.
  This is a config-only change (no code, no UPROPERTY, no compile) so no rebuild was needed to take
  effect, but it still needs one real PIE or packaged-build New Raid click, watched, before this is
  more than "the setting that gates it is now on."
- Untouched but adjacent and still stale: `WidgetRegistryAsset` / `DefaultMenuMap` / `DefaultNewGameMap`
  in the same file (lines ~76-79) are still commented placeholders from the same #335 stage plan,
  now that their own preconditions (the registry, `L_MainMenu`) may also be met. Did not touch -
  outside this ticket's one-line directive, and each is its own claim decision.
- Touched outside the original goal: nothing else.

## Refine

Leaving the `MinimumLoadingScreenDisplayTime=2.000000` and other `LoadingScreenAttributes` values
as-is - they're already tuned (the old comment's "five-second" was itself stale next to the 2.0
actually on disk), and changing them wasn't reported as a problem. If the screen shows but for the
wrong duration, that's a separate, smaller follow-up.
