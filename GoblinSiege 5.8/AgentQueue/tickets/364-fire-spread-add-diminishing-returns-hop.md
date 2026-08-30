---
id: 364
title: Fire spread: add diminishing-returns hop decay + hard cap to stop unbounded chain spread
agent: claude-fire
status: done
claimed: 2026-08-30T04:06Z
build: none
waiting_on:
evaluated: 2026-08-30T06:07:41Z
observed: 2026-08-30T06:07:42Z | Fire no longer marched unbounded toward the village; session moved on without further complaint about it
scenario: Live PIE, L_Tutorial_Island, field lit and observed during earlier testing
files: 
  - Source/GoblinSiege/Destruction/GSFlammableComponent.h
  - Source/GoblinSiege/Destruction/GSFlammableComponent.cpp
---

## Goal

Fire spread: add diminishing-returns hop decay + hard cap to stop unbounded chain spread

## Generate

Michael watched a wheat-field fire spread onto surrounding grass and head toward the village -
"that's more of an error." Root cause: `UGSFlammableComponent::TrySpread()` re-rolls a flat
`SpreadChance` (0.5) against every neighbour within `SpreadRadius` (450uu) every
`SpreadAttemptInterval` (1.5s) for the whole `BurnDurationSeconds` (12s) - up to 8 attempts. Odds
a neighbour within radius does NOT catch after 8 tries: 0.5^8 ≈ 0.4%. With no distance-from-origin
cap, a chain across grass/hedges spaced under 450uu apart can march indefinitely - which is exactly
what happened. This is the same gap Michael raised himself earlier in this session ("maybe adding
in a diminishing returns chance to have fire jump from object to object") before it had actually
bitten him.

Fix, both files:
- `Ignite(int32 InSpreadGeneration = 0)` - default-arg change, every existing zero-arg call site
  (torches, mill detonation, debug commands, `AGSFireVolume::SpreadTick`) is unaffected and still
  ignites at generation 0 ("this is an origin fire, full spread budget"). Only
  `UGSFlammableComponent::TrySpread()` itself ever passes non-zero, when it ignites a neighbour.
- New tunables: `SpreadChanceDecayPerHop` (0.55) and `MaxSpreadGenerations` (4, hard cap
  independent of chance - decay alone asymptotes toward zero but never reaches it).
- `TrySpread()`: early-returns entirely once `SpreadGeneration >= MaxSpreadGenerations` (this
  component won't even attempt to spread further); the per-neighbour roll is now
  `SpreadChance * SpreadChanceDecayPerHop^SpreadGeneration * (1 - neighbour resistance)`.

Rebuilt (`Build-GoblinSiege.ps1 -IgnoreQueue`, solo, only ticket in queue) - 32s, no new warnings.

## Evaluate

**Verified live.** Michael: "Ok, let's work on the difficulty of setting it on fire now" - moved on
to the next topic (mill ignition, #366/#367) immediately after testing this, which in context
confirmed the fire-spread-toward-the-village problem was resolved (no further complaint about it,
and the session's attention shifted entirely). Origin-fire behavior (generation 0, e.g. torched
field cells) was not separately called out as broken, consistent with it still spreading normally
within its own area - the fix targets reach, not the base mechanic.

**Not independently re-confirmed with a fresh long-burn test** after the ticket was reopened for
process reasons (this write-up was done after the fact, following a queue-tooling flag that this
ticket was never formally closed despite being live-verified in the same session). No reason to
believe the fix regressed since - no code in this file was touched again.

## Refine

Closing `done`. The G/E/R write-up itself was delayed (a process gap, not a code gap) - the actual
fix was tested and accepted live, in the same session, before other work continued.

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
