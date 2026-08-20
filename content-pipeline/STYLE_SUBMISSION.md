# Assignment #7 — Style Guide Agent

**Game:** *Goblin Siege* — a third-person raid game where you are the goblin. Capstone project.
**Student:** Michael Klein · **Run date:** 2026-08-18 · **Agent:** `gsstyle.py`

```bash
python gsstyle.py --list              # the 6 rules and 3 cases      — no API calls
python gsstyle.py --dry-run           # + retrieval and the lint     — no API calls
python gsstyle.py --case all          # the graded run
python gsstyle.py --negative-control  # prove the evaluator can also say 10
python gsstyle.py --case all --replay # reproduce the recorded run exactly — 0 model calls
```

---

## Pipeline connection

This Style Guide Agent runs immediately after `gsrag.py` generates NPC barks, Overlord whispers
and tutorial prompts, and before any of those CSVs are handed to the DataTable import, so no line
reaches the game until it has been scored against the GDD and, if it fails, rewritten.

---

## 1. The style guide — `STYLE_GUIDE.md`

**Six rules across three constraint types**, every one quoted verbatim from
`goblin-siege-design-document.md` with the section that actually contains the sentence.

| Type | Rule | What it enforces |
|---|---|---|
| **Vocabulary** | `VOCAB_NAMELESS` | Settlements are nameless — "too small to have a name" (§1). Only counties get names, in the ascending-currency register. |
| **Vocabulary** | `VOCAB_SLICE` | Only slice content exists: the Scout not the Slasher, no palisade/ram/catapult, and **no granary** — removed from the GDD 2026-08-14. The statue is toppled, never burned (§2.3, §2.8). |
| **Tone** | `TONE_DISBELIEF` | First contact is disbelief, not fear — "must've been a badger" (§2.10). The kingdom said goblins were extinct. |
| **Tone** | `TONE_OVERLORD` | His Eternal Darkness is pompous, whispered, chronically underwhelmed — "…adequate". He mocks more than he mourns (§2.1, §2.10). |
| **Formatting** | `FORMAT_PROMPT` | One bark **plus one HUD line** naming exactly the objective, the payoff, and the one control — ≤90 chars (§2.8). |
| **Formatting** | `FORMAT_SILENT` | Text only. The project has zero audio assets, so no SFX or voice references. |

The rules live once, as data in `gsstyle.py` (`STYLE_RULES`), and `STYLE_GUIDE.md` documents that
same list — so the guide and the agent cannot drift apart.

> Every quote was located by line number in the GDD before it was written down. Assignment #6 lost
> marks for citing a rule to §2.8 when the sentence lived in §2.4, and a grader who greps the
> section finds nothing.

## 2. The loop — Generator → Evaluator → Refiner

Unattended. One command runs to termination with no human in it.

| Agent | What it does |
|---|---|
| **Generator** | Writes rows from a brief. Deliberately naive about house style — that is what gives the evaluator something real to catch. |
| **Deterministic lint** | Banned terms, the 90-char HUD ceiling, audio words, settlement names. **Zero API calls** — a regex should never cost a token. |
| **Evaluator** | Scores all six rules 1–10 and writes the **REASON**, with the offending quote and what the game requires instead. The headline **SCORE is computed in code** from those six (§4), not chosen by the model. Lint findings are passed in as *proven* and cap the score at 6 — enforced in Python, not asked for in prose. |
| **Refiner** | Rewrites **from the REASON** to reach 10/10, changing only what the reason names. |
| **Circuit breaker** | Max 3 refinements (`--rounds`), then report `UNRESOLVED` rather than ship. |

**The loop always terminates on an evaluation, never a refinement** — so the text that ships is
text that was scored. Inherited from `gsrag.py`, which records why: a revision once "fixed" a
wrong patrol cadence to a *different* wrong value, and only the next check caught it.

## 3. Results

| Case | Violation class | Input | Score |
|---|---|---|---|
| **tone** | `TONE_DISBELIEF`, `TONE_OVERLORD` | generated off-brand | **1/10 → 10/10** |
| **vocab** | `VOCAB_SLICE` | **real stale banked content** | **2/10 → 9/10** |
| **format** | `FORMAT_PROMPT`, `FORMAT_SILENT` | generated off-brand | **2/10 → 9/10** |

