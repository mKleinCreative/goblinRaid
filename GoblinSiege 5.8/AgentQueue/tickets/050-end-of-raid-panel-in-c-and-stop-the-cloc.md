---
id: 050
title: End-of-raid panel in C++, and stop the clock when the raid ends
agent: claude-hud
status: done
claimed: 2026-08-06T21:56Z
build: required
waiting_on:
evaluated: 2026-08-06T22:00:49Z
files: 
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.h
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.cpp
  - Source/GoblinSiege/Core/GSGameState.h
  - Source/GoblinSiege/Core/GSGameState.cpp
  - Source/GoblinSiege/Raid/GSRaidDirector.cpp
  - Content/UI/WBP_GSPlayerHUD.uasset
  - GoblinSiege 5.8/AGENT_STATE.md
  - decision-queue-kanban.html
---

## Goal

End-of-raid panel in C++, and stop the clock when the raid ends

## Generate

Acting on #049's finding. **First, a correction to it.**

#049 reported "all five HUD BlueprintImplementableEvents are unimplemented" and let that stand as
five holes. Four of them are not holes: `UGSPlayerHUDWidget` already writes health, the objective
list, the clock, lives and the alarm to bound text widgets in C++. `OnLivesChanged` and friends are
*enrichment hooks over* that - unimplemented, they cost a nicer display, not the display.
**`OnRaidEnded` was the only output of the raid loop with no C++ behind it at all.** Corrected in
AGENT_STATE rather than left as a more impressive-sounding finding.

**1. End-of-raid panel, C++-driven.** `UGSPlayerHUDWidget` gains `EndPanel` / `EndTitleText` /
`EndDetailText` (`BindWidgetOptional`), hidden in `NativeConstruct`, filled and shown by
`HandleRaidEnded`:

| result | title | detail |
|---|---|---|
| Extracted | EXTRACTED | You made it through the portal. |
| LeftBehind | LEFT BEHIND | The portal collapsed without you. |
| OutOfLives | OUT OF LIVES | The warband is spent. |

Plus `N of M objective types burned`, read live off the director, so a loss still reports what was
achieved. The Blueprint `OnRaidEnded` event still fires afterwards - a real end screen can replace
this wholesale; the point is that the game can say you lost without one having been authored.

`EndPanel` is deliberately **not** in the startup `WarnIfUnbound` list: it only matters when a raid
ends, and `HandleRaidEnded` warns there instead, where the message is actionable.

**2. The clock stops.** New `AGSGameState::StopRaidClock()` sets a replicated `bRaidClockHalted`
that `TickRaidClock` early-outs on, called from `UGSRaidDirector::EndRaid`. Halts rather than forcing
a phase: the phase stays truthful (a raid lost with 29 minutes left really was `Running`), and the
HUD freezes at the time it stopped. Forcing `Expired` would make every loss claim the portal
collapsed, which is a different ending.

**3. Widgets added to `WBP_GSPlayerHUD`** over MCP - a `VerticalBox` named `EndPanel` centred on the
canvas with the two text blocks inside. Verified present off a fresh load.

## Evaluate

**NOT COMPILED** - the editor is open for the widget work. The three widgets exist and are saved; the
C++ that drives them does not exist in the DLL yet, so **right now the panel is inert content**.

**What #049 proved and this has not re-proved:** both lose paths fire. This ticket changes what
happens next, and that has not been observed once. The specific things to check after a build:
- the panel appears at all (it is `BindWidgetOptional`; a name mismatch fails silently, hence the
  warning in `HandleRaidEnded`)
- the clock actually freezes - two `GS.Raid.Status` calls after an `OutOfLives` should now show the
  same number, where #049 saw 1769 then 1762
- `Extracted` still ends cleanly, since `StopRaidClock` now runs on the win path too and that path
  was previously verified without it

**Things I chose that are not obviously right:**
- **The panel is `HitTestInvisible`.** So it cannot host a button. That is correct for a text panel
  and wrong the moment anyone wants "Retry" on it - at which point the visibility and the
  `WarnIfUnbound` exemption both need revisiting.
- **Nothing stops the player after a loss.** Input is still live, the pawn still walks, the world
  still runs. The panel says OUT OF LIVES over a game you can keep playing. Pausing, disabling input
  or restarting are all end-of-raid *policy*, and I have deliberately not invented policy - but the
  result is that this fixes the silence, not the soft-lock-shaped hole behind it.
- **`bRaidClockHalted` is one-way.** Nothing un-halts it, because nothing restarts a raid. If restart
  ever lands it must clear this, and `StartRaidClock`'s `NotStarted` guard will not do it.

**Touched outside the goal:** none - all eight files claimed. The AGENT_STATE edit is the #049
correction described above.

## Refine

- **Corrected #049 rather than building on it.** "Five dead events" was the more striking version and
  it was wrong; four of them have working C++ underneath. Had I not checked, this ticket would have
  implemented four Blueprint events that duplicate text the widget already writes.
- **Gave `OnRaidEnded` a C++ fallback while keeping the BP event.** The header says what a lose screen
  looks like is not a C++ decision, and that is still true - but "no decision has been made" had
  become "the game cannot tell you that you lost". The fallback is the floor, not the ceiling.
- **Halted the clock instead of forcing `Expired`.** Setting the phase was one line shorter and would
  have made a raid lost at 29:00 report the same terminal phase as one where the portal collapsed -
  destroying the distinction #049 had just established between the two lose paths.
- **Replicated the halt flag.** Server-only would freeze the clock on the host and leave every client
  counting down past the end of a raid.
- **Left `EndPanel` out of the startup unbound-warning list.** Six warnings at load about missing art
  is a wall people stop reading; one warning at the moment it matters is not.

**Deliberately left undone:** end-of-raid policy (pause / input lock / restart) - it is a design
decision and this ticket is about the game being able to speak at all; styling the panel; and a
restart path, which is what `bRaidClockHalted` will need to clear.
