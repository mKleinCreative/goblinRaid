---
id: 383
title: Design: persist score across sessions and close the raid loop (end panel is a dead end)
agent: claude-savegame
status: abandoned
claimed: 2026-08-30T15:47Z
build: none
waiting_on:
evaluated: 2026-08-30T15:48:32Z
observed:
scenario:
files: []
---

## Goal

Design: persist score across sessions and close the raid loop (end panel is a dead end)

## Goal

Michael: "run a subtask to figure out how to bank the points you receive locally between sessions.
we need to close the raid loop. (meaning we need to work on the UI assets next, so we can complete
the tutorial raid and then exit to the main menu and restart the map.)" **Design writeup only** - no
code/content changed. Two questions bundled together because they are the same feature from the
player's side: the raid has no ending a player can act on.

## Generate

**Score persistence: the plumbing exists, fully built, and is called by NOTHING.**
`UGSGameInstance` (`Source/GoblinSiege/Core/GSGameInstance.cpp`) already has a real `USaveGame` path -
`Init()` loads `UGSSaveGame` from slot `"GoblinSiegeSave"` if it exists or creates a fresh one;
`SaveGame()` writes it back via `UGameplayStatics::SaveGameToSlot`. `UGSSaveGame`
(`Source/GoblinSiege/Progression/GSSaveGame.h`) already declares `TMap<FName, int32> BestScores`
keyed by map/mission name, with a comment citing "tech doc S10, Part II S23 score screen" - this was
clearly scoped and half-built on a previous pass, then parked. Grepped the entire `Source/GoblinSiege`
tree for every caller of `SaveGame()`, `GetSaveGame()`, `CurrentSave`, `BestScores` outside the two
files that declare them: **zero hits.** Nothing ever calls `SaveGame()`. Nothing ever writes
`BestScores`. This project has never actually persisted anything to disk - the seam is there, wired
to nothing, which is a materially different (and better) starting point than "build save/load from
scratch."

`UGSScoreSubsystem` (`Source/GoblinSiege/Raid/GSScoreSubsystem.h`) is the live in-memory tally -
`Deeds` + `Loot`, fed by `AGSBurnObjectiveBase::OnBurnObjectiveCompleted` and (as of tonight's #379
work) `UGSLootBankComponent::CommitBank`. It is a `UWorldSubsystem`, which the header states plainly
is deliberate ("matching `UGSRaidDirector` ... needs no level authoring") - which also means it is
**destroyed and recreated on every level load**, including the one Michael is asking for
("restart the map"). Nothing currently reads `GetTotal()`/`BuildSummaryLine()` and writes it into
`BestScores` at any point - that write is the entire missing link between "the raid ended with a
score" and "the score survived past this play session."

**Closing the raid: `OnRaidEnded` already fires and already drives a real end panel - which is a
visual dead end, not a functional one.** `UGSRaidDirector::EndRaid` (`GSRaidDirector.cpp:580`)
broadcasts `FGSOnRaidEnded` on extraction, left-behind, or out-of-lives.
`UGSPlayerHUDWidget::HandleRaidEnded` (`GSPlayerHUDWidget.cpp:1276`) is bound to it, sets title/detail
text (`"EXTRACTED" / "LEFT BEHIND" / "OUT OF LIVES"`), pulls `ScoreSys->BuildSummaryLine()` onto the
panel, and calls `EndPanel->SetVisibility(ESlateVisibility::HitTestInvisible)`. **That visibility
value is the whole bug: `HitTestInvisible` means visible but explicitly NOT clickable** - so even if
a Restart/Quit button already existed on `WBP_GSPlayerHUD`'s `EndPanel` (unconfirmed - the C++ only
knows about `EndTitleText`/`EndDetailText`/`EndScoreText`, no button reference at all), the panel as
currently wired could not be pressed. The raid has a real, working "the raid is over" signal and a
real panel that displays it - and then nothing. No restart call, no `OpenLevel`, no `QuitGame`, no
main menu to go to.

