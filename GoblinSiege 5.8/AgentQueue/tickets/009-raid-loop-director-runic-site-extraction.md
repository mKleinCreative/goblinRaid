---
id: 009
title: Raid loop: director, runic site, extraction; fix player spawning inside geometry
agent: claude-raid
status: blocked
claimed: 2026-08-05T21:05Z
build: required
waiting_on: 008 - build gate closed. Spawn-in-geometry fix is written but uncompiled and unverified.
files: 
  - Source/GoblinSiege/Raid/GSRunicSite.cpp
  - Source/GoblinSiege/Raid/GSRunicSite.h
  - Source/GoblinSiege/Core/GSGameMode.cpp
  - Source/GoblinSiege/Core/GSGameMode.h
  - Source/GoblinSiege/Raid/GSRaidDirector.cpp
  - Source/GoblinSiege/Raid/GSRaidDirector.h
  - Source/GoblinSiege/Raid/GSRaidLibrary.cpp
  - Source/GoblinSiege/Raid/GSRaidLibrary.h
  - Source/GoblinSiege/Raid/GSRaidMarker.cpp
  - Source/GoblinSiege/Raid/GSRaidMarker.h
  - Source/GoblinSiege/Raid/GSRaidTypes.h
  - Source/GoblinSiege/Destruction/GSBurnObjectiveBase.cpp
  - Source/GoblinSiege/Destruction/GSBurnObjectiveBase.h
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.cpp
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.h
  - Source/GoblinSiege/Combat/GSGameplayTags.cpp
  - Source/GoblinSiege/Combat/GSGameplayTags.h
  - Content/Maps/L_Tutorial_Island.umap
---

## Goal

Raid loop: director, runic site, extraction; fix player spawning inside geometry

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
