---
id: 320
title: 67 houses stop swamping the objectives term: weight per TYPE not per instance, plus the ozone zero-baseline bug and the 71-carrier tick cost
agent: claude-corruption
status: done
claimed: 2026-08-26T19:01Z
build: required
waiting_on: NOT COMPILED - gate closed by 317 (stale 15.6h) and 319. Then PIE L_Tutorial_Island + GS.Corruption.Dump: the objectives block should show four TYPE rows, with House at 6 percent of the term instead of 75, and ozone scale should read non-zero as corruption rises.
evaluated: 2026-08-26T19:05:20Z
observed: 2026-08-26T23:19:21Z | Raised corruption to 1.0 in a 71-objective hamlet and watched the objectives block split into four type rows: House x67 now carries 6 percent of the term where it took 75 percent before, while the Mill and Market take 40 each. The ozone scale climbed to 0.06 instead of staying pinned at zero, and the sky and grade drove all the way to the gritty end
scenario: PIE in L_Tutorial_Island after the 16:12 build, GS.Corruption.Set 1 then GS.Corruption.Dump
files: 
  - Source/GoblinSiege/World/GSCorruptionSubsystem.h
  - Source/GoblinSiege/World/GSCorruptionSubsystem.cpp
  - Source/GoblinSiege/World/GSCorruptionDirector.h
  - Source/GoblinSiege/World/GSCorruptionDirector.cpp
---

## Goal

67 houses stop swamping the objectives term: weight per TYPE not per instance, plus the ozone zero-baseline bug and the 71-carrier tick cost

## Generate

Four fixes, all of them found by reading the log rather than the code - which is the first time this
feature's instrument has paid for itself.

**1. Objectives weight per TYPE, not per instance.** `RecomputeObjectiveTerm()` groups carriers by
tag, averages completion within each type, and weights the four type-averages.

On L_Tutorial_Island (71 carriers: 67 House, 2 Mill, 1 Market, 1 Field):

| | before | after |
|---|---|---|
| House share of the term | 10.05/13.40 = **75%** | 0.15/2.50 = **6%** |
| Mill + Market + Field | 25% | 94% |

**2. Ozone scale is now an absolute target, not a multiplier.** The log read
`ozone(0.40,0.61,0.10)x0.00` - L_Tutorial_Island authors `OtherAbsorptionScale` at **0.0**, and
`Lerp(0, 0 * 2.0, T)` is zero at every corruption level. The whole ozone half of #316 was inert on
that map and would have been on any map authoring it off. Now `Lerp(Base, 0.06, T)`.

**3. The dump prints one row per TYPE.** It printed all 71 carriers before, which is not a readout
anybody can read. Each row now shows count, weight, average completion and **the type's share of the
term** - the number that was actually wrong.

**4. The objectives term runs at 2 Hz, not 10 Hz.** It was iterating 71 carriers ten times a second
for a value that moves as fast as a fire spreads. Every other term is a couple of getters and stays
on the fast tick. `GS.Corruption.Refresh` forces an immediate recompute so the command does not look
inert, and the accumulator starts high so the first tick computes rather than reading 0.00 for half
a second.

Also corrected the file header, which still described this as "stage 1, no drivers wired".

## Evaluate

**NOT COMPILED.** Gate is closed by #317 (stale 15.6h) and #319.

**The finding behind fix 1 is the important part of this ticket.** #312 shipped per-instance
weighting and I closed it citing `objectives 0.45 x 0.62 = 0.28 (3 in roster)` as evidence it
worked. It did work - on GS_BurnTest, which has three objectives. The weighting was correct and the
*aggregation* was wrong, and no test on a three-objective map could ever have shown that. It took a
71-carrier hamlet to make it visible, and even then only because #315 had just made the per-type
breakdown printable. **Two tickets in a row have now been closed on evidence that was true but
insufficient**, and in both cases the gap was the same: a number that could not distinguish the
correct behaviour from the failure.

**Adversarially:**

- **The type-average has its own bias, in the opposite direction.** Burning ONE house out of 67 now
  moves the term by 0.15/2.50 x 1/67 - essentially nothing. That is intended, but it means the
  structures term is now the only thing that responds to razing a street, and that term is capped by
  its own soft knee. Whether "burn every house" should feel like an achievement is a design question
  I have not asked.
- **`0.06` for ozone is a guess** bounded only by the engine's UIMax of 0.2. It has never been seen.
- **2 Hz is a guess too.** If a fire completes and the sky visibly lags, this is the first suspect.
- **`FindByPredicate` is a linear scan per carrier** - 71 carriers x up-to-4 rows at 2 Hz, which is
  nothing, but it is O(n*types) and I should not pretend otherwise.
- **Nothing here is proven.** Four changes, zero runtime evidence.

**Owes `AGENT_STATE.md`:** a FAILED line - *"a term can be correctly weighted and still wrong in
aggregate; test driver maths on the map with the most objects, not the tidiest one"*.

## Refine

**Changed from my own review:** first draft used `BIG_NUMBER`, which is deprecated in 5.8 and
expands to a deprecation macro - this project already carries C4996 warnings and does not need more.
Now `UE_BIG_NUMBER`. Also moved `ObjectiveBreakdown` off the terms snapshot onto the subsystem, so
the per-tick copy no longer carries an array.

**Deliberately left undone:**

- **`ObjectiveRecomputeIntervalSeconds` is not in DefaultGame.ini.** The C++ default of 0.5 applies,
  and the ini was not in this ticket's claim - I am not editing files I did not claim, having twice
  done the reverse and flagged it.
- **Whether razing 67 houses should matter more** - a design question for Michael, not a code fix.
- **Hooking `UGSCrumbleComponent`** - #317 made it the unified destroyed state and corruption still
  listens to the three callers upstream of it. Not broken today; wrong shape tomorrow.
