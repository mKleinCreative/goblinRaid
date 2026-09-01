---
id: 386
title: Close raid loop: boot menu, complete-all debug cmd, gold/xp profile
agent: claude-loot2
status: abandoned
claimed: 2026-08-31T01:13Z
build: none
waiting_on:
evaluated:
observed:
scenario:
files: 
  - Config/DefaultEngine.ini
  - Source/GoblinSiege/Destruction/GSBurnObjectiveBase.h
  - Source/GoblinSiege/Destruction/GSBurnObjectiveBase.cpp
  - Source/GoblinSiege/Raid/GSRaidDebugCommands.cpp
  - Source/GoblinSiege/Progression/GSSaveGame.h
  - Source/GoblinSiege/Raid/GSRaidDirector.cpp
---

## Goal

Close raid loop: boot menu, complete-all debug cmd, gold/xp profile

## Generate

<!-- REPLACE: what you produced. Files touched, what each change does, the calls
you made. Delete this comment when you write the section. -->

## Evaluate

<!-- REPLACE: judge your own output against the goal, adversarially. What is
verified and by what evidence (a log line, a PIE observation, a compile result -
not "should work"); what is written but has never run; what you touched outside
the goal; the DECISION or FAILED line this owes AGENT_STATE.md. -->

## Refine

<!-- REPLACE: what you changed in response to your own evaluation, and what you
are deliberately leaving undone. "Nothing changed, and here is why the first pass
survives scrutiny" is a valid answer; silence is not. -->

> 2026-08-31T01:13Z Redundant with my own still-open #385, which already claims GSRaidDirector.cpp for this exact continuing work (menu/loop closure). Folding this work into #385 instead of running two overlapping tickets.
