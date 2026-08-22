---
id: 188
title: level-gen: _move_building_clear rotates a house back onto the road it was escaping, burning all 3 passes and opening a sightline
agent: claude-roadpush
status: done
claimed: 2026-08-18T06:30Z
build: none
waiting_on:
evaluated: 2026-08-18T07:16:54Z
observed: 2026-08-18T07:16:53Z | Watched all eight seeds converge in the review gate where seven did before: seed 3 house_0 now steps off road 0 on the first push instead of sliding along it for three passes, and seed 6 keeps its windmill occluded because the house that was shading it is no longer moved twice for one collision. Every hamlet ended with all three required objectives dark from all 48 treeline rays.
scenario: Headless CLI, measured 187-mesh kit: python -m gslevelgen.pipeline --seed 1 --seeds 8, plus per-seed instrumented traces of seeds 3 and 6 printing each finding the refiner received and each move it made, plus the 9-group/33-assertion fixture suite. No editor.
files: 
  - ../level-gen/gslevelgen/refine.py
  - ../level-gen/test_pipeline.py
  - ../level-gen/README.md
  - ../level-gen/SUBMISSION.md
---

## Goal

level-gen: _move_building_clear rotates a house back onto the road it was escaping, burning all 3 passes and opening a sightline

## Goal (context)

Gate sat at 7/8 after #170. Michael asked what it would take to reach 8/8. The answer was not
the copse radius - it was three instances of one bug, two of which I introduced in #170.

## Generate

**`gslevelgen/refine.py`**
- `_blocked` now rejects a destination that sits on ANY road (`seg_intersects_rect` against
  `cand.inflated(-40.0)`, the same inflation `check_roads_clear` uses, so the two agree on what
  "on the road" means).
- New `_exposure(plan, bid, cand)` - counts unoccluded (objective, treeline-ray) pairs with a
  building optionally relocated. Mirrors `check_cover_guarantee` exactly, including the
  `is not obj.rect` identity test.
- `_move_building_clear` now scores each geometrically-clear candidate with `_exposure` and
  takes the first that does not INCREASE exposure; falls back to the geometrically-clear spot,
  then to the blind move. An overlap is a harder failure than an exposed ray, and the resulting
  cover findings are themselves refinable.

**`gslevelgen/evaluate.py`**
- `check_no_overlap` no longer lists buildings that carry an `objective_id`. The objective entry
  already stands for that footprint.

## Evaluate

Run, headless, repeatedly:

- **Gate: 8/8.** `--seed 1 --seeds 8` -> `passed: [1,2,3,4,5,6,7,8]`, none escalated, every seed
  inside 3 passes, 10-25 findings apiece at generation. ~1s total.
- **33/33 assertions** across 9 fixture groups still pass, including the negative control.
- **The breaker is still non-vacuous** - `test_circuit_breaker_fires` drives it with unpackable
  layouts and 3/3 escalate with problem statements inside the cap. 8/8 on live seeds does not
  mean the breaker became dead code, and that distinction is now stated in both write-ups.
- **Determinism holds** - two consecutive `--seed 7` runs, identical `plan.json` md5
  (`616d2a584e9186c9c9d994b326b5dc40`).

All three defects were found by INSTRUMENTING a failing seed - printing what `refine` actually
received and did per pass - not by reading the code. Reading it, I was about to enlarge the
copse radius, which would have fixed nothing. Michael's note on this: "this is why we run the
workflow rather than guessing, to find failures."

## Refine

The three, in the order they surfaced, each found only after the previous was fixed:

1. **Seed 3: the refiner walked a house along the road it was escaping.** `_move_building_clear`
   rotates the push bearing up to +/-1.6 rad to dodge a neighbour and never checked the road.
   `house_0` went 3794 -> 3405 -> 4265 -> 5793 across all three passes without clearing road 0,
   and its final position opened the sightline the breaker escalated. The 0-degree arc was the
   symptom; the road push was the disease.
2. **Seed 6: buildings are cover, and moving one destroys it silently.** A house between the
   treeline and the windmill does the cover guarantee's job whether or not it was placed for
   that reason. Repairs are now cover-preserving.
3. **Seed 6 again: one collision reported twice, so the house moved twice.** The windmill
   building shares its objective's `Rect` OBJECT; stalls sit inside the market footprint. The
   first move resolved the overlap and the redundant second carried the occluder away.

**The detail worth keeping:** #3 landed on pass 3, and the loop always terminates on an
evaluation - correct, and it means a failure INTRODUCED by the final pass can never be repaired.
A refiner has to be conservative, not merely effective. Written up as catch #4 in README.md.

**Left undone:** nothing in scope. The wider `Objective.Granary` / `AGSObjective_BurnGranaries`
rename is still #179's flagged follow-on, and still wants the editor.
