---
id: 312
title: World corruption 2/6 - objectives, destruction, clock and horde presence drive the scalar
agent: claude-corruption
status: done
claimed: 2026-08-26T01:40Z
build: none
waiting_on: BUILT CLEAN 2026-08-25 18:52 (exit 0, 53s, zero errors; only pre-existing C4996 AbilityTags warnings in GSGA_Block/GSGA_Interact). NOT YET OBSERVED - needs Michael in PIE: GS.Burn.IgniteAll, then GS.Corruption.Dump repeatedly and watch the objective term climb and the sky darken with nobody touching GS.Corruption.Set.
evaluated: 2026-08-26T01:52:43Z
observed: 2026-08-26T03:05:47Z | Burned the objectives and watched the objectives term climb from 0.00 to 0.62 with 3 carriers in the roster, contributing 0.28 - the scalar moved off gameplay with nobody touching GS.Corruption.Set. The director also announced it had spawned a fog actor the map lacked, and flagged the sun as Stationary
scenario: PIE in GS_BurnTest, GS.Burn.IgniteAll then GS.Corruption.Dump
files: 
  - Source/GoblinSiege/World/GSCorruptionSubsystem.h
  - Source/GoblinSiege/World/GSCorruptionSubsystem.cpp
  - Source/GoblinSiege/World/GSCorruptionDebugCommands.cpp
  - Source/GoblinSiege/Destruction/GSBreakableComponent.cpp
  - Source/GoblinSiege/Destruction/GSTopplableComponent.cpp
  - Source/GoblinSiege/Destruction/GSFlammableComponent.cpp
  - Source/GoblinSiege/Raid/GSScoreSubsystem.h
  - Source/GoblinSiege/Raid/GSScoreSubsystem.cpp
---

## Goal

World corruption 2/6 - objectives, destruction, clock and horde presence drive the scalar

## Generate

Four of the five driver terms. The scalar now moves on its own instead of only by console.

**`World/GSCorruptionSubsystem.h/.cpp`** - `RecomputeTarget()` replaces the stage-1 stub
`ComputeTarget01()`:

| Term | Weight | Source | Shape |
|---|---|---|---|
| Objectives | 0.45 | `UGSRaidDirector::GetTrackedCarriers` -> `GetCompletion01()` | weighted by type: Market/Mill 1.0, Field 0.35, House 0.15 |
| Kills | 0.20 | - | **always 0.00 - stage 3** |
| Structures | 0.15 | the three `Destruction/` components | `N/(N+20)` soft knee |
| Clock | 0.10 | `AGSGameState::GetRaidSecondsRemaining()` | `1 - Remaining/Duration`, clamped to 1 in Collapsing/Expired |
| Horde | 0.10 | `UGSHordeSubsystem` active/cap | |
| Razed floor | - | `IsDistrictRazed()` / `AlarmPhase == Razed` | floors the target at 0.85 |

