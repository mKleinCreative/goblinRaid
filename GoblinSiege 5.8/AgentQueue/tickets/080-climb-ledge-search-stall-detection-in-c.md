---
id: 080
title: "Climb ledge search + stall detection in C++ (UGSClimbLibrary), with a real log line"
agent: claude-climbrebuild
status: done
claimed: 2026-08-08T20:20Z
build: done
waiting_on:
evaluated: 2026-08-08T20:39:58Z
files: 
  - Source/GoblinSiege/Characters/GSClimbLibrary.h
  - Source/GoblinSiege/Characters/GSClimbLibrary.cpp
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
---

## Goal

Michael: *"let's go ahead and build and see if that resolves any of the issues you've been facing,
because if you read it again, you'll see that I got stalled, again."*

He stalls at **Z 1899.9 on the south face of Medium_11**, every attempt, four sessions running.

## Generate

**New: `UGSClimbLibrary` (Blueprint function library).**

- **`FindClimbLedge(Climber, WallNormal) -> FGSClimbLedgeResult`** - sweeps a 25uu sphere downward
  at insets **80..340 step 20**, accepts the first surface that is walkable (`ImpactNormal.Z >=`
  the character's own `GetWalkableFloorZ()`) **and** has open sky above it. Returns the deck, the
  inset, the rise, the normal, and counts of what was rejected and why.
- **`IsClimbBlockedUpward(Climber, RiseDistance=45)`** - stateless "can the capsule rise right now",
  using the capsule's *current* scaled dimensions (the climb shrinks it to radius 24).
- **`GS.Climb.LogLedge 1`** - logs `GSDBG|LEDGE|found=..|inset=..|rise=..|nz=..|steep=..|interior=..|empty=..`
  on every search. Enabled.

**Blueprint rewiring** (`ClimbStaminaExits`): exec is now head-trace -> `IsClimbBlockedUpward` ->
`FindClimbLedge` -> accept branch. `bFound` drives `FCABE512.A`; `Deck` sets `ClimbLedgeLocation`;
`IsClimbBlockedUpward` replaces `ClimbBlockedSeconds > 0.10` on the OR that bypasses the head guard.
The Blueprint `FindClimbLedge` function written earlier today is **deleted**. 772 nodes,
`UP_TO_DATE`, saved.

Build succeeded in 4:57. Reflection confirmed by grepping `GSClimbLibrary.gen.cpp` (21x
`FindClimbLedge`, 22x `IsClimbBlockedUpward`) rather than asking the editor, which reports the DLL
it started with.

## Evaluate

**Why this is C++ and not more Blueprint.** The search is a loop with an early exit. Expressing it
in Blueprint cost me three separate silent-failure traps in one session, all recorded in #079: a
multi-output `bind` with the wrong arity produced a graph that compiled clean and read the wrong pin;
`write_graph_dsl` appended instead of replacing, leaving two function bodies; and `read_graph_dsl`
rendered the broken and the correct graph identically, so I nearly rewrote a working function.

**The decisive reason, though, is that I could not see the failure.** Three consecutive play tests
were spent inferring the state of `ClimbBlockedSeconds` from position deltas. In the last log it sat
at **0.000 for 1285 ticks** while the character was demonstrably jammed with input held - both of its
conditions satisfied for 7 consecutive ticks - and I still cannot say why from outside. Replacing a
multi-frame accumulator with a single-frame capsule sweep removes the state entirely, and the new
`GSDBG|LEDGE` line reports the decision instead of leaving it to be reconstructed.

**Measured before writing any of it** (Python, real collision, 56 roof lips across 14 houses):

| approach | roof lips resolved |
|---|---|
| fixed inset 120 (was shipped) | 46 / 56 = 82% |
| fixed inset 80 (original) | 49 / 56 = 88% - the ceiling for any constant |
| **search 80..340** | **54 / 56 = 96%** |

**PLAYED AND CONFIRMED.** Michael: *"it works great."* The log backs it: 132 ledge searches, 2
returned `found=true`, both `inset=80 nz=0.53 rise=+220`; the climbs that ended at Z~1500 carried to
**1952 and 1978** after release, and a later climb *starts* at **Z 2007** - above the 1899.9 wall that
stopped every attempt for four sessions. The stall is closed.

**The new log line did its job on the first run.** The failed searches read
`steep=0-3 | interior=1-4 | empty=10-11` - i.e. below a lip most insets hit nothing at all, and the
few that do hit find interior floors. That is a sentence I could not have written from the old
instrumentation at any price.

**A correction I owe this ticket.** I claimed the frozen stamina in the last log was evidence that
`ClimbTick` had stopped executing. It was not - #076 shipped "stamina freezes on the wall" and
constant stamina while climbing is the intended behaviour. I also blamed my own `FindClimbLedge`
for halting the exec chain; its `Completed` pin was correctly wired and both returns were reachable.
Two wrong diagnoses in one turn, both from reasoning about behaviour instead of measuring it.

**Owed AGENT_STATE.md** - DECISION (2026-08-08): ledge detection is a SEARCH over inset depth, not a
tuned constant, and it lives in C++ (`UGSClimbLibrary`). No single inset exceeds 88% on this art pack.

## Refine

- **Read the walkable limit off the character** (`GetWalkableFloorZ()`) instead of hardcoding 0.4695.
  That number is derived from `WalkableFloorAngle 62`; hardcoding it would silently drift the first
  time the angle is tuned. The constant survives only as a fallback when there is no movement component.
- **Used the capsule's current scaled dimensions in the block test.** The climb shrinks the capsule to
  radius 24; testing the default size would report "blocked" in gaps the real capsule fits through.
- **Rejected a zero wall normal early.** Without that, `Normalize()` fails and the probe column
  collapses onto the climber, sweeping straight down and finding the ground as a "ledge".
- **Made the result report its rejections** (`steep`/`interior`/`empty`). A bare false would put me
  straight back to guessing which stage failed - the exact loop this ticket exists to end.
- **Deleted the Blueprint `FindClimbLedge` rather than leaving it orphaned.** Two implementations of
  the same search, one unreachable, is how the next session gets misled.

**Deliberately left undone:** the old single-inset probe, its 0.4695 gate and the sky-check nodes from
#079 still execute in `ClimbStaminaExits` even though nothing reads them - two wasted traces per tick,
kept for now so the rollback stays small. The lean-out and `ClimbBlockedSeconds` accumulator are still
present and still fed. 3 of 56 lips have no walkable deck at any inset (steep roofs) and need the
separate roof-continuation feature. And the mantle itself is unproven: the rise from jam to deck is
246-375uu against a 1.13s montage, which may read as a teleport.
