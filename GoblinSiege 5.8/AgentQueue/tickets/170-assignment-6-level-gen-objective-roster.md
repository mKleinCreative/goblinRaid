---
id: 170
title: Assignment 6: level-gen objective roster follows GDD 156 (granary out, Market/Statue/Windmill in); submission docs
agent: claude-rosterdrift
status: done
claimed: 2026-08-17T22:10Z
build: none
waiting_on:
evaluated: 2026-08-17T23:57:55Z
observed: 2026-08-17T23:57:06Z | Watched the eight-seed gate run end to end: seven hamlets converged, each ending with the statue, market and windmill occluded from all 48 treeline rays, and watched the refiner close arcs it had been handed, re-kind a duplicate objective to the missing required type, and flip a fire-marked statue back to topple. Seed 3 stopped itself after three passes and printed a problem statement naming one exposed ray of 48 on the optional field instead of shipping the layout.
scenario: Headless CLI against the measured 187-mesh kit.json: 'python -m gslevelgen.pipeline --seed 1 --seeds 8' and '--seed 7', plus the 9-group/33-assertion fixture suite. No editor, no API key. Determinism confirmed by identical plan.json hashes across two consecutive seed-7 runs.
files: 
  - ../level-gen/gslevelgen/generate.py
  - ../level-gen/gslevelgen/evaluate.py
  - ../level-gen/gslevelgen/refine.py
  - ../level-gen/test_pipeline.py
  - ../level-gen/README.md
  - ../level-gen/PRE_BUILD_DECLARATION.md
  - ../level-gen/SUBMISSION.md
---

## Goal

Assignment 6: level-gen objective roster follows GDD 156 (granary out, Market/Statue/Windmill in); submission docs

## Goal (context)

#156 removed the granary from the GDD on 2026-08-14 and closed with "the DOWNSTREAM
consequences are unverified". `level-gen/` was one of them: `check_objective_mix` went on
enforcing `granary | field | windmill` and every seed passed it. The assignment this pipeline
is submitted for is graded on "the Evaluator enforces a specific rule from the GDD — the rule
is identifiable in the GDD", so a stale roster is worth 1 of 10 points as well as being wrong.

No C++, no assets, no editor. Pure Python, headless.

## Generate

**`gslevelgen/generate.py`**
- `Objective` gains `required: bool` and `destruction: str`. Kind comment now
  `market | statue | windmill | field | house`.
- New module constants `REQUIRED_KINDS = (market, statue, windmill)`, `OPTIONAL_KINDS`,
  `DESTRUCTION` (statue → `topple`, everything else → `fire`), `STALL_GAP`,
  `BUILDING_CLEARANCE`.
- Objective roll is now Market/Statue/Windmill. Statue takes the granary's central radius
  (900–1800) and its 2.8 rationale — the village square, where the guards are thickest — at a
  plinth-sized footprint rather than a building's. Market is central too (1200–2400) and is
  **sized from its measured stall cluster**, not a chosen number.
- Market stalls moved inside the market objective's footprint and given its `objective_id`, so
  the overlap check does not report the market colliding with itself. Same linkage the
  windmill already needed. The old scattered-stall block is gone.
- Wheat fields now emitted as **optional** objectives (1–2 per hamlet), outside the trio.
- `free_spot` now rejects against `plan.objectives` as well as `plan.buildings`.
- New `free_angle` / `anchor_spot` helpers: objectives are placed by walking out along a
  bearing until the footprint is clear, instead of trusting a 0.9 rad angular gap that means
  nothing when one footprint is a 500cm plinth and the next is a windmill sweeping 5354.
- House ring inner radius **derived** from where the core anchors actually landed
  (`core_reach + BUILDING_CLEARANCE`) instead of the typed `mw * 3.0`.

**`gslevelgen/evaluate.py`**
- `check_objective_mix` counts **required** objectives only: exactly three, max two of a kind,
  and a new `objective_not_required` finding if an optional type is smuggled into the trio.
- **New check `check_statue_is_toppled`** (registered in `DETERMINISTIC`, now 10 entries = 9
  running + 1 declared SKIPPED). 2.8: "the one target that doesn't burn: it has to be brought
  down, stone on stone." Fails a statue marked `fire`.
- Stale granary comment in `check_no_overlap` corrected to the statue.

**`gslevelgen/refine.py`**
- `objective_dupes` handler extended to `objective_not_required`, re-kinds to a missing
  required type and fixes `destruction` with it.
- New `wrong_destruction` handler.
- **New `_move_building_clear`** + `_blocked`. The blind `_move_building` was shoving a house
  clear of one overlap and straight into another; fail counts oscillated 1 → 2 → 1 across the
  three passes and the breaker fired on satisfiable layouts. Destination is now checked before
  it is taken, with a fallback to the requested move so a crowded layout still escalates
  honestly rather than silently doing nothing. Overlap, road and reachability fixes all route
  through it.

**`test_pipeline.py`** — fixture roster updated to the trio + an optional field; planted
failure is now a statue in the open; two new assertions that optional objectives do not count
toward the three; new `test_statue_does_not_burn` (4 assertions).

**Docs** — `PRE_BUILD_DECLARATION.md` original text preserved byte-for-byte with a dated
addendum (it is evidence of having been written first; it does not get backdated).
`README.md` granary references corrected, check count reconciled against the code, evidence numbers
re-measured, new catch #3 written up. New `SUBMISSION.md` — the short ReadMe the brief asks
for.

## Evaluate

**Verified by running it, three times, headless:**

- `python test_pipeline.py` — 33 assertions across 9 fixture groups, all pass. Includes the
  negative control (strip the cover from a corrected layout and it fails again), so PASS is
  not vacuous.
