---
id: 261
title: Delete GetFireCooldownRemaining - its only caller was the draw gate removed in 258
agent: claude-acf
status: done
claimed: 2026-08-23T21:55Z
build: done
waiting_on:
evaluated: 2026-08-23T21:55:57Z
observed: UNOBSERVED 2026-08-23T21:55:58Z - The function had no callers, so its removal cannot change behaviour. Evidence is a clean build after a whole-module grep confirmed nothing referenced it.
scenario: none - never run
files: 
  - Source/GoblinSiege/Weapons/Abilities/GSGA_BowShot.h
  - Source/GoblinSiege/Weapons/Abilities/GSGA_BowShot.cpp
---
## Goal

Michael: "delete the unused code."

## Generate

Removed `UGSGA_BowShot::GetFireCooldownRemaining` - declaration, doc comment and definition. It was
added in #241 for one purpose: gating the bow draw on the fire interval. #258 removed that gate after
Michael found it suppressed the timing bar entirely on a quick second press, leaving the accessor
with no callers.

## Evaluate

Verified unreferenced across the whole module before deleting (`grep` over `Source/`, 3 hits, all
three inside `GSGA_BowShot` itself). Build succeeded in 00:21 afterwards, which is the real check -
a `UFUNCTION(BlueprintPure)` could also have been called from a Blueprint graph, and that would have
surfaced as a compile or load error rather than silently.

Not runtime-observed, and there is nothing to observe: the function had no callers, so its removal
cannot change behaviour.

## Refine

Nothing. Worth recording that this is the second piece of #241 to be undone - the retargeter it
created was deleted as broken and redundant, and now its accessor goes too. Both existed to serve
decisions that measurement later reversed.
