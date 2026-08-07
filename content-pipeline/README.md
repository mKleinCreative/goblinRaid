# Goblin Siege — Dynamic Content Pipeline

A retrieval-augmented content pipeline that writes game text for *Goblin Siege* out of the
project's own design documents, then checks every line back against them with a critic agent
before anything ships.

```
python gsrag.py --list              # the jobs, and the gap each one fills
python gsrag.py --retrieval-only    # chunking + retrieval, no API calls, no cost
python gsrag.py barks whispers prompts
```

Credentials: `ANTHROPIC_API_KEY` in a `.env` at `content-pipeline/`, the repo root, or
`GoblinSiege 5.8/` (all three gitignored), or an `ant auth login` profile.

---

## 1. Knowledge base — the actual GDD

No placeholder lore. The corpus is this project's real design documents, one directory up:

| Document | Role |
|---|---|
| `goblin-siege-design-document.md` | The GDD, final draft — 311 lines, authoritative |
| `race-design-goblins.md` | Race brief — Goblins |
| `race-design-humans.md` | Race brief — Humans |
| `race-design-elves.md` | Race brief — Elves |
| `race-design-dwarves.md` | Race brief — Dwarves |

These split into **95 heading-aware chunks**. Every chunk keeps its heading path, so a citation
reads `GDD§2.10#2 — 2. Game Mechanics > 2.10 Tone — the comedy is systemic`. Each generated row
records the chunk ids it was written from, and each critic finding must cite the chunk that
proves it. Nothing in the output is traceable to anything but these five files.

**The corpus contradicts itself, on purpose, and the pipeline is told so.** The race briefs
predate the 2026-07-23/24 scope decisions: `race-design-goblins.md` calls the player class the
**Slasher** (110 HP, speed 190) while the GDD calls it the **Scout** and says it is the only
class in this slice; the briefs still assume palisade-era content. The system prompt states the
GDD supersedes, and a deterministic lint fails any row containing `Slasher`, `Brute`, `Shaman`,
`Pennybrook`, `Silverford`, `palisade`, `battering ram`, or `catapult`. This is the retrieval
problem the project actually has, not a synthetic one.

---

## 2. What was generated, and the gap each fills

The rubric asks the submission to name the gap. All three are gaps the **GDD names about
itself** — this is outstanding work the design document is already tracking, not content
invented to have something to generate.

### `out/barks.csv` — NPC bark sheet, livestock & stealth extension (18 rows)

> **The gap.** GDD §3.2 (The Writer): the approved bark sheet is *"now due a livestock-and-stealth
> extension pass."* The livestock system (§2.7) and the lean-five stealth layer (§2.4) both landed
> *after* that sheet was approved, so the states they introduce — a startled flock, a coin
> scramble, a discovered corpse, an overdue patrol — have no lines at all.

Keyed to speaker × alarm phase × trigger. `alarm_state` values match the `EGSAlarmPhase`
enumerators in `Source/GoblinSiege/Alarm/GSAlarmTypes.h` exactly (`Quiet`/`Suspicious`/`Raid`/
`Razed`), so the column will import against that enum.

### `out/whispers.csv` — His Eternal Darkness, whisper and verdict lines (14 rows)

> **The gap.** GDD §3.2: *"new capture/purge-triggered Overlord verdict lines (§2.10)"* are
> outstanding. §2.10 asks specifically for a raid that comes home with a rope chain of prisoners
> to earn a different flavour of verdict than one that comes home merely rich. The 10/5/2-minute
> clock nudges (§2.2) have no lines either.

Covers clock nudges, per-objective barks, milestone whispers, and end verdicts.

### `out/prompts.csv` — tutorial hamlet first-time prompts (9 rows)

> **The gap.** GDD §2.8 specifies a skippable teaching layer — every objective and mechanic
> carries *"a bark from His Eternal Darkness plus a one-line HUD note naming the objective, the
> payoff, and the one control that does it"* — and §3.1 lists it as a Settlement Generator Agent
> deliverable for week 4. Exactly one instance is currently written: the windmill Stage-1 bark.

---

## 3. How it works

