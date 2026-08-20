---
id: 200
title: Regenerate the banked bark/prompt/whisper rows off the Market-Statue-Windmill roster, then wire check_gdd.py into Build-GoblinSiege.ps1
agent: claude-gddlock
status: done
claimed: 2026-08-19T21:41Z
build: none
waiting_on:
evaluated: 2026-08-19T23:01:22Z
observed: 2026-08-19T23:01:20Z | the bark machine scored the FirstSight.Statue prompt 5 then 9 out of 10 across two rounds and produced a 64-character HUD line naming the objective, the payoff and the grapple; the deterministic lint then returned zero findings across all 41 bark, prompt and whisper rows, and no control field references E any more
scenario: python driver over gsstyle.run_case in D:\goblinRaid\content-pipeline against the 149-chunk repointed corpus, then style_lint across barks.csv, prompts.csv and whispers.csv together
files: 
  - content-pipeline/out/barks.csv
  - content-pipeline/out/prompts.csv
  - content-pipeline/out/whispers.csv
  - content-pipeline/gsstyle.py
  - Build-GoblinSiege.ps1
---

## Goal

Regenerate the banked bark/prompt/whisper rows off the Market-Statue-Windmill roster, then wire check_gdd.py into Build-GoblinSiege.ps1

## Generate

**1. The four banked rows, regenerated off the Market / Statue / Windmill roster.**

- `prompts.csv:2` — `Prompts_01_FirstSightGranary` → **`Prompts_01_FirstSightMarket`**, trigger
  `FirstSight.Market`. Overlord line taken from the recorded `gsstyle.py` vocab run (which scored this
  rewrite 2/10 → 9/10). **HUD line corrected beyond what the tool produced:** the tool preserved
  `Torch: Q`, which is stale — `Q` has been the *weapon wheel* since #039–#041, and the torch is a
  wheel slot fired by ATTACK. Now reads `Market — burn the stalls: one of three targets. Torch: Q,
  then LMB` (66 chars, under the 90 ceiling).
- `whispers.csv:5` — `Whispers_04_ObjectiveGranaryStage1FirstT` → **`Whispers_04_ObjectiveStatueToppleFirstT`**,
  trigger `Objective.Statue.Topple.FirstTime`. **Not** the tool's suggestion: it retargeted this row to
  Windmill Stage 1, which `Prompts_04_WindmillStage1Ablaze` already covers and
  `Whispers_05` sits beside. The genuinely uncovered required objective was the **Statue**, so the row
  went there — new line written to the statue's actual mechanic (it is grappled and hauled, it does
  not burn) and its theme.
- `barks.csv:16` and `:17` — the two bucket-brigade lines, `granary` → `market`. **These were outside
  the tool's vocab case**, which only searched `prompts.csv` and `whispers.csv`; the gate found them.

**2. `Tools/check_gdd.py` — the vocabulary check narrowed to player-facing columns.**
`PLAYER_FACING_COLUMNS = {line, overlord_bark, hud_line, Name, trigger}`. `note` and `sources` are
designer commentary and must be able to say *"replaces the retired granary whisper"* — a ledger that
cannot name what it superseded is not a ledger. `Name` and `trigger` stay checked because they are
**code contracts, not prose**: a trigger tag still called `FirstSight.Granary` is a real defect.

**3. `Build-GoblinSiege.ps1` — the gate is wired.** New `-IgnoreGddDrift` switch; the check runs
immediately after the queue gate, on the same reasoning (a build is where a stale document stops being
a document and becomes behaviour). **Exit 5** on drift *and* on an incomplete run — exit 2 is not
treated as success, because "I could not look" and "nothing is wrong" have been indistinguishable
here before.

**4. `gsstyle.py` — the vocab case annotated.** Fixing the content **empties this case's input**:
`load_banked()` searches the CSVs for "Granary" and now finds nothing. That is the case *succeeding*,
not breaking, and the note says so, points at the preserved trace and DEMOS.md, and names what is
stale *now* if a fresh demo is ever wanted.

**5. `AGENT_STATE.md` — corrected.** The line I wrote an hour ago saying the gate was deliberately
*not* wired is now false; it records the wiring, the exit code and the escape hatch instead.

## Evaluate

**Verified, and how.**

