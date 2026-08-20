---
id: 191
title: Assignment 7: Style Guide Agent - GDD-anchored rules, scoring Evaluator (SCORE+REASON), Refiner loop, three before/after demos
agent: claude-styleagent
status: done
claimed: 2026-08-18T23:07Z
build: none
waiting_on:
evaluated: 2026-08-19T00:38:20Z
observed: 2026-08-19T00:38:19Z | Watched the loop run itself clean three times over: an off-brand villager screaming Run for your lives came back as That was a badger. Big one. Green badger. Standing up. Anyway; two real banked rows naming the deleted granary came back rewritten onto the Market and the Windmill; and three 900-to-1300-character HUD paragraphs came back at 63, 67 and 69 characters with every invented audio cue gone. Also watched the evaluator score five known-good shipped barks 10/10, so it is discriminating rather than just complaining.
scenario: Headless CLI on the Max subscription: python -u gsstyle.py --case all --rounds 2 (11 calls, 232s), plus --negative-control, --dry-run and --list with no API key set. No editor, no build, no C++.
files: 
  - ../content-pipeline/gsstyle.py
  - ../content-pipeline/STYLE_GUIDE.md
  - ../content-pipeline/STYLE_SUBMISSION.md
  - ../content-pipeline/DEMOS.md
---

## Goal

Assignment 7: Style Guide Agent - GDD-anchored rules, scoring Evaluator (SCORE+REASON), Refiner loop, three before/after demos

## Goal (context)

Assignment #7 wants a Generator -> Evaluator -> Refiner loop enforcing this game's own aesthetic
rules, where the Evaluator returns a SCORE (1-10) and a REASON and the Refiner rewrites from that
reason, unattended. Binary pass/fail is explicitly disallowed.

`content-pipeline/gsrag.py` already had ~85% of the machinery. Its critic, however, is purely
categorical and exception-based - a clean row emits nothing at all, and there is no number
anywhere in the file - so the scoring evaluator was genuinely new work.

## Generate

**`content-pipeline/gsstyle.py`** (NEW, ~740 lines). Imports `call_json`, `Retriever`,
`load_corpus`, `render_context`, `load_dotenv`, `_items_schema`, `USAGE`, `OUT`, `MODEL` from
`gsrag`. `gsrag.py` itself is untouched.

- `STYLE_RULES` - six rules across three constraint types (vocabulary / tone / formatting), each
  with its GDD section and a verbatim quote. Rules live once, as data; `STYLE_GUIDE.md` documents
  the same list, so guide and agent cannot drift.
- `style_lint()` - deterministic, zero API calls: banned terms (incl. granary), the 90-char HUD
  ceiling, audio words, settlement-name pattern with the legal counties allow-listed.
- `EVAL_SYSTEM` + `EVAL_SCHEMA` -> `{score 1-10, reason, per_rule[]}`. Lint findings are passed in
  as PROVEN; the prompt forbids scoring them away and caps such a draft at 6.
- `REFINER_SYSTEM` - rewrites from the evaluator's REASON.
- `run_case()` - the loop. Breaks on `score >= threshold and no lint`; max 2 refinements; always
  terminates on an EVALUATION, never a refinement; reports UNRESOLVED rather than shipping.
- Three cases: `tone` and `format` generated from deliberately off-brand briefs, `vocab` from the
  REAL stale banked CSVs.
- CLI: `--list`, `--dry-run`, `--negative-control`, `--rebuild-demos` (all free), `--case`,
  `--rounds`, `--threshold`, `--provider`.

**Second provider, unplanned.** The `ANTHROPIC_API_KEY` in `GoblinSiege 5.8/.env` returns
**401 "API key is invalid"**. Added a `claude-cli` provider (headless `claude -p`, billed to the
Max subscription) with schema-in-prompt and a `raw_decode`-walking `extract_json` - deliberately
not brace-counting, which is how CodeArchitect's live-002 died. Default is now `claude-cli`;
`--provider api` still works once a key is replaced.

Also new: `STYLE_GUIDE.md`, `STYLE_SUBMISSION.md`, generated `DEMOS.md`, `out/style/*.trace.json`.

## Evaluate

**Watched it run, repeatedly, headless.**

- `--case all --rounds 2`: **tone 3/10 -> 9/10, vocab 3/10 -> 10/10, format 4/10 -> 9/10**, all
  PASS. 11 calls, 232s, **$0** (subscription, not credits).
- **Negative control**: five known-good shipped barks score **10/10 with zero lint**. This is the
  check that matters - an evaluator that flags everything is as useless as one that flags nothing,
  and #6 taught that a non-vacuous PASS has to be demonstrated, not assumed.
- Free paths verified with no key: `--list`, `--dry-run`, `--rebuild-demos` all work and make zero
  API calls. The dry run alone finds the two real granary rows.
- Determinism is NOT claimed. This loop is model-driven and scores vary run to run (tone scored
  2/10 on one run and 3/10 on the next). The traces record actual observed runs, not a guarantee.

**The demo that is not staged:** `out/prompts.csv` and `out/whispers.csv` really still carry
`FirstSight.Granary` and `Objective.Granary.Stage1`, generated before #156 removed the granary on
2026-08-14. The agent caught both deterministically before spending a token.

## Refine

**The format case first ran to UNRESOLVED at 6/10, and the refiner was not at fault - I was.**
GDD 2.8 specifies a prompt as TWO things ("a bark from His Eternal Darkness plus a one-line HUD
note"), and my row schema had a single `text` field. The model correctly produced both and packed
them into it, so the lint measured bark+HUD against the HUD's 90-char ceiling. Unsatisfiable by
construction: no rewrite could ever have passed, and the score was pinned at 6 by my own
lint-cap rule.

The model had done the work - 1078 chars down to 154. Splitting `hud_line` into its own field (the
shape `prompts.csv` already used) took the case to 9/10 PASS with HUD lines of 63, 67 and 69
characters. Diagnosed by dumping what `refine()` actually received per pass, not by reading it.

Two smaller ones: a `.replace()` on the demo formatter silently no-op'd because I did not assert
the anchor matched, so `DEMOS.md` shipped without the HUD breakout - caught by reading the
generated file rather than trusting the write. And a backgrounded run with `tee` produced an empty
log and no artifacts; re-run in the foreground with `python -u`, which is how the first real
numbers arrived.

**Left undone:** `--provider api` is wired but unexercised, because the key is dead. Whether to
replace the key is Michael's call, and the submission does not need it.

**Cleanup:** `out/style_run.log` (the empty artifact of the failed background run) deleted.
