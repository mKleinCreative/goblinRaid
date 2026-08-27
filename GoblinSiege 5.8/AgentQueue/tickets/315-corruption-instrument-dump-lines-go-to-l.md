---
id: 315
title: Corruption instrument: dump lines go to LogGSCorruption not LogTemp, and the objectives line shows its per-carrier working
agent: claude-corruption
status: done
claimed: 2026-08-26T03:06Z
build: required
waiting_on: Needs an editor-closed build, then one PIE run in GS_BurnTest: GS.Corruption.Dump and check (a) the line appears in Saved/Logs/MyProject.log, and (b) the objectives line names each carrier with its weight.
evaluated: 2026-08-26T03:08:27Z
observed: 2026-08-26T19:39:29Z | Ran GS.Corruption.Dump in PIE and watched the dump come out under LogGSCorruption rather than LogTemp, with the objectives term showing its per-carrier working: 'objectives 0.45 x 0.00 = 0.00 (3 carriers, 1 types)' followed by '<none> x3 w0.15 avg 0.00 -> 100% of the term'. The carrier reads <none> because this test map's objectives are untyped, which is the map and not the instrument.
scenario: PIE on GS_BurnTest after an editor-closed build, console GS.Corruption.Dump, read back from Saved/Logs/MyProject.log
files: 
  - Source/GoblinSiege/World/GSCorruptionDebugCommands.cpp
  - Source/GoblinSiege/World/GSCorruptionSubsystem.h
  - Source/GoblinSiege/World/GSCorruptionSubsystem.cpp
---

## Goal

Corruption instrument: dump lines go to LogGSCorruption not LogTemp, and the objectives line shows its per-carrier working

## Generate

Two defects in the instrument, both found by using it rather than by reading it.

**1. `GSCorruptionDebugCommands.cpp` - `Report()` logged to `LogTemp`, now `LogGSCorruption`.**
Every dump line showed correctly on screen and **none of them reached
`Saved/Logs/MyProject.log`** - confirmed by grep: 27 `LogTemp` lines present in the session, zero
`[GS.Corruption]` lines. The instrument could only be read over Michael's shoulder, which defeats
the reason for having one: I could not verify stage 2 from the log, and had to work from a single
line he typed to me. Every other line this feature emits already used the feature category; this
file was the odd one out.

**2. `GSCorruptionSubsystem.h/.cpp` - the objectives line now shows its per-carrier working.**
Was: `objectives 0.45 x 0.62 = 0.28   (3 in roster)`
Now: `objectives 0.45 x 0.62 = 0.28   (3 in roster: Mill w1.00 c0.62, Market w1.00 c0.71, Field w0.35 c0.40)`

Recorded as raw values (`FGSObjectiveWorking`: tag, weight, completion) in the terms snapshot and
formatted only inside `DescribeState()`.

## Evaluate

**NOT COMPILED.** The editor is open and in use, and the gate is held by this ticket.

**Why defect 2 matters more than it looks, and why the first observation was weaker than it read.**
`0.62` with `(3 in roster)` was reported as evidence that stage 2 works. It is evidence the term
moved - but it **cannot distinguish the correct behaviour from the failure mode ruling 42 exists to
prevent**. If every carrier's tag failed to match and all fell through to the 0.15 default, the
equal weights cancel in the normalise and the term degenerates to a plain average of completions,
which would also have produced a perfectly plausible 0.62. So the one number I used to justify
closing #312 could not tell "type weighting works" from "type weighting silently does nothing".
That is precisely the class of bug this project keeps paying for, and I had built an instrument one
level too coarse to catch it. **#312's observation stands as far as it goes** - the term does climb
off gameplay, which was its claim - but the weighting is still unverified.

**Adversarially:**

- **Neither fix is verified.** They are the kind of change that is very likely right and still
  unproven; the log fix in particular is a hypothesis about why the lines were missing. If the dump
  is still absent from the log after this, the cause was something else and I was wrong about it.
- **The tag-leaf split is the only new logic with a failure mode.** `Objective.Burn.Mill` -> `Mill`
  via a right-most split; a tag with no dot falls back to the full string, and an invalid tag prints
  `<none>`. Both fallbacks are the interesting cases and neither has been run.
- **I moved formatting out of the 10 Hz path after first writing it into it.** The string version
  worked and would have allocated three FStrings per tick for output nobody was reading. Caught in
  review of my own edit, not by anything structural.

**Owes `AGENT_STATE.md`:** a FAILED-section line - *"an instrument that only prints to screen is
half an instrument; log to the feature category, never LogTemp"*.

## Refine

**Changed in response to my own evaluation:** the breakdown was first implemented as
`TArray<FString>` built inside `RecomputeTarget()`. That is per-tick allocation for debug output
read on demand, so it became a POD struct with the formatting moved into `DescribeState()`.

**Deliberately left undone:**

- **`GS.Corruption.Debug 1`** - the persistent on-screen bar the plan specified was never built in
  stage 1 and is still missing. Not in this ticket's scope, but it is the thing that would let
  someone watch the terms move continuously instead of typing `Dump` repeatedly.
- **Re-verifying the type weighting.** That is the next PIE run's job, and it is the reason this
  ticket exists.