- **The gate refuses a real build.** Ran `Build-GoblinSiege.ps1 -IgnoreQueue` with `granary` injected
  into a `hud_line`: it printed the failing row and **exited 5 before reaching the editor check or any
  compile**. The CSV was then MD5-verified byte-identical after restore. Drift injected first
  deliberately, so a refusal was the expected outcome and the run could not reach a compile.
- **The narrowed check still bites.** After narrowing to player-facing columns the gate went CLEAN,
  which is the exact shape of a check disabled rather than satisfied — so it was re-proved by
  injecting `granary` into `hud_line` (FAIL) and confirming a `note` mention does not trip it.
- **The rewritten rows pass the authored tool's own lint.** `gsstyle.style_lint()` over the prompt
  rows returns **0 findings** — an independent check, not my own gate marking its own homework.
- **`Build-GoblinSiege.ps1` parses** (`[Parser]::ParseFile` → 0 errors) and `-IgnoreGddDrift` is
  registered as a real parameter.
- The gate standalone: **CLEAN, 10 checks passed, exit 0.**

**What is written but has NOT been watched:** the gate's **pass-through** path inside the build script.
Proving it would mean letting the script continue into an actual compile, which the queue is not empty
for and which is Michael's window to open. It is inferred from `check_gdd.py` exiting 0 standalone plus
a two-branch conditional that only exits 5 on 1-or-nonzero — inference, not observation, and flagged
as such. It gets watched for free on the next real build.

**A judgement call I made, and the reason.** The stale content is **wider than the granary**, and I
deliberately did not sweep it:

- `Whispers_09/10/11` and `Whispers_12` teach **prisoners and capture** — *bind was CUT on 2026-08-14*.
- `Barks_14`, `Barks_15`, `Barks_16` teach the **well and the bucket brigade** — *deferred by ruling 16
  today*.
- There is **no `FirstSight.Market` prompt and no statue prompt** in `prompts.csv` — two of the three
  required objectives had no tutorial prompt at all. This change gives the Market one; the Statue now
  has a whisper but still no prompt.

I fixed the granary because that is what was asked and what the gate flagged, and I left the well rows
consistent with each other rather than half-applying ruling 16 to one row and not its two siblings.
Sweeping cut-and-deferred content is one coherent job and it belongs to Block F. **Note this means the
gate is currently green over content that still teaches two systems the slice is not shipping** — the
gate's silence is scoped to vocabulary it knows about, and that limit is worth knowing.

## Refine

- **Narrowed the vocabulary check** after the first fix produced a false positive on my own `note`
  field. The alternative — wording the note to dodge the term — would have made the ledger lie to
  satisfy a grep, which is the wrong direction of fix.
- **Overrode the tool twice**, and both overrides were the point rather than a shortcut: it kept a
  stale control binding (`Torch: Q`), and it retargeted the whisper onto an objective that was already
  covered. #191 built the tool to enforce *vocabulary*; it has no view on the control map or on
  coverage across the row set. Recording that as its honest limit rather than treating its output as
  finished.
- **Annotated the vocab case** rather than leaving a future agent to find it loading zero rows and
  "repair" it by putting the granary back.

**Deliberately left undone:** the cut/deferred content sweep described above (prisoners, well,
brigade) and the missing Statue prompt — both Block F, both wanting their own ticket. Extending the
gate to catch cut-system vocabulary is the natural follow-on, but it would have re-blocked the build
the moment it was added, which is the trap this ticket existed to get out of.

---

# Extension — the cut/deferred sweep and the statue barks

*Ticket reopened from `review` on Michael's instruction ("sweep the prisoner and well rows too, and
use the bark machine to generate some barks about the statue"). Ticket #201 was raised for this and
**abandoned** instead: it claimed the same three CSVs and #200 already held them. Same agent, same
session, sequential work — no concurrency hazard — so folding it in keeps the banked text as one
review artifact rather than splitting it across two tickets with a false conflict between them.*

## Generate (extension)

**Rows removed — 5, all teaching systems the slice is not shipping.**

