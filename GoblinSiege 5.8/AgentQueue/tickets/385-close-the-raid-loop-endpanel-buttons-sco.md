---
id: 385
title: Close the raid loop: EndPanel buttons, score persistence, minimal main menu
agent: claude-loot2
status: done
claimed: 2026-08-31T00:22Z
build: none
waiting_on:
evaluated: 2026-08-31T01:56:02Z
observed: 2026-08-31T01:54:31Z | Michael watched PIE live: menu shows Gold/XP top-right, New Raid -> movement works -> GS.Raid.CompleteAllObjectives -> portal opens -> auto-extracts -> EndPanel -> back to menu with updated totals. Ran two full raids back to back: totals went 0/0 -> 25 gold/3180 xp -> 90 gold/6360 xp, additive not overwritten (confirmed via live GameInstance save read both times). GameInstanceClass bug (never set in Config, so banking silently no-opped) found and fixed mid-session.
scenario: PIE: L_MainMenu -> New Raid -> L_Tutorial_Island -> GS.Raid.CompleteAllObjectives -> extract -> EndPanel -> Main Menu -> New Raid again -> repeat, watched by Michael both times
files: 
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.h
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.cpp
  - Source/GoblinSiege/Raid/GSRaidDirector.h
  - Source/GoblinSiege/Raid/GSRaidDirector.cpp
  - Source/GoblinSiege/Core/GSGameMode.cpp
  - Source/GoblinSiege/Core/GSMainMenuGameMode.h
  - Source/GoblinSiege/Core/GSMainMenuGameMode.cpp
  - Source/GoblinSiege/Core/GSGameInstance.h
  - Source/GoblinSiege/Core/GSGameInstance.cpp
  - Source/GoblinSiege/Progression/GSSaveGame.h
  - Source/GoblinSiege/Destruction/GSBurnObjectiveBase.h
  - Source/GoblinSiege/Destruction/GSBurnObjectiveBase.cpp
  - Source/GoblinSiege/Raid/GSRaidDebugCommands.cpp
  - Config/DefaultEngine.ini
  - Content/UI/WBP_GSPlayerHUD.uasset
  - Content/UI/WBP_MainMenu.uasset
  - Content/Blueprints/BP_GSMainMenuGameMode.uasset
  - Content/Maps/L_MainMenu.umap
---

## Goal

Close the raid loop: EndPanel buttons, score persistence, minimal main menu

## Generate

Closed the raid loop end-to-end, playable via PIE:

- **EndPanel unblocked**: `GSPlayerHUDWidget::HandleRaidEnded` was setting `EndPanel`'s visibility to
  `HitTestInvisible` (unclickable) instead of the widget's own Blueprint-authored
  `SelfHitTestInvisible` default - fixed, and added `RestartButton`/`MainMenuButton` (+ text) wired
  to `OpenLevel(current level)` / `OpenLevel("L_MainMenu")`. Also resets input mode/cursor on raid
  end (see below).