- `python -m gslevelgen.pipeline --seed 1 --seeds 8` — **7 of 8 pass** (1, 2, 4, 5, 6, 7, 8),
  each after 2–3 refine passes, having generated with **10–25 findings apiece**. Seed 3
  escalates with a problem statement.
- Determinism re-checked by hash: two consecutive `--seed 7` runs produced byte-identical
  `out/plan.json` (`416406ae2082fb580e9efd80fc4a8ab0` both times).

**The gate moved and I am reporting it moved.** It was 8/8 before this change and is 7/8 now.
Swapping a field out of the required trio for two central objectives, and adding 1–2 optional
fields on top, is materially more anchored area than the old roster; the first run after the
swap fell to 5/8. Two generator defects and one refiner defect were fixed to recover 7/8, all
three of them arithmetic rather than tuned constants.

**Seed 3 is left escalating on purpose.** Its surviving finding is a 0° arc — one exposed ray
of 48 — on an optional field. Enlarging the copse radius would close it and would be the exact
mistake catch #1 in the README is about (tuning a number to paper over a modelling question).
The circuit breaker exists to fire on what the refiner cannot honestly close.

**Not verified / out of scope:** `Source/GoblinSiege/Combat/GSGameplayTags.cpp` still declares
`Marker.ObjectiveAnchor.Granary` and has no Statue or Market anchor tag. That is a C++ change
needing an editor-closed rebuild, and `apply_in_editor.py` has never been run, so nothing is
blocked by it today. Deliberately not done here — it wants its own ticket.

Nothing outside `level-gen/` was touched. No build, no editor, no assets.

## Refine

Three things changed in response to running it rather than reading it:

1. **First gate run after the roster swap: 5/8, drowning in house-vs-objective overlaps.**
   Diagnosis was not "the radii are too small" — `free_spot` had never rejected against
   objectives at all, so the generator was *creating* overlaps it knew the refiner would have
   to undo. Fixed at the source.
2. **Then seed 7 still failed with the statue unreachable and overlapped.** The house ring was
   a typed `mw * 3.0` that overlapped the village square, so houses wrapped around the statue
   and walled it in — reachability correctly called it an unwinnable raid. Ring inner radius
   is now measured off the placed anchors. Same discipline `kit.json` applies to meshes.
3. **Then the gate sat at 5/8 with fail counts oscillating 1 → 2 → 1.** That pattern is the
   refiner making things worse, not the layout being hard. `_move_building_clear` checks the
   destination; 5/8 → 7/8.

### Second refine round — an adversarial grading pass found worse than the roster drift

Six agents were run over the finished package: three grading it cold against the rubric, one
re-running every claim, one verifying every GDD citation against the document. The citation
verifier found the thing this whole ticket is about, one level deeper:

4. **The headline rule was cited to the wrong section in every artifact.** `cover_guarantee`
   carried `gdd="2.8"`, and `SUBMISSION.md` block-quoted a sentence attributed to §2.8 that is
   **§3.1 verbatim**. §2.8 spans lines 135–169 and its only cover sentence is about trees
   filling seams between modules. The rule is stated at **§2.4** (line 88, "if the generator
   authors it, the cover-placement rule has to be enforced algorithmically from day one") and
   restated at **§3.1** (line 233). I introduced the block quote myself earlier in this
   ticket. A grader checking the citation — which is exactly what the rubric asks them to do —
   greps §2.8 and finds nothing. Now cited `2.4`, with both quotes attributed correctly.
5. **`objective_mix` cited §2.8 for "never more than two of a kind".** §2.8 says the opposite
   for this hamlet (a fixed one-of-each layout) and names the randomized roll as the thing it
   replaces. The clause is **§1**. Now `2.8/1` — roster from §2.8, the duplicate rule from §1.
6. **`reachability` cited §2.1/2.2, which say nothing about it.** The sentence is in §2.8's
   week-2 timebox: "no objective sited somewhere unreachable". Now `2.8`. (The graders
   proposed §4.4 here; §2.8 line 143 carries it directly, so §2.8 it is.)
7. **Three checks were claiming a GDD authority they do not have.** `no_overlap`,
   `building_integrity` and `roof_coverage` were all stamped `2.8` under a module docstring
   asserting "none of these rules were invented here". The GDD states no building-intersection
   rule, no roof rule and no door rule. They now carry `gdd="build"` and the docstring names
   which checks are design rules and which are buildability. This *strengthens* the
   submission: diluting one real, quotable rule with three generic ones made the real one
   harder to believe.
8. **Count drift, again, and mine this time.** I wrote "Eight deterministic checks" — the list
   registers ten (nine running + `buildable_ground` SKIPPED). I wrote "8 fixture groups" —
   there are nine. `README.md` still carried "18 fixtures" in two places and a stale
   "done: 8/8 pass" line. All reconciled against a live run and re-verified:
   **9 groups / 33 assertions, gate 7/8**.

The irony is the point and is now written into the README: this ticket exists because an
evaluator went stale against the document it cites, and the fix for it shipped with **five
more mis-citations of the same document**. Nothing in the loop can see that class of defect,
which is exactly the gap the README names as the one worth closing next.

Fixtures and gate re-run after every one of these edits; behaviour unchanged (7/8, 33/33).

**Deliberately left undone:** the 0° arc on seed 3 (above); the gameplay-tag work (above); and
the wider gap the README now names — nothing verifies that a GDD citation still says what the
check believes it says. That is the defect that let this ticket exist, and a real fix is a
check on the document, not on the output. It is a bigger piece of work than this ticket and
should not be smuggled into it.