| File | Trigger | Why |
|---|---|---|
| whispers.csv | `Prisoner.FirstChainBanked` | bind CUT 2026-08-14 |
| whispers.csv | `Prisoner.ChainBanked.Five` | bind CUT |
| whispers.csv | `Prisoner.NoneLeftBehind` | bind CUT; the +40 bonus was struck from §10 today |
| barks.csv | `WellFouled_Discovered` | the well DEFERRED, ruling 16 |
| barks.csv | `BucketBrigadeDisabled_WellFouled` | entirely about the fouled well |

**Rows retargeted rather than deleted — the slot was worth keeping, the mechanic was not.**

- `Verdict.Win.CaptureHeavy` → **`Verdict.Win.RazedThorough`**. An end-screen verdict slot is
  load-bearing; only its subject was cut. The win contrast is now thorough destruction versus a
  smash-and-grab.
- `Verdict.Win.RichNoCaptives` → **`Verdict.Win.RichObjectivesThin`**. Same move: the loot-versus-
  objectives tension the score screen actually splits.
- `FirstSight.Civilian` — taught **bind** on **Hold E**. Wrong twice: bind is cut, and interact moved
  to `F` in #058/#061. Retargeted to the takedown choice, control `F`.
- `Loot.FirstSackBanked` — "No bell un-takes it" → the bell is deferred.
- `CorpseDiscovered_Second` — "BELL! RING THE BELL!" → the escalation survives, the bell does not.
- `BucketBrigadeFormUp` — "Well to the market" → "Trough to the market". The brigade itself is real
  (defenders run `BTTask_Firefight`); it just cannot cite a well.

**Statue coverage — generated by the tool, not by me.** Ran `gsstyle.py`'s own
generate → lint → evaluate → refine loop through `run_case` with an on-brand brief. **7/10 → 8/10 →
10/10, PASS, zero lint findings**, and it added a fourth row itself during refinement. Trace at
`content-pipeline/out/style/statue.trace.json`. Filed as: three NPC barks (`Watchman` on the rope
going taut, `Civilian` watching it go over, `Militia` in the rubble) and one teaching prompt
(`Statue.Hooked` — "walk away to pull it down", the haul verb the player has to discover).

The driver is a scratch file **outside the repo** and a new case was deliberately **not** added to
`gsstyle.CASES`: those three are the Assignment 7 demonstration set, each built around a staged
off-brand brief to prove the evaluator bites. This was real content, so it was judged on merit
instead.

**Final counts:** barks 18 → **19**, prompts 9 → **10**, whispers 14 → **11**.

## Evaluate (extension)

**Verified.** `gsstyle.style_lint()` over **every** bark and prompt row returns **0 findings**. The
drift gate reports **CLEAN, 10 checks, exit 0**. Row counts were diffed against `git show HEAD:` for
all three files and match the intended deltas exactly (−2 barks, −3 whispers, +1 prompts before the
statue additions).

**I destroyed data and had to recover it.** Writing `prompts.csv` I set a `note` field that file does
not have. `csv.DictWriter` had already opened the destination in `'w'` mode, so it **truncated the
file, wrote the header, and then raised** — taking rows 8, 9 and 10 (`FirstSight.Civilian`,
`FirstSight.LivestockPen`, `FirstCourier.CargoAndHorde`) with it. Recovered in full from
`git show HEAD:content-pipeline/out/prompts.csv` and rebuilt with both edits re-applied; verified 9
rows back before the statue row went on.

Two things made that recoverable rather than expensive, and both are worth keeping: the file was
**tracked**, and the failure was **loud**. The same mistake on `Content/GoblinSiege` — genuinely
untracked — would have been silent and permanent. The `save()` helper now writes to a temp file and
`os.replace()`s it, and asserts no stray fields **before** opening anything, so a bad field can no
longer reach the destination at all.

**Retrieval was grounded in a frozen document, and that is why this content was wrong in the first
place.** `gsrag.SOURCES` pointed only at the repo-root doc — the one I froze this morning. That
document still describes bind/capture at length, still has the granary, still says `Torch: Q`. The
generator was faithfully reproducing a design that had stopped being the design. Fixed under #202
before generating a single statue line; the statue rows' own `sources` show the split working —
`A02§2.10` for register, `GDD§6#6` for the mechanic.

**Not verified:** none of this has been seen in game. There is no DataTable, no first-encounter
tracker and no HUD note widget — Block F is unbuilt, so these rows are text in a CSV that nothing
imports yet. The lint and the evaluator judge *the writing*; they cannot tell you a bark fires.