```
5 design docs
   └─ heading-aware chunker ────────────► 95 chunks (heading path preserved)
         └─ BM25 + heading boost ───────► ~18 chunks per job, from 7–8 queries
               └─ WRITER (Claude Opus 5) ──► draft rows, each citing chunk ids
                     └─ LINT (deterministic) ──► banned tier-1 terms, uncitable rows
                     └─ CRITIC (Claude Opus 5) ─► findings, each citing the chunk that proves it
                           └─ REVISER ────────► applies only the findings
                                 └─ loop, always ending on a check
```

**Retrieval is BM25 with a heading-match boost, not vector embeddings.** Anthropic ships no
embeddings endpoint, and at 95 chunks the vocabulary is highly specific — "Warren", "soft
signal", "First Spark Unseen", "raid-response pool", "None Left Behind". Exact-term matching
beats semantic similarity on terms like these, is deterministic, and adds no second vendor. Each
job issues 7–8 queries and unions the per-query top-4. An optional `--rerank` flag puts a Claude
semantic pass over the BM25 candidates, using the same key.

**The critic is a separate agent with its own authority.** It is prompted as the Design Steward
(GDD §3.2, agent 7), reads the same retrieved canon, and must cite `evidence_chunk_id` for every
finding — *"a finding you cannot ground in a quoted chunk is not a finding, and you must not
raise it."* It classifies into `LORE_BREAK`, `TONE_DRIFT`, and `REGISTER_DRIFT`, and supplies a
concrete replacement that preserves the row's trigger.

**The loop always ends on a check, never on a revision.** This was a bug in the first build,
caught by running it — see §4.

Each job writes `<job>.csv` (data-table shaped), `<job>.draft.json` (the pre-critic draft),
`<job>.trace.json` (queries, retrieved chunks, every round, final rows) and `<job>.trace.md`
(query → retrieved chunk → output, side by side).

---

## 4. What the critic caught

Every finding below is in `out/*.trace.json`, with the pre-critic text in `out/*.draft.json`.

### The clearest one: a wrong number, fixed wrong, caught again

`Barks_04_PatrolWaypointCheckIn`, across three rounds:

| | Line | Verdict |
|---|---|---|
| **Draft** | "Waypoint's clear, horn's dry, back **before the hour** — mark us in." | `LORE_BREAK` — §2.1 sets a 5–7 minute patrol cadence |
| **After revision 1** | "…back **inside the half hour**… call it five minutes." | `LORE_BREAK` **again** — "half hour is the raid clock, not the patrol cadence" |
| **After revision 2** | "Waypoint's clear, horn's dry, **back in five — six if the mud's bad.** Mark us in." | clean |

The reviser's first correction was *also* wrong. Only because the loop re-checks after revising
did the second error get caught — and in the original build it would not have been, because the
loop ended on a revision. **That is the bug the run found**, now fixed: the loop runs up to
`--rounds` revisions but always terminates on a verification pass, and reports `UNRESOLVED` if
findings survive rather than shipping unverified rows.

### Others worth naming

- **`Whispers_09` — "Every soul in the hamlet, marched home alive."** `LORE_BREAK`: "None Left
  Behind" (+40, §2.9) scores *civilians* marched home; the bind mechanic applies to unarmed
  civilians only — guards, militia and archers are not ropeable. Corrected to *"Every villager
  in the hamlet…"*. A one-word error that would have taught players a mechanic that does not exist.
- **`Prompts_07` — "no shout, no bell, no witness."** `LORE_BREAK`: takedowns are silent but not
  clean. §2.4 has no body dragging, and a guard who *sees* a corpse goes to Suspicious. The
  prompt promised an evidence-free kill the mechanic does not deliver. Corrected to *"…but the
  body still talks if a guard finds it."*
- **`Prompts_03` — windmill.** `REGISTER_DRIFT`: the draft folded both burn stages into one
  first-sight line. §2.8's whole point is that the two stages are *separately legible*; the
  window-shot instruction belongs to the Stage-1 prompt that fires later.
- **Tone.** The critic rejected *"Let that be the last thing he does"* as "generic assassin
  menace" against an Overlord whose register is "pompous, whispered, chronically underwhelmed
  condescension." That one is still open — see §6.

---

## 5. Does it sound like the game?

Mostly yes, and the failures were specific rather than general.

**What landed.** The propaganda-as-denial joke (§2.10) came through without being asked for:
the watchman dozes on *"The king slew the last of 'em, so what exactly am I watching for… mmnh."*
Greed reads as an exploitable stat, per §2.10 — a guard scrambling for a tossed coin says
*"dropped coin, no owner, that's salvage, that's mine by law."* And the verdict pair §2.10
specifically asked for came out distinct:

