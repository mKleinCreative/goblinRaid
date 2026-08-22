---
id: 196
title: A toppled idol scores: OnToppled feeds UGSScoreSubsystem deeds
agent: claude-idol
status: done
claimed: 2026-08-19T03:47Z
build: none
waiting_on:
evaluated: 2026-08-19T04:09:08Z
observed: 2026-08-19T04:09:08Z | Toppling the statue awarded +100 deeds, logged in two independent PIE sessions at 03:51 and 04:08, each alongside OUTCOME: MOVED with 202 and 206 uu/s. The same log window shows burn objectives scoring 100/40/40 from a separate code path, confirming the idol sits on the same scale as a required objective.
scenario: PIE in L_CombatArena: grapple the statue, haul it over by walking back, read Saved/Logs/MyProject.log for the LogGSTopple score line
files: 
  - Source/GoblinSiege/Destruction/GSTopplableComponent.h
  - Source/GoblinSiege/Destruction/GSTopplableComponent.cpp
---

## Goal

A toppled idol scores: OnToppled feeds UGSScoreSubsystem deeds

## Generate

Michael: *"OnTopple should score, go ahead and fix that."*

Toppling a monument scored NOTHING. `OnToppled` had no subscribers anywhere in the project,
`UGSScoreSubsystem` binds only to `AGSBurnObjectiveBase`, and the only trace a felled idol left was a
single log line. The most dramatic thing a goblin can do to a settlement did not register.

`UGSTopplableComponent::Topple()` now awards deeds directly: `ToppleDeeds = 100` for the first monument
of its type, `DuplicateToppleDeeds = 40` for each after, and an `EditAnywhere` `DeedTypeTag` for the
score bucket.

**Rates match the existing scale rather than inventing one** - `GSScoreSubsystem.cpp:20-21` defines
`DeedsPerRequiredObjective = 100` and `DeedsPerOptionalObjective = 40`, and the Statue is one of the
three REQUIRED objectives in the GDD roster. The duplicate demotion is copied from the burn objectives
so a hamlet with three idols cannot out-score the entire raid.

## Evaluate

**OBSERVED in the log across two independent PIE sessions:**

```
03:51:17  'BP_Statue_Warrior_C_0' scored +100 deeds (first untagged) -> 100 total.
03:51:18  OUTCOME: MOVED - linear 202 uu/s, angular 63 deg/s
04:08:12  'BP_Statue_Warrior_C_0' scored +100 deeds (first untagged) -> 100 total.
04:08:13  OUTCOME: MOVED - linear 206 uu/s, angular 55 deg/s
```

The same log window contains burn objectives scoring `+100 first`, then `+40`, `+40` for duplicates -
an unplanned but useful control showing the idol's award sits on exactly the same scale as a required
objective, from a completely separate code path.

**Scored inside `Topple()` rather than from a listener on `OnToppled`,** deliberately. The topple is
already first-wins through the `bToppled` latch, so awarding there cannot double-count. A delegate
subscriber can be bound twice and would silently award twice - and a wrong score is precisely the class
of bug nobody notices until an end-of-raid screen looks slightly off. `HandleObjectiveCompleted` carries
a guard against exactly that, which is evidence the failure is real rather than theoretical.

**NOT verified: that the deeds reach the end-of-raid panel.** `GetTotal()` returning 100 is read back
from the subsystem in the same call that set it. The HUD reads the same subsystem so it should follow,
but nobody has watched a raid end with a toppled idol in it.

## Refine

**The deed tag ships EMPTY, and that is a compromise rather than a design choice.** The obvious value is
`Objective.Statue`, but that string is currently only an ACTOR tag - there is no gameplay tag for it,
and `GSGameplayTags.h` is held by #179, which has been at `review` for 26 hours. Declaring one there
would collide with another agent's open work.

An untagged award still scores; it only loses the per-type breakdown in `GetDeedsForType`, which is
reporting detail rather than the feature. Once #179 closes this is one property on the component.

**Deliberately not doing: wiring `AGSObjective_ToppleStatue`.** That class searches for actors by the
`Objective.Statue` actor tag and then casts to `AGSDestructibleObjective`, which `BP_Statue_Warrior` is
not - so the objective counter still will not tick for a toppled idol. Scoring and objective completion
are two separate gaps, and #189's rename of that class is itself still unclosed at review. Fixing the
objective on top of another agent's unmerged rename is how two agents collide.
