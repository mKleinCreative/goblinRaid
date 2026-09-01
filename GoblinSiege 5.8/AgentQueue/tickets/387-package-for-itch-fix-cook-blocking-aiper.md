---
id: 387
title: Package for itch: fix cook-blocking AIPerceptionComponent Error log
agent: claude-package
status: done
claimed: 2026-08-31T02:10Z
build: none
waiting_on:
evaluated: 2026-08-31T15:50:59Z
observed: 2026-08-31T15:51:01Z | AIPerceptionComponent Error log line confirmed gone from every subsequent cook attempt across the rest of the packaging session (dozens of repackage cycles), never recurred.
scenario: Repeated UAT BuildCookRun cook passes throughout the overnight packaging session
files: 
  - Source/GoblinSiege/Horde/GSHordeAIController.cpp
---

## Goal

Package for itch: fix cook-blocking AIPerceptionComponent Error log

## Generate

`AGSHordeAIController`'s constructor passed `ObjectInitializer.DoNotCreateDefaultSubobject(TEXT("AIPerceptionComponent"))` to decline the perception component (the intended §3.4 "perception-less agents" optimization). Unreal 5.8 ignores that opt-out for a component the base class marks required and creates it anyway - documented in `GSAIControllerBase.cpp`'s own header comment - which logs `LogUObjectGlobals: Error: Ignored DoNotCreateDefaultSubobject...` on every `AGSHordeAIController` construction. Harmless at runtime, but UAT hard-fails a packaging cook on ANY `Error:`-level log line even when the cook itself completes. Removed the dead, non-functional call and rewrote the constructor comment to stop claiming the optimization works, since it never did in this engine version.

## Evaluate

Confirmed by rebuilding and re-running the packaging cook: the `AIPerceptionComponent` error no longer appears in the cook log, and this specific blocker never recurred across the many subsequent repackage attempts later that session (the cook went on to fail for several *other*, unrelated reasons - see ticket #388 - but never again for this one). The actual perf implication (every horde goblin genuinely does carry its own perception component, contrary to the design intent) is pre-existing and unrelated to this fix; flagged in the code comment for whoever picks up real perception-less-agent work later.

## Refine

Nothing further needed - this was a single, well-isolated dead-code removal with no cascading effects. Folded into the broader packaging effort (#388) since it was found while chasing the same "get a package to actually build" goal.
