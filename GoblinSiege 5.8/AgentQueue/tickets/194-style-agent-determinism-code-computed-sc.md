---
id: 194
title: Style agent determinism: code-computed score, response cache with replay, so a graded run is byte-reproducible
agent: claude-styledeterm
status: done
claimed: 2026-08-19T00:50Z
build: none
waiting_on:
evaluated: 2026-08-19T02:17:39Z
observed: 2026-08-19T02:17:38Z | Ran the recorded suite then replayed it twice and watched the same three verdicts come back with zero model calls in zero seconds, and the md5 of all three trace files match across both replays. Also watched the stricter code-computed score still award 10/10 to five known-good shipped barks, so tightening the grader did not cost it the ability to recognise good writing.
scenario: python -u gsstyle.py --case all --rounds 3 (17 live calls, 370s) then the same command twice with --replay, md5summing out/style/*.trace.json each time and diffing; plus --negative-control and direct unit calls into aggregate_score and _coerce. Headless CLI on the Max subscription, no editor.
files: 
  - ../content-pipeline/gsstyle.py
  - ../content-pipeline/STYLE_SUBMISSION.md
  - ../content-pipeline/STYLE_GUIDE.md
  - ../content-pipeline/DEMOS.md
---

## Goal

Style agent determinism: code-computed score, response cache with replay, so a graded run is byte-reproducible

## Goal (context)

I closed #191 with an explicit non-claim: the loop is model-driven and scores shift run to run
(tone scored 2/10 on one run and 3/10 on the next). Michael asked for that fixed.

## Generate

Diagnosed first rather than guessed, by dumping what the evaluator actually returned. Two
distinct sources, only one of which was the evaluator:

1. **The Generator re-rolls its draft every run**, so consecutive runs score DIFFERENT TEXT. This
   was the dominant source, larger than any evaluator wobble.
2. **The headline score was a model gestalt.** On the run that motivated this, per-rule scores came
   back `10, 10, 1, 2, 3, 10` and the model then announced **3/10** - a number nothing in its own
   judgement produces.

Three changes to `content-pipeline/gsstyle.py`:

- **`aggregate_score()`** - the model no longer emits an overall score at all (`score` removed from
  `EVAL_SCHEMA`). It judges the six rules; Python computes `min(per_rule)`, then hard-caps at 6 if
  deterministic lint fired. The guide is a conjunction, so the weakest rule governs. The lint cap
  was previously a polite request in the system prompt; it is now arithmetic.
- **Response cache** - every call keyed on `sha256(provider, system, user, schema)` into
  `out/style/cache/`. `--replay` reproduces a recorded run and REFUSES to contact the model; a
  cache miss is a loud error, never a silent re-roll. `--no-cache` opts out.
- **`_coerce()` + hardened `extract_json()`** - see Refine.

## Evaluate

**Watched, and measured, not asserted.**

- Fresh recorded run, `--case all --rounds 3`: **tone 1->10, vocab 2->9, format 2->9**, all PASS.
  17 live calls, 370s, $0 (subscription).
- **Determinism proof**: two consecutive `--replay` runs produced BYTE-IDENTICAL traces -
  `md5 1a3f1214… / 95336e8b… / bc27e78b…` on tone/vocab/format - in **0s with 0 live calls**.
  Verified by diffing the md5 sets, not by inspection.
- **Negative control still 10/10** on five known-good shipped barks under the stricter aggregation,
  all six rules at 10. This mattered: `min()` is harsher, and a stricter grader that can no longer
  recognise good content would have been a regression dressed as a fix.
- `aggregate_score` unit-checked directly against the numbers that motivated it:
  `(10,10,1,2,3,10)` -> 1; all-10s -> 10; all-10s + one lint -> 6; all-9s -> 9.

**What is NOT claimed, and is stated in the submission:** a fresh run against the live model still
varies. A model is not deterministic and cannot be made so. What is now guaranteed is that a
RECORDED run reproduces exactly, offline, with no key - which is the property a grader actually
needs.

**Side effect, disclosed:** `min()` is stricter than the model's own gestalt, so the before-scores
dropped (tone 3->1, vocab 3->2, format 4->2). The afters held at 9-10.

## Refine

Three defects surfaced while doing this, all found by running rather than reading:

1. **`KeyError: 'items'` killed the refiner mid-run.** In text mode the model returns the contract
   object, a bare array of rows, or a single unwrapped row, more or less at random, and
   `extract_json` took the FIRST parseable object - on that run a nested example inside prose.
   Fixed structurally rather than by re-prompting harder (a second attempt at the same approach
   is not a fix): the parser now prefers an object carrying the contract's required keys, and
   `_coerce()` accepts all three real shapes plus fenced and prose-wrapped replies. Unit-tested
   against all five.
2. **A `.replace()` silently no-op'd** on the demo formatter because I did not assert the anchor
   matched, so `DEMOS.md` shipped without the HUD breakout - the whole point of the format demo.
   Caught by reading the generated file instead of trusting the write. Every later patch asserts.
3. **I clobbered a `for` loop** while repairing the summary print, because I assumed the statement
   was three lines when it was two. Caught immediately by `ast.parse`, which is now run after
   every scripted edit to this file.

**Left undone:** `--provider api` remains unexercised - the key in `GoblinSiege 5.8/.env` is still
401. Not this ticket's business.
