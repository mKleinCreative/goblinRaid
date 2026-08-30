---
id: 335
title: Front-end stage 1: the ruling, and ACF UI config reaches the project
agent: claude-frontend
status: done
claimed: 2026-08-27T23:38Z
build: none
waiting_on: 
evaluated: 2026-08-28T23:26:39Z
observed: UNOBSERVED 2026-08-28T23:26:41Z - Iceboxed by Michael to free the build gate. The ini half was never verified - needs an editor restart then one CDO read-back (AUT_GameSettings_BP_C, GetTheme non-null, six SC_GS_* sound classes in order).
scenario: none - never run
files: 
  - docs/goblin-siege-gdd.md
  - docs/decisions-ledger.md
  - Config/DefaultPlugins.ini
  - Config/DefaultEngine.ini
  - Config/DefaultGame.ini
  - Config/DefaultInput.ini
  - Config/DefaultGameUserSettings.ini
---

## Goal

Front-end stage 1: the ruling, and ACF UI config reaches the project

## Generate

Ruling 71 (front end on ACF's UI stack) needs ACF's plugin config to reach the project first.
Nothing was compiled and no C++ was touched; this ticket is documents and ini only.

**The ruling, written down**
- `docs/decisions-ledger.md` — new dated group, **rulings 70/71/72**: the project gets a front end;
  it is built on ACF's UI stack, not hand-rolled UMG; and there is no save game. The entry records
  the two ACF defects found while planning, so nobody rediscovers them.
- `docs/goblin-siege-gdd.md` — §12.4 IN column gains the front end, CUT column gains mid-raid
  saving; revision row v1.4. `Tools/check_gdd.py`: **CLEAN, 10 checks passed.**

**The config, replicated by value**
- `Config/DefaultEngine.ini` — `GameUserSettingsClassName` and
  `GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient`.
- `Config/DefaultPlugins.ini` — `[/Script/AscentUITools.AUTDeveloperSettings]` (theme, layer tag,
  corrected `KeysConfigByPlatform`, and the six-entry `DefaultSoundClasses`) plus a deliberately
  **disabled** `[/Script/AscentLoadingScreen.ALSLoadingScreenSettings]`.
- `Config/DefaultGame.ini` — `CommonUISettings` + `CommonInputSettings.InputData`.
- `Config/DefaultInput.ini` — `[/Script/CommonUI.CommonUIInputSettings]`, nine UI actions.
- `Config/DefaultGameUserSettings.ini` — **new file**; the project shipped no quality preset at all.

**The one that mattered.** ACF's `DefaultSoundClasses` points at its own `SC_ANS*` classes, which no
cue in this game is routed through. Copied as shipped, every audio slider would move, save, persist
and change nothing audible. It now names the project's own six (`SC_GS_Master/Music/SFX/Voice/
Ambience/UI`, #249). `UAUTGameUserSettings::AudioVolumeLevels` is a bare `TArray<float>` indexed
**positionally** against that array, so its order is a load-bearing contract — said so in the file.

## Evaluate

**Verified, with evidence:**
- `Tools/check_gdd.py` → `CLEAN: 10 checks passed` after the §12.4 and revision edits.
- **Every asset path written to config resolves on disk** — all 13 checked individually
  (6 sound classes + `SMix_GS_Base`, 5 ACF assets, `/CommonUI/GenericInputData`). This is the check
  that matters most here, because the defect being avoided *is* a dangling path.

**Written but NEVER RUN — this is the honest state of the ticket.** The editor is closed
(`Get-Process Unreal*` → nothing), and `DefaultEngine.ini` / `DefaultPlugins.ini` are read at
startup. So **not one line of this has been proved at runtime.** Specifically unproven:
`GetGameUserSettings()` actually returning a `UAUTGameUserSettings`; `GetTheme()` resolving;
`GetDefaultSoundClasses()` returning six entries in the written order; CommonUI accepting the
viewport client class. The plan's own top-priority check is exactly this and it has not happened.
That is what this ticket is waiting on, and it must not be closed until somebody has run it.

**Three things deliberately left dangling-free rather than dangling:**
- `WidgetRegistryAsset` is **unset**, not pointed at a not-yet-existing asset. ACF's own shipped
  value is a dangling reference into `/Game/FullSample/` (not installed here), and no
  `UANSUIWidgetRegistryDataAsset` instance ships anywhere in the plugin — a content grep for the
  class returns zero. Stage 4 authors it from scratch and sets this line.
- `DefaultMenuMap` / `DefaultNewGameMap` unset until `L_MainMenu` exists (stage 8).
- `IconsByTag` omitted entirely — ACF's value points at a FullSample DataTable that is not here.

**Touched outside the goal:** nothing.

**Two findings AGENT_STATE.md should carry:**
1. `UAUTUIFunctionLibrary::SetSoundClassVolume` is `TargetClass->Properties.Volume = NewVolume;` —
   it writes into the `USoundClass` **asset**, no mix, no override, and dirties the package in
   editor. ACF's audio page uses it. Stage 3 replaces that path with `SetSoundMixClassOverride` on
   `SMix_GS_Base`; until then, dragging an ACF audio slider in PIE will dirty `SC_GS_*` assets.
2. `Saved/Config/WindowsEditor/GameUserSettings.ini` already exists and **shadows** the new
   `DefaultGameUserSettings.ini` for the editor (it currently holds `sg.ResolutionQuality=0` and
   everything else at 3). The preset cannot be observed without deleting that file first.


**ICEBOXED 2026-08-28 on Michael's call (claude-acf, on behalf of claude-frontend).** Closed to free
the build gate, following the precedent of #342. Nothing here has been verified and this ticket must
not be read as if it were.

**What is on disk and unverified:** the ruling in `docs/decisions-ledger.md` (rulings 70/71/72) and
the GDD §12.4 edit are documents and stand on their own. The **ini changes are the untested part** -
`Config/DefaultPlugins.ini`, `DefaultEngine.ini`, `DefaultGame.ini`, `DefaultInput.ini` and the new
`DefaultGameUserSettings.ini`.

**What was still owed when it was iceboxed**, per its own waiting_on: an EDITOR RESTART (ini is read
at startup), then one Python round-trip to read the CDOs back and confirm three things -
`GetGameUserSettings` class name is `AUT_GameSettings_BP_C` and not `GameUserSettings`, `GetTheme`
comes back non-null, and `GetDefaultSoundClasses` lists six `SC_GS_*` entries in order. Several editor
restarts have happened since for unrelated reasons, so the ini is almost certainly loaded - but
nobody has read those CDOs, so it stays unobserved.

## Refine

- `GameUserSettingsClassName` was going to point at `/Script/GoblinSiege.GSGameUserSettings`, a class
  that will not exist until stage 3. That would have made this ticket unverifiable by construction
  and shipped precisely the dangling reference it criticises ACF for. It now points at ACF's
  `AUT_GameSettings_BP`, so the plumbing is provable the moment the editor opens; the file says in
  place that stage 3 replaces it.
- The loading screen block is written but `EnableLoadingScreen=False`. Enabling it now would put a
  loading screen in front of every other agent's PIE session for no gain — stage 8 flips it when
  there is a menu to transition from.
- Four stale keys in ACF's shipped config were **not** copied (`StartUpAttributes`,
  `DefaultMenuMapName`, `DefaultNewGameMapName`, `ComponentsToBeSaved`); all four were checked
  against the current headers and have no matching `UPROPERTY`. `KeysConfigByPlatform`'s mount
  prefix was corrected from ACF's `/AscentUITools/` (which does not exist) to
  `/AscentCombatFramework/`.
- Six audio sliders, not thirteen. The finer classes (`SFX_Player/NPC/Impact/World/Signal`,
  `Voice_Human/Creature`) are mix structure and inherit from the six; exposing them would be a
  settings page that is harder to use and no more capable.
- **Left undone on purpose:** the runtime verification above, because the editor is closed. Nobody
  should mark this done on the strength of "the ini files look right" — that is exactly the reading
  that let #228's empty `HealthAttribute` sit unnoticed.