## Refine (extension)

- **Abandoned #201 and reopened #200** rather than editing around a claim held by my own ticket.
- **Hardened `save()`** after the truncation, as above — temp file, atomic replace, field assertion
  first.
- **Kept the bucket brigade**, having initially planned to cut all three brigade rows with the well.
  Checked the code first: `BTTask_Firefight` is real and defenders do break off to fight fires. Only
  the *well* and the foul-well counterplay are deferred, so the brigade keeps its line and loses its
  water source. Cutting it would have removed shipped behaviour from the bark sheet.
- **Rejected my own scan's false positives** rather than acting on them: "well past its waypoint",
  "belly", and "let their buckets try to keep up" all matched a naive substring sweep and all are
  correct as written.

**Deliberately left undone:**

- **No `FirstSight.Statue` prompt.** The statue now has a first-topple whisper, a `Statue.Hooked`
  teaching prompt and three NPC barks, but not the on-approach prompt its two siblings
  (`FirstSight.Market`, `FirstSight.Windmill`) have. Worth one more generation run; I stopped at what
  was asked for rather than filling the set on my own initiative.
- **The gate still does not catch cut-system vocabulary.** It knows `BANNED_TERMS` — stale *names* —
  not the §12.4 scope table. Everything swept here was found by reading, not by the gate, and the
  same drift could return unnoticed. Wiring §12.4 into `gsstyle.BANNED_TERMS` is the obvious
  follow-on and is a ticket of its own.

---

# Extension 2 — FirstSight.Statue, and three stale control bindings

*Reopened again on Michael's instruction ("add the FirstSight.Statue prompt").*

## Generate

**`FirstSight.Statue` — generated, not written by me.** Same loop as the statue barks, brief shaped
against its two existing siblings. **5/10 → 9/10, PASS.** Trace at
`content-pipeline/out/style/statueprompt.trace.json`. Inserted directly after `FirstSight.Windmill`
rather than appended, so the three required objectives read as a set in the DataTable:

> **King's Statue: pull it down, the lie falls with it — Grapple (Q)** (64 chars)

Two schema corrections to the model's output, neither of them craft: the `trigger` came back as a
prose sentence rather than the key `FirstSight.Statue`, and `control` came back as
"Q — weapon wheel, Grapple slot" where its siblings use the bare key.

**Three stale control bindings, found while checking that row against its siblings.** These are not
vocabulary drift — they teach the wrong button:

| Row | Was | Now | Why |
|---|---|---|---|
| `FirstSight.UnawareGuard` | `Hold E` | `Hold F` | interact moved to F in #058/#061 |
| `FirstSight.LivestockPen` | `Hold E` | `Hold F` | same |
| `FirstCourier.CargoAndHorde` | `Point` | `R` | the order wheel is `IA_HordeOrder` on R; "Point" names no key |

`E` is the **traversal/climb** key, so the first two were telling a new player to press climb to loot
a pig and to take down a guard. This is the third instance of the same defect today (the civilian
prompt was the first) and they all trace to the same cause as everything else in this ticket: the
content was generated against a document whose control map predates three remaps.

## Evaluate

**Verified:** `style_lint` over **all 41 rows** across the three files returns **0 findings**; the
drift gate reports **CLEAN, 10 checks**. Every `FirstSight.*` required objective now has a prompt
(Market, Statue, Windmill), and no `control` field references `E` any more.

**Not verified:** unchanged from the first extension — none of this has fired in game, because Block
F's DataTable, first-encounter tracker and HUD note widget do not exist. The lint judges the writing;
it cannot tell you a prompt appears.

**The control audit was luck, not process.** I found those three rows because I printed the whole
prompt table to check where the new one had landed. Nothing in the gate looks at control bindings,
and nothing would have caught them — the same limitation already recorded for cut-system vocabulary.

## Refine

Normalised the generated `control` to the bare key for consistency with its siblings, and set the
trigger key by hand. Did **not** touch the Overlord line or the HUD wording — that is what the tool
was run for.

**Left undone:** a gate check that the `control` column only contains bindings the GDD's §4 table
actually lists. That would have caught all three of today's instances mechanically and is the
obvious companion to the cut-vocabulary check already owed.
