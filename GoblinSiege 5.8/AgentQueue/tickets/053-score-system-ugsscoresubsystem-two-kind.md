---
id: 053
title: Score system: UGSScoreSubsystem two-kind tally (deeds/loot), fed by real raid events, shown on the end panel
agent: claude-score
status: done
claimed: 2026-08-06T23:13Z
build: required
waiting_on:
evaluated: 2026-08-06T23:17:16Z
files: 
  - Source/GoblinSiege/Raid/GSScoreSubsystem.h
  - Source/GoblinSiege/Raid/GSScoreSubsystem.cpp
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.h
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.cpp
  - Content/UI/WBP_GSPlayerHUD.uasset
  - GoblinSiege 5.8/AGENT_STATE.md
  - decision-queue-kanban.html
---

## Goal

Score system: UGSScoreSubsystem two-kind tally (deeds/loot), fed by real raid events, shown on the end panel

## Generate

The u=4.0 NEXT item, and the other half of "what happens when a raid ends" that #050's panel opened.

**`Raid/GSScoreSubsystem.h/.cpp` - a `UWorldSubsystem`**, matching `UGSRaidDirector` and for the same
reason: no level authoring, so a generated map gets scoring for free.

- **Deeds** fed by `AGSBurnObjectiveBase::OnBurnObjectiveCompleted`, bound by a world sweep in
  `OnWorldBeginPlay` and unbound exactly in `Deinitialize`.
- **First of a type scores 100, duplicates 40, extraction 150.** Duplicates score less rather than
  zero: the Q-37 demotion marks siblings Optional once one of a type burns, so a second field is real
  work that simply does not advance the win. Zero would tell the player it was pointless.
- **Loot exists and is always 0**, with `HasLootSource()` so the UI omits the line rather than
  printing a permanent zero. Nothing in the project has a loot value - `UGSCarryComponent` carries
  objects and no object declares what it is worth. `AddLoot` is the seam.
- Double-award guard: a re-broadcast would silently double the score and nothing downstream would
  notice a wrong number.
- `BuildSummaryLine()` lives on the subsystem, so the tally owns its own wording.

**A real constraint found while writing it.** I planned to read Required/Optional off the objective.
`AGSBurnObjectiveBase::HandleCompleted` calls `SetListState(Complete)` **before** broadcasting, so by
the time any listener sees it every objective is `Complete` and the distinction is gone. Asking
`UGSRaidDirector` instead would work but depends on whether its handler ran before ours - both bind
the same delegate and that order is not contractual. The subsystem therefore tracks **its own** set of
scored types: first of a type is full, later ones are duplicates. Self-contained, deterministic, and
it matches what the demotion pass actually means.

**End panel** gains `EndScoreText` (gold, Bold 26). If the widget is absent the score is appended to
the detail line instead - a missing widget should cost layout, never information.

## Evaluate

**NOT COMPILED.** New `UCLASS`, so it needs an editor-closed build. The widget half is saved and live;
the C++ that fills it is not.

**The honest shape of this: one of the two kinds is real.** The design doc asks for a deeds/loot
split; deeds is wired to actual events, loot is a declared seam that will read 0 forever until
something has a value. I chose that over inventing a loot economy, and over dropping loot and having
the score screen rebuilt later. It is stated in the header, in `HasLootSource`, and here - because a
counter that always reads 0 looks like a bug, and this one is a placeholder.

**Untested, all of it.** No raid has been scored. Specifically unproven:
- that the world sweep in `OnWorldBeginPlay` runs after objectives exist. The director does the same
  sweep AND has a self-announce fallback because "ordering between the two is not contractually
  fixed" (#009). **I did not add the fallback**, on the reasoning that a missed objective costs
  points rather than a broken win condition - but that is exactly the reasoning that would produce a
  silently low score, and #009 already learned this lesson on the director.
- the first-of-type rule against a real raid with two fields.
- that 100/40/150 are sensible. They are invented; nothing has played a scored raid.

**Balance is constants in the .cpp, not data.** A DataAsset is the tunable answer and is premature -
no evidence exists yet about what these should be. Gathering them in one namespace block makes the
move cheap later; scattering literals through the handlers would not.

**Touched outside the goal:** none. All seven files claimed.

**Owed AGENT_STATE.md** - the ListState-erased-before-broadcast finding, which is a trap for anything
else that wants to know what an objective WAS at completion time.

## Refine

- **Tracked scored types locally instead of reading `GetListState()`.** Discovered mid-write that the
  state is set to `Complete` before the broadcast, so the obvious implementation would have scored
  every objective as a duplicate - or every one as required, depending which way I defaulted - and
  looked correct in code review either way.
- **Rejected asking the director**, which was the next obvious source. It would have made the score
  depend on delegate binding order between two subsystems, which is the kind of coupling that works
  until someone adds a third listener.
- **Made loot's emptiness visible in the API** (`HasLootSource`) rather than commenting it. A comment
  would not stop the end screen printing "0 loot" every raid.
- **Added the double-award guard before any evidence it was needed.** Completion is meant to be
  terminal; the cost of the guard is a TSet lookup, and the cost of being wrong is a score nobody can
  reproduce.
- **Put the wording in the subsystem, not the widget.** The HUD asks for a line; if scoring changes
  what it counts, one place changes.

**Deliberately left undone:** the self-announce fallback for late objectives (flagged above as the
weakest point); loot; balance as data; and any per-type breakdown on screen - `GetDeedsForType` exists
and nothing displays it.