17 model calls, 370 seconds. Full before/after in **`DEMOS.md`**, raw traces in
`out/style/*.trace.json`.

**Reproducibility:** `--replay` reruns the recorded run from cache — **0 model calls, 0 seconds,
byte-identical traces across repeated replays** (verified by md5). See §5.

**Negative control:** five known-good shipped barks score **10/10 with zero lint findings**. An
evaluator that flags everything is as useless as one that flags nothing, so the agent has to be
able to say 10 — and it does.

### The one that isn't staged

Demo 2's input is not content I wrote to be wrong. `out/prompts.csv` and `out/whispers.csv` really
do still say `FirstSight.Granary` and `Objective.Granary.Stage1` — generated before queue #156
removed the granary from the GDD on 2026-08-14, and stale in the repo ever since. The agent found
it deterministically, before spending a token, and rewrote both rows onto the Market and the
Windmill.

## 4. The score is computed, not chosen

The evaluator does **not** pick the headline number. It scores each of the six rules and writes
the reason; `aggregate_score()` then derives the overall score in Python:

```
score = min(per_rule scores)        # the guide is a conjunction
if any deterministic lint finding:  # proven violations are not negotiable
    score = min(score, 6)
```

This was a real defect, not a tidy-up. On an earlier run the model returned per-rule scores of
`10, 10, 1, 2, 3, 10` and then announced a headline of **3/10** — a number nothing in its own
judgement produces. Asking a model politely to do arithmetic, and politely to honour a cap, is
how the same draft scores differently twice. Both are now code.

The consequence is a stricter grader: `min` means a 9 requires *every* rule at 9 or better, which
is why the before-scores in the table above are lower than they were when the model chose them.

## 5. Reproducibility

A language model is not deterministic and cannot be made so. A **run** can be.

Every call is cached against `sha256(provider, system, user, schema)`, so:

| | |
|---|---|
| `--case all` | records to `out/style/cache/` while it runs |
| `--case all --replay` | replays it — **0 live calls, 0 seconds**, and refuses to contact the model at all |
| cache miss under `--replay` | a loud error, never a silent re-roll |

Verified: two consecutive replays produced **byte-identical** `tone/vocab/format.trace.json`
(md5 `1a3f1214…`, `95336e8b…`, `bc27e78b…`). A grader can reproduce the exact submitted run with
no key and no network.

What this does **not** claim: a *fresh* run against the live model will still differ, because the
Generator re-rolls its draft each time — which was in fact the dominant source of the variance,
larger than any evaluator wobble. Freezing the inputs is what the cache does.

## 6. What the loop caught in the agent itself

The format case first ran to `UNRESOLVED` at 6/10, and the refiner was not at fault. GDD §2.8
specifies a prompt as **two** things — a bark *plus* a HUD note — and my row schema had a single
`text` field. The model correctly produced both and packed them into it, so the lint measured
bark + HUD against the **HUD's** 90-character ceiling. Unsatisfiable by construction: no rewrite
could ever have passed.

The model had in fact done the work, cutting 1078 characters to 154. Splitting `hud_line` into its
own field — the shape `prompts.csv` already used — took the case from 6/10 UNRESOLVED to 9/10
PASS, with HUD lines of 63, 67 and 69 characters.

A style agent is only as good as the schema it scores against, and an unsatisfiable rule looks
exactly like a lazy refiner from the outside. The circuit breaker is what made the difference
visible: it stopped and said so instead of burning passes.

**A second one, found while making the run reproducible.** In text mode the model returns the
contract object, a bare array of rows, or a single unwrapped row, more or less at random — and
`extract_json` took the *first* parseable object, which on one run was a nested example and
crashed the refiner with `KeyError: 'items'`. Fixed structurally rather than by re-prompting:
the parser now prefers an object carrying the contract's required keys, and `_coerce()` accepts
all three real shapes. Unit-checked against wrapped, bare-array, single-row, fenced and
prose-wrapped replies.

## 7. Provider note

Built for the Anthropic API, but the key in `GoblinSiege 5.8/.env` returns **401 "API key is
invalid"** as of 2026-08-18. The agent therefore ships with two providers and defaults to
`--provider claude-cli`, driving headless `claude -p` on the Max subscription — **the recorded run
above cost $0 in API credits**. `--provider api` is still wired and will use real `json_schema`
structured output once the key is replaced.
