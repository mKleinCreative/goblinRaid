---
id: 366
title: Instrument AGSTorchProjectile::OnProjectileHit to diagnose mill exterior-ignite difficulty
agent: claude-fire
status: done
claimed: 2026-08-30T04:46Z
build: none
waiting_on:
evaluated: 2026-08-30T04:56:02Z
observed: 2026-08-30T04:56:03Z | Diagnostic logging directly captured the actual root cause (field objective intercepting hits) that led to the real fix in #367
scenario: Live PIE, L_Tutorial_Island, multiple torch throws at mill and a house
files: 
  - Source/GoblinSiege/Destruction/GSTorchProjectile.cpp
---

## Goal

Instrument AGSTorchProjectile::OnProjectileHit to diagnose mill exterior-ignite difficulty

## Generate

After three sessions of theories about the mill's exterior ignition (reach/height, then collision
detection), Michael asked for "more science" instead of another guess: added `UE_LOG(Warning, ...)`
lines to `AGSTorchProjectile::OnProjectileHit`, printing on EVERY throw: (1) what actor/component the
torch physically hit and the impact point, (2) which `AGSBurnObjectiveBase` (if any)
`FindObjectiveAtLocation` resolved for that point. Marked temporary in the code comments. Also
corrected a stale comment in the same function left over from #362 (still describing the mill as
"unreachable this way, window-only" - the actual retired rule).

Rebuilt, relaunched, Michael threw several torches at the mill and a nearby house.

## Evaluate

**Immediately decisive - not what either prior theory predicted.** The log showed every hit,
including ones that visibly landed on the mill's own tower mesh and one on an unrelated house, being
claimed by `GSFieldFireObjective_0` - not a collision miss, not a reach problem, an objective-
priority bug in `FindObjectiveAtLocation` (fixed in #367, filed immediately off this ticket's own
evidence). The instrumentation did exactly its job: turned three sessions of plausible-sounding
guesses into one log capture that pointed straight at the real cause.

## Refine

Closing `done`. The `TORCH HIT` log lines are left in place (Warning-level, one line per throw,
explicitly marked temporary/diagnostic in comments) rather than stripped immediately - cheap to
leave, and useful if this class of bug (an objective silently swallowing a hit meant for something
else) recurs. Worth a follow-up cleanup pass once nobody needs it, not urgent enough to do now.
