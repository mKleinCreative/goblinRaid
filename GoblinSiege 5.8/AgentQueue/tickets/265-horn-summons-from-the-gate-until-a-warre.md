---
id: 265
title: Horn summons from the gate until a Warren is down: arrival falls back to the runic site, not the treeline markers
agent: claude-warren
status: done
claimed: 2026-08-23T22:50Z
build: none
waiting_on: #264 (claude-acf) holds GSHordeSubsystem and is lower-numbered, so it has right of way. Nothing written.
evaluated: 2026-08-23T23:07:48Z
observed: 2026-08-23T23:16:50Z | The fallback chain flipped in play, logged both ways. With no Warren planted the horn summoned from the runic site: Arrival: the gate BP_GS_RunicSite_C_1 - no Warren planted yet. Twenty seconds later, with a Warren down, seven goblins each logged Arrival: the Warren BP_GS_Warren_C_0 and climbed out of the Warren instead. Reserve fell 17 to 10 across that blast, so they were real summons and not a lookup running in isolation.
scenario: PIE in L_CombatArena with a runic site Michael placed for the test and a T-planted Warren, 2026-08-23 23:15-23:16 UTC, editor build of 16:09, LogGSHorde at Verbose.
files: 
  - Source/GoblinSiege/Horde/GSHordeSubsystem.h
  - Source/GoblinSiege/Horde/GSHordeSubsystem.cpp
---

## Goal

Horn summons from the gate until a Warren is down: arrival falls back to the runic site, not the treeline markers

## Generate

`UGSHordeSubsystem::FindArrivalTransform` gains a middle fallback. The order is now:

1. **Nearest arrival-mouth Warren** (unchanged, ledger ruling 18)
2. **The gate - nearest `AGSRunicSite`** (new)
3. **`Marker.HordeArrival` markers** (unchanged, and still not dead code)

Michael's ruling, 2026-08-21: *"horn blasts working from the gate until a warren is down on the map."*
Before a Warren is planted the horde now comes out of the portal the raid arrived through - somewhere
the player has been and can find again - rather than a treeline marker they have never seen.

**`GetSpawnTransform()` and not the actor transform.** The runic site already solves "a standable
spot outside the extraction sphere, traced against the world", which is the same question being asked
here and one this project got wrong once before (players spawning inside a building, 2026-08-05).
Reusing it means the goblins cannot climb out into geometry the portal already knows to avoid.

**The marker path is untouched.** Every map predating the Warren answers the horn through it, and its
own comment says it must not be deleted.

**All three branches now log which source was chosen**, at Verbose. Until now nothing did, and on
2026-08-21 "did the goblins arrive from the Warren or the markers?" was unanswerable from the log -
which is exactly the sort of question a log should settle. This closes an instrumentation gap flagged
in #236's review two days ago.

## Evaluate

**NOT COMPILED, NOT RUN.**

**Verified:** the shape of the change only - `AGSRunicSite::GetSpawnTransform()` exists,
`GSRunicSite.h` and `EngineUtils.h` were already included in this translation unit, and the three log
lines are present. A spell-check, not a compile.

**Unrun and specifically at risk:**

- **`L_CombatArena` has no runic site**, so this new branch does nothing there and will fall straight
  through to the markers. Testing it needs a map with a gate - which is also the only place the
  feature matters.
- **The gate is chosen by straight-line distance to the summoner**, with no line-of-sight or
  behind-the-player preference. The marker branch below it goes to some trouble to avoid goblins
  appearing in the middle of frame; this branch does not, because there is normally exactly one gate
  and picking a different one is not an option. If a map ever has two, this will want revisiting.
- **No filter on whether the portal is OPEN.** Deliberate: the gate is where you came in, and the
  horde should come from it all raid, not only at the end.

## Refine

Nothing changed on review.

**Deliberately left undone:** no ordering preference between multiple gates, and no check that the
chosen gate is reachable from the summoner. Both are speculative until a map exists with more than
one runic site.

> 2026-08-23T23:07Z Gate fallback written plus arrival-source logging. Uncompiled; needs a map with a runic site to exercise.
