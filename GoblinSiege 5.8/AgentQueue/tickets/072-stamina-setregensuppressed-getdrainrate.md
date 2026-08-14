---
id: 072
title: "Stamina: SetRegenSuppressed + GetDrainRate so the pool can freeze on the wall"
agent: claude-climbrebuild
status: done
claimed: 2026-08-07T18:05Z
build: required
waiting_on:
evaluated: 2026-08-07T18:12Z
files: 
  - Source/GoblinSiege/Characters/GSStaminaComponent.h
  - Source/GoblinSiege/Characters/GSStaminaComponent.cpp
---

## Goal

Give `UGSStaminaComponent` a way to hold the pool still, so climbing can implement Michael's ruling
that hanging on a wall is free but not restful.

## Generate

Three additions, all self-contained:

- **`SetRegenSuppressed(bool)`** / `IsRegenSuppressed()` - freezes the pool: no drain, no regen.
- **`GetDrainRate()`** - lets a caller tell "idle on the wall" from "not climbing" without tracking
  that state a second time.
- A `bRegenSuppressed` guard in `TickComponent`, placed **after** `SecondsSinceSpend += DeltaTime` so
  suppression does not touch the regen delay timer - releasing the wall resumes from wherever the
  delay had got to rather than restarting it and charging you twice.
- `ResetToFull()` clears the flag. Dying mid-climb would otherwise leave the pool frozen for the rest
  of the raid with nothing on screen to explain why stamina stopped coming back.

**Why `SetDrainRate(0)` could not do this job**, which is the whole reason the function exists: the
component's own header states that zero drain "lets regen resume". Hanging would slowly refill you,
every building becomes climbable in stages, and the meter stops gating verticality at all.

Not replicated. The server owns the pool and drives the number; a client guessing at suppression
would only diverge between corrections. Consistent with how `DrainRate` is already handled.

## Evaluate

**NOT COMPILED.** The build gate is closed by **#069 (claude-horde)**, which Michael confirms is
live. Nothing here is proven until it builds.

**Nothing calls it yet.** This is deliberate - the caller is the Blueprint stamina migration (plan
stage 1c), which was blocked precisely because this function did not exist. Shipping the primitive
first means the migration lands in one pass instead of leaving Blueprint and C++ both owning the
pool, which is the two-sources-of-truth race the plan warns about. **A tuning field with no reader
has shipped twice on this project; this is a function with no caller and the same rule applies -
if stage 1c does not land, this should be removed rather than left as decoration.**

**Risk to the build it rides in on:** low. One bool, one early-return, two accessors, no signature
changes, no reflection changes beyond three new `UFUNCTION`s. It does not touch `GSPlayerCharacter`
or `GSCharacterBase`, which is where #069 is working.

**I flagged the build-window collision rather than staying quiet about it.** #069 holds
`GSPlayerCharacter.h/.cpp` and `GSCharacterBase.h/.cpp` and is actively editing them; building mid-edit
compiles whatever state that session happens to be in. `AGENT_STATE.md` records exactly this failure
from 2026-08-04. The decision is Michael's, but it should be a decision rather than an accident.

**Owed AGENT_STATE.md** - DECISION (2026-08-07): stamina FREEZES while attached to a wall - zero
drain and zero regen. Hanging is free but not restful, so a tall building stays one commitment.

## Refine

- **Put the guard after the delay-timer increment, not before.** The obvious placement is an early
  return at the top of the tick, which would also freeze `SecondsSinceSpend` - so letting go of a
  wall would restart the regen delay and punish the player twice for the same climb.
- **Cleared the flag in `ResetToFull()`.** A frozen pool that survives a respawn is a silent,
  unexplainable bug: the bar simply stops refilling and nothing on screen says why.
- **Wrote the primitive rather than the whole migration.** Stage 1c also wants the vault/mantle costs
  and the HUD repoint, but those are Blueprint work and the editor is down for this build. Splitting
  at the C++/Blueprint boundary keeps this ticket to what actually needs compiling.
- **Left it uncalled on purpose, and said so.** The alternative was to wire a caller in Blueprint
  afterwards and pretend this ticket was complete.

**Deliberately left undone:** every caller. The Blueprint migration (drain rates, vault/mantle
`TryConsume`, deleting the BP `SprintStamina` float and its `NOT PlayerIsClimbing` regen gate, and
repointing `WBP_GSPlayerHUD` at `GetStaminaNormalised()`) needs the editor back.