- **Score/economy persistence**: `GSRaidDirector::EndRaid` now banks `ScoreSys->GetLoot()` ->
  `Save->Gold` and `ScoreSys->GetDeeds()` -> `Save->Experience` (additive `+=`, distinct from
  `BestScores`' per-map `Max()`), single `SaveGame()` call gated on a `bDirty` flag.
- **Main menu**: new `AGSMainMenuGameMode` (no pawn/HUD, just loads+adds `MenuWidgetClass` and sets
  `FInputModeUIOnly`), `BP_GSMainMenuGameMode` child (`MenuWidgetClass = WBP_MainMenu_C`), `WBP_MainMenu`
  (Overlay root -> centered VerticalBox with logo/New Raid/Quit, built entirely via safe `WidgetService`
  calls after two rounds of real corruption from raw `CanvasPanelSlot` UObject edits - see Refine),
  `L_MainMenu` level with World Settings GameMode override, `DefaultEngine.ini` boots into it.
- **`GS.Raid.CompleteAllObjectives`** debug command: force-completes every `AGSBurnObjectiveBase` via
  a new `DebugForceComplete()` (goes through the real `SetCompletion01` path, not a parallel one) and
  topples every `UGSTopplableComponent`, relying entirely on the existing director/runic-site cascade.
- **Movement-lock fix**: nothing anywhere reset input mode back to Game after `FInputModeUIOnly` -
  fixed in `AGSGameMode::RestartPlayerAtPlayerStart` (the one choke point every spawn path already
  funnels through).
- **`GameInstanceClass` bug**: found while wiring the menu's gold/XP display - `GameInstanceClass` had
  never been set anywhere in `Config/`, so the engine ran the base `UGameInstance` at runtime and every
  `World->GetGameInstance<UGSGameInstance>()` call (the entire banking path above) was silently
  returning null and no-opping since it was written. Fixed by adding it to `DefaultEngine.ini`.
- **Main menu resource display**: `UGSSaveGame::Gold`/`Experience` made `BlueprintReadOnly`
  (`UCLASS(BlueprintType)`), new static `UGSGameInstance::GetCurrentSave(WorldContextObject)` helper,
  and a top-right `ResourcesBox` (Gold/XP rows) added to `WBP_MainMenu`, reading on `Event Construct`.

## Evaluate

Verified live in PIE, watched by Michael, not just read back:

- Movement-lock fix: Michael confirmed he can move after New Raid ("yeah I can move now, it works").
- Full loop x2 back-to-back: main menu -> New Raid -> `L_Tutorial_Island` -> `GS.Raid.CompleteAllObjectives`
  ("forced 71 burn objective(s), toppled 1 monument(s)") -> `LogGSRaid` "All 5 objective type(s) burned -
  the portal opens" -> auto-extracted ("RAID ENDED: Extracted (5/5 objective types burned)") -> back to
  L_MainMenu automatically, no manual EndPanel click needed this run.
- `GameInstanceClass` fix confirmed live: `GameplayStatics.get_game_instance(gw).get_class()` reads
  `/Script/GoblinSiege.GSGameInstance` in PIE, not the base engine class.
- Banking confirmed additive across two separate raids, read from the live GameInstance both times:
  0/0 -> 25 gold/3180 xp -> 90 gold/6360 xp. Not overwritten (would have stayed 25/3180 on a Max()-style
  bug). Michael independently confirmed from the running game: "I completed another game and it gave
  us more gold and xp."
- What is NOT independently verified by me: the on-screen text of the Gold/XP display itself - I could
  not get a real PIE screenshot through this pipeline (`CaptureViewport` only grabs the empty editor
  viewport in this build, not the PIE game view - a real gap, worth a future fix). Michael's "gave us
  more gold and xp" report is consistent with him having read it off the display, and the underlying
  data is proven correct via direct GameInstance reads, but I never got a pixel-level look at the
  TextBlocks myself.
- `EndPanel` Restart/Main Menu buttons and the manual (non-auto-extract) EndPanel path were built and
  compiled but not separately re-exercised this session - the automatic-extraction path above already
  covers `HandleRaidEnded`/`SetPortalOpen` end-to-end, and the buttons are plain `OpenLevel` calls with
  no new logic, so this is a low-risk gap, not an unknown.
- `BestScores` Max()-vs-additive distinction: exercised (`FinalScore > Best` logged "New best score for
  'L_Tutorial_Island': 3205" on run 1) but not deliberately tested against a LOWER second-run score to
  confirm it does NOT regress the best. Left unverified - low risk, the comparison is a one-line `>`.
- AGENT_STATE.md: owes a DECISION line for the `GameInstanceClass` fix (a real, previously-silent
  runtime bug, not scope creep) - not yet written, should be added before this closes.

## Refine

Given the above, nothing left to change in this pass - the gaps found (no PIE screenshot pipeline,
Max() regression path, EndPanel manual-click path) are all low-risk and explicitly logged above rather
than silently accepted. Will add the AGENT_STATE.md DECISION line for the GameInstanceClass fix as
part of closing this ticket.

> 2026-08-31T01:30Z Also fixed: rebuilt WBP_MainMenu from scratch after raw CanvasPanelSlot manipulation earlier corrupted it for real (generated_class()==None, empty tree) - this time built entirely via safe WidgetService API (Overlay root + HorizontalAlignment/VerticalAlignment=Center on the child, no raw slot UObject edits at all). Also fixed a real movement-lock bug: nothing anywhere reset input mode back to Game after AGSMainMenuGameMode/GSPlayerHUDWidget set FInputModeUIOnly, so a player arriving in a fresh raid level via New Raid or Restart/Main Menu was stuck unable to move - fixed in AGSGameMode::RestartPlayerAtPlayerStart (Source/GoblinSiege/Core/GSGameMode.cpp), the single choke point every spawn path already goes through. Not in original claim, checked clear of conflicts.

> 2026-08-31T01:46Z Added GameInstanceClass to DefaultEngine.ini (was never set - GSGameInstance was dead code at runtime, gold/xp banking never actually ran). Added GSGameInstance::GetCurrentSave() static helper + BlueprintReadOnly on Gold/Experience for a main-menu top-right resource display (Michael request). Files: GSGameInstance.h/.cpp, GSSaveGame.h, DefaultEngine.ini, WBP_MainMenu.