**There is no main menu level at all.** Enumerated every `World` asset under `/Game/` via
`AssetRegistryHelpers` (not a filename guess - the actual asset registry): `L_Tutorial_Island`,
`L_CombatArena` (+ `_Hills`), `L_Hamlet_01/02/03/T1`, `L_LevelGen_Scratch`, plus a pile of vendor demo
maps (Dreamscape, VFX packs). Nothing named or shaped like a menu. "Exit to the main menu" has no
destination to build toward yet - it would be a new, minimal level (or the tutorial map's own opening
state used as a stand-in) plus a new widget, not a redirect to something already there.

## Evaluate

**Two independently-sized pieces of work bundled in Michael's ask, and they should probably stay
bundled for exactly the reason he gave: closing the loop means both together, or the panel just says
"the raid is over" and still goes nowhere.** Sizing them separately so a future session can scope
correctly:

- **Persistence (small):** one new call site in `UGSPlayerHUDWidget::HandleRaidEnded` (or better,
  `UGSRaidDirector::EndRaid` itself, so it works even with no HUD watching) that reads
  `ScoreSys->GetTotal()`, compares against `SaveGame->BestScores.FindOrAdd(MapName)`, updates and
  calls `GameInstance->SaveGame()` if it's a new best. The hard infrastructure (the `USaveGame`
  class, the slot, the load-on-Init) is DONE and just needs a caller. Genuinely small.
- **Closing the loop (larger, and the part with real unknowns):** fix `EndPanel`'s visibility to
  something clickable, add Restart/Quit (and eventually Main Menu) buttons to `WBP_GSPlayerHUD`'s
  `EndPanel`, wire them to `UGameplayStatics::OpenLevel` (restart = reload the current map fresh;
  quit = whatever "main menu" ends up being). The main menu level itself is new content work, not a
  wiring fix - a blank level with a widget and two buttons (New Raid / Quit) would satisfy the literal
  ask without inventing a bigger menu system than a tutorial slice needs.

**Not verified, because nothing was built this pass:** whether `WBP_GSPlayerHUD`'s actual widget
graph already has button widgets sitting under `EndPanel` unused (the C++ header only binds text
widgets, but the Blueprint asset itself hasn't been opened/read - a real possibility given #049's
history of the panel existing before the C++ side caught up to it, per the code comment at
`GSPlayerHUDWidget.cpp:1276`). Worth checking before authoring new buttons from scratch.

## Refine

**Open questions for Michael - do not guess these:**

1. **Persistence scope.** `BestScores` is a per-map high score, not a running meta-currency. Does
   "bank the points between sessions" mean exactly that (a best-score record, survives an
   editor/game relaunch), or is he expecting loot to accumulate into something spendable later
   (an economy) - the `GSSaveGame.h` header comment ("post-slice economy landing pad - unused during
   the slice") explicitly says that second thing was deliberately deferred project-wide. If he wants
   accumulation, that is a bigger design conversation than wiring the existing best-score seam.
2. **What "restart the map" resets.** Reload `L_Tutorial_Island` fresh with the score subsystem
   zeroed (since it's a `UWorldSubsystem`, this happens automatically on level reload) and only the
   new best gets banked to the save file, or something else?
3. **What "main menu" needs to be, for a tutorial slice.** A real menu level (New Raid / Quit, maybe
   Options) has to be authored from nothing - there is no existing level or widget to repoint. Is a
   minimal placeholder (a blank level, two buttons) acceptable for now, or does this want to wait for
   real menu art/design?
4. **Whether the EndPanel already has button widgets in the Blueprint that the C++ just isn't
   binding to yet** - genuinely unknown without opening `WBP_GSPlayerHUD` in the editor, flagged above
   rather than assumed either way.

Recommended next step once Michael answers: fix `EndPanel`'s visibility first (one-line, unblocks
manual testing of everything else), open `WBP_GSPlayerHUD` to check for pre-existing button widgets,
then implement persistence (small, self-contained) and the restart/quit wiring (needs the level/menu
answer from question 3) as two separate follow-up tickets rather than one - they have different risk
profiles and don't need to land in the same build.

> 2026-08-31T01:44Z Superseded by #385 (save/gold/XP persistence + raid loop close already implemented and verified live in PIE). Stale 9.9h, confirmed dead with Michael 2026-08-30.