> **Capture-heavy:** "You brought me a rope chain, not a corpse pile. Proof they remember us. …adequate."
> **Merely rich:** "Gold, a pig, three fires — and not one of them roped to say we were ever here. I have seen rats do better. …adequate."

**The concrete tweak that improved game-fit.** The critic filed `REGISTER_DRIFT` on the tutorial
prompts three separate times — not for canon errors but for stuffing three chained mechanical
clauses into a line §2.8 says is one HUD note. The brief was under-specified: it repeated the
GDD's phrase "one-line HUD note" without saying what one line *means*. I added hard constraints
to the `prompts` job brief — one objective, one payoff, one control; under ~90 characters; a
second stage is a *separate* prompt; modelled on the GDD's own written instance
(`"Windmill: Ablaze — needs a window shot"`) — and made the Overlord bark carry the flavour so
the HUD line does not have to. Re-running against the same retrieval:

| | max | mean | over 90 chars |
|---|---|---|---|
| Before tweak | 169 | 138 | 9 / 9 |
| **After tweak** | **85** | **64** | **0 / 9** |

Unresolved findings on that job dropped from 2 to 1. The before/after pair is preserved in
`out/_run1_unverified/prompts.beforeTweak.csv`.

**A retrieval note.** Register queries had to name the register, not the topic. Querying
`"His Eternal Darkness verdict"` returns §1 Executive Summary and §3.2 (the Writer agent's role)
above §2.10 Tone, because §1 mentions him most often. Adding the register words themselves —
`adequate rats layered subtitles register` — pulls §2.10 to the top, which is the section that
actually defines the voice. Topic words find *where a thing is discussed*; style words find
*where it is specified*.

---

## 6. Honest limitations

- **This content has no reader.** There is no `UGSBarkSubsystem`, and no `FTableRowBase` struct
  anywhere in `Source/` — the project has no DataTable infrastructure yet. These CSVs are
  data-table *shaped* (row `Name` first, flat columns, enum values matching `EGSAlarmPhase`) but
  they cannot be imported until someone defines the row structs. Authoring bark sheets ahead of
  the runtime is the order GDD §3.2 and the §4.5 schedule intend — barks are W7 — but "designed
  order" is not "wired," and the column names are my proposal, not a contract.
- **One finding is still open.** `Prompts_06` retains a tone line the critic flagged and two
  revisions failed to fix, with a correction supplied in the trace. Left flagged deliberately:
  GDD §3.3 gives Michael *"exclusive authority over aesthetics and tone,"* and an unresolved
  aesthetic call is exactly the thing an agent should escalate rather than settle.
- **The pipeline found a real ambiguity in the GDD.** On `Prompts_05` the critic reversed itself
  between rounds — round 1 cited §2.6 (a "goblin half-confirmed" is a soft signal that escalates
  to Suspicious), round 2 cited §2.4 (break the look before the confirm lands and "you were never
  there"). Those two sentences cannot both be literally true. This is a design question for the
  decisions ledger, not a model error.
- **No point value was ever wrong, but that is a narrower claim than it sounds.** Across every
  run, no finding concerned an incorrect number from the §2.9 score table — the values drafts
  would most plausibly have drifted on, and retrieval put §2.9 in context for every job that
  needed it. §2.9 *was* cited as evidence once, on `Whispers_09`, but for **who qualifies** for
  "None Left Behind" (civilians, not guards) rather than for its +40 value.
- Lexical retrieval will degrade if the corpus grows well past a few hundred chunks or if
  queries stop sharing vocabulary with the docs. At that point add embeddings.

---

## 7. Cost

Opus 5, adaptive thinking, effort `high`. The final production run — all three jobs, plus the
prompts re-run after the brief tweak — was **24 calls, 420,248 input / 104,810 output tokens
≈ $4.72**. Including the first (superseded) run, total development spend was 39 calls ≈ $7.91.

Retrieval keeps that low: each call sees ~11k tokens of retrieved context rather than the ~76 KB
GDD. Note this draws on **API credits**, which the Max 20x subscription in GDD §4.3 does not
cover — that budget table accounts for the ten agent sessions, not programmatic API calls.