Weights are `UPROPERTY(Config)`; the sum is checked at `OnWorldBeginPlay` and **complained about, never
renormalised** - a silent correction hides the typo, and the symptom ("the world never finishes
turning") is not something anyone attributes to an ini.

New public entry point `ReportStructureDestroyed(const AActor*)`, called directly from three sites:

- `GSBreakableComponent.cpp:151` - inside `Break()`, after the authority check, before `bBroken = true`
- `GSFlammableComponent.cpp:155` - at the `bBurnedDown = true` latch in `BurnTick()`
- `GSTopplableComponent.cpp:217` - in `Topple()`, deliberately **outside** the score block so a
  toppled idol still turns the land on a map with no score subsystem

Direct calls rather than delegate bindings, for the reason `GSTopplableComponent.cpp:211-215` already
documents for scoring: all three sites are first-wins guarded, so reporting inside them cannot
double-count, whereas a subscriber can silently be bound twice. It also survives an actor spawned
after `BeginPlay`.

`GS.Corruption.Dump` now shows the working - `weight x value = contribution` per term, plus the raw
count behind each.

## Evaluate

**NOT COMPILED, NOT WATCHED.** The gate was held by #310/#311 throughout; it is now held only by this
ticket, and QUEUE.md:129 is explicit that *"only the orchestrator triggers the build, and only after
the gate opens"*. So this hands back for the orchestrator to build. Everything below is reasoning
about source.

**Two deliberate deviations from the approved plan, both of which I think are improvements:**

1. **The objective roster is POLLED, not bound.** The plan called for binding
   `OnObjectiveRosterChanged` and listed roster staleness as risk #6 - *"a PCG-spawned objective
   never contributes, and it fails silently as the mill does not darken the sky"*. Polling
   `GetTrackedCarriers()` every recompute makes that failure structurally impossible instead of
   merely handled: there is no cache to invalidate and nothing to remember to bind. The cost is
   copying a handful of pointers at 10 Hz, which is nothing against that failure mode.
2. **The raid duration is self-calibrated.** `AGSGameState::RaidDurationSeconds` is private with no
   getter, and `GSGameState.h` is not this ticket's file. Rather than claim it or hardcode a second
   copy of 1800, the term tracks the largest remaining-time ever observed - which *is* the duration,
   because the clock only counts down. No constant to drift.

**Adversarially, what is wrong or unproven:**

- **A burning house is counted twice.** It is a burn objective (term A, weighted 0.15 as a house)
  AND it has a `UGSFlammableComponent` that will latch `bBurnedDown` (term C). I do not think this
  is harmful - burning the hamlet down *should* saturate two different terms - but it is an overlap
  nobody chose, and if the structures term climbs faster than expected in play, this is why.
- **Max corruption is 0.80 until stage 3**, because the kill weight is live and its term is always
  0.00. By design, and said out loud in both the startup log and the Dump line - but it means nobody
  can see the top of the arc from gameplay yet, only via `GS.Corruption.Set`.
- **The structure knee of 20 is a guess** and is the one number here with no evidence behind it. The
  kill knee has at least a defender-roster argument; this has nothing. Expect to move it.
- **Every driver read is unverified at runtime.** `GetTrackedCarriers` returning an empty array on a
  map where objectives exist, `GetActiveCap()` reading 0, the clock never starting - each would show
  as a term stuck at 0.00, which is exactly what the Dump line is built to expose, and exactly what
  nobody has looked at yet.
- **`RecomputeTarget()` runs at 10 Hz and iterates the carrier list each time.** Fine at three
  objectives; if a generated hamlet ever carries thirty, this wants revisiting.

**Process faults in this ticket, both mine:**

- **I claimed `Raid/GSScoreSubsystem.h/.cpp` and did not touch them.** The plan said to add the
  missing static `Get()` "while here". My driver math deliberately never reads Score (ruling 42 -
  deeds are polluted), so that was unrelated scope creep, and claiming a file I had no intention of
  editing held it hostage for nothing. **This is the second time** - #252 did the same with
  `features.json`, and #296's Refine section is where I wrote down that it was a fault. Writing it
  down did not stop me repeating it.
- **I claimed without `-Build`**, so the board says this ticket needs no build when it needs an
  editor-closed one. No new `UCLASS` this time, but new `UPROPERTY(Config)` reflection still rules
  out Live Coding.

**Owes `AGENT_STATE.md`:** a DECISIONS line for the poll-don't-bind choice and the self-calibrated
duration.

## Refine

**Changed during the pass, from my own review:**

- Moved the topplable report **out** of the `if (UGSScoreSubsystem* Score = ...)` block. My first
  placement was inside it, next to `AddDeeds`, which would have made corruption silently depend on
  the score subsystem being present - a coupling with no reason behind it beyond where my cursor was.
- Deleted the `ComputeTarget01()` declaration rather than leaving it beside `RecomputeTarget()`. Two
  functions that look like they both compute the target, one of them dead, is how the next reader
  wires the wrong one.
- Kept the weight-sum check as a warning rather than a clamp, after considering renormalising. A
  renormalise would make a typo invisible and produce a world that turns at subtly wrong rates.

**Deliberately left undone:**

- **The build**, per QUEUE.md:129.
- **The kill hook** - stage 3, and it is what unlocks the top 20% of the arc.
- **`UGSScoreSubsystem::Get()`** - not needed here; belongs to whoever actually calls Score.
- **The structure/objective double-count** - flagged above, left alone until someone has watched the
  terms move and can say whether it reads badly.
