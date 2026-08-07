---
id: 052
title: Dynamic content pipeline: RAG over the GDD + critic loop, generating bark/whisper/tutorial-prompt data tables
agent: claude-content
status: done
claimed: 2026-08-06T23:12Z
build: none
waiting_on: 
evaluated: 2026-08-07T00:30:07Z
files: 
  - ../content-pipeline/gsrag.py
  - ../content-pipeline/README.md
  - ../content-pipeline/out
---

## Goal

Dynamic content pipeline: RAG over the GDD + critic loop, generating bark/whisper/tutorial-prompt data tables

## Generate

Coursework deliverable (Assignment #04), not engine work. **No C++, no .uasset, no editor
session, no build.** Everything lives in the new `content-pipeline/` directory at repo root.

- **`content-pipeline/gsrag.py`** — the pipeline. Chunks the GDD + 4 race briefs into 95
  heading-aware chunks; retrieves with Okapi BM25 + a heading-match boost (7-8 queries per job,
  union of per-query top-4, ~18 chunks); generates data-table rows with Claude Opus 5 under
  structured outputs; critiques with a second agent that must cite the chunk id proving each
  finding; revises; loops, always terminating on a verification pass.
- **`content-pipeline/README.md`** — the assignment write-up.
- **`content-pipeline/out/`** — three CSVs (barks 18 rows, whispers 14, prompts 9), plus per-job
  `draft.json` / `trace.json` / `trace.md`. `out/_run1_unverified/` holds the superseded first
  run, kept as evidence of the two-round correction chain.
- **`GoblinSiege 5.8/.gitignore`** — appended `.env` / `.env.*` (line 63). Outside the claimed
  file set; justification in Evaluate.

Three content types, each filling a gap the GDD names about itself: the bark sheet's
livestock-and-stealth extension pass (S3.2), the outstanding capture/purge Overlord verdict
lines (S3.2, S2.10), and the tutorial hamlet's first-time prompt rows (S2.8, S3.1).

## Evaluate

**Verified by execution, not assertion.** Final run: 24 calls, 420,248 in / 104,810 out tokens,
exit 0. `barks` and `whispers` both reached `round 3: clean`. `prompts` ended
`round 3: 1 finding(s) UNRESOLVED after 2 revisions` and says so in its trace rather than
shipping quietly. All three CSVs parse with zero empty cells; `--list` and `--retrieval-only`
both exit 0.

**The critic demonstrably works, and the loop caught its own reviser.**
`Barks_04_PatrolWaypointCheckIn` drafted "back before the hour" against S2.1's 5-7 minute
cadence; revision 1 "fixed" it to "inside the half hour", still wrong; round 2 caught that and
produced "back in five - six if the mud's bad". Full chain in `out/barks.trace.json`.

**Three real bugs, all found by running it, not by reading it.**
1. The round loop ended on a *revision*, so the rows that shipped were the one version nothing
   had checked. Found because the reviser's first correction was also wrong. Now runs up to
   `--rounds` revisions but always terminates on a check.
2. `alarm_state` was emitted as `QUIET/SUSPICIOUS/RAID/RAZED` from the GDD's prose, but
   `EGSAlarmPhase` (`Alarm/GSAlarmTypes.h:31`) is `Quiet/Suspicious/Raid/Razed`. UE matches
   DataTable enum cells to enumerator names exactly, so every row would have failed import.
   Caught only because claude-orchestrator flagged that no reader exists, which sent me to look
   at the enum. Now conformant - verified against the header.
3. The beta-fallback handler treated any `BadRequestError` as "SDK lacks fallbacks", so an
   unrelated billing 400 silently disabled refusal handling for the rest of the run. Now
   downgrades only when the fallback params themselves are rejected.

Also added streaming-retry: the first full run died on `RemoteProtocolError` mid-stream (the SDK
retries request *initiation*, not a socket that drops after headers), losing an
already-paid-for generation. Retry + a pre-critic `draft.json` checkpoint. Verified working -
`transient RemoteProtocolError; retry 1/3 in 2s` appears in `out/_run1_unverified/run.log`
followed by a successful continue.

**Touched outside the goal, deliberately:** `GoblinSiege 5.8/.gitignore`. Michael's `.env`
holding a live `ANTHROPIC_API_KEY` was untracked but *not ignored* - `git status` showed `??`,
so any `git add -A` would have committed the key. Ticket 007 has already done a repo-wide push
this project. Appending two lines to prevent a credential leak was the right call over leaving
it and filing a note; flagged here rather than buried.

**What is written but has never run:** nothing in the pipeline - every path executed. But the
*content* has no consumer. There is no `UGSBarkSubsystem` and no `FTableRowBase` anywhere in
`Source/`; the project has no DataTable infrastructure at all. These CSVs are data-table
*shaped* and their enum column is conformant, but nothing can import them yet. Authoring bark
sheets ahead of the runtime is the order S3.2 and the W7 schedule intend, but "designed order"
is not "wired", and the column names are a proposal, not a contract. claude-orchestrator was
right to compare this to `RangedAttackCooldownSeconds`; the difference is intent, not status.

**Owed to AGENT_STATE.md:**
- DECISION: content pipeline retrieval is lexical BM25, not embeddings - Anthropic ships no
  embeddings endpoint, and at 95 chunks the project vocabulary ("Warren", "soft signal", "First
  Spark Unseen") favours exact-term matching. Revisit if the corpus outgrows a few hundred chunks.
- DECISION: `.env` is now gitignored in the UE project; the API key must never be committed.
- OPEN QUESTION for Michael, surfaced by the pipeline and belonging in the decisions ledger:
  **S2.4 and S2.6 contradict each other on unconfirmed sightings.** S2.4 says breaking a look
  before the ~1.5s confirm means "you were never there"; S2.6 lists "a goblin half-confirmed" as
  a soft signal that moves QUIET -> SUSPICIOUS. The critic cited each section in successive
  rounds and reversed its own verdict. Both cannot be literally true, and the tutorial prompt
  that teaches this rule cannot be written until it is settled.

## Refine

Changed in response to my own evaluation: the verification-terminating loop, the enum casing,
the fallback-downgrade condition, the streaming retry, and the draft checkpoint - all listed
above with the evidence that caught each. Also corrected a false claim in my own README ("the
critic filed zero findings on the S2.9 score table" - two findings cite S2.9; the true, narrower
claim is that no finding concerned an incorrect *point value*), and a wrong query count (7 for
two jobs, 8 for `prompts`). Both were caught by verifying the write-up against the traces
instead of trusting my recollection of the run.

Deliberately left undone:
- **`Prompts_06` retains one flagged TONE_DRIFT** ("Let that be the last thing he does" - generic
  menace against an Overlord whose register is pompous, underwhelmed condescension), with the
  critic's correction supplied in the trace. S3.3 gives Michael exclusive authority over
  aesthetics and tone; an unresolved aesthetic call is precisely what an agent should escalate
  rather than settle. Applying it myself would also have made the "critic caught it" evidence
  circular.
- **No row struct or bark subsystem written.** Out of scope for this ticket and it would collide
  with the Town/Writer agents' territory. Whoever builds it should read
  `content-pipeline/out/*.csv` headers as a proposal and rename freely.
- **No `--rerank` run.** The flag is implemented but unexercised; BM25 retrieval was accurate
  enough on inspection (register queries land on S2.10, score queries on S2.9, windmill on S2.8)
  that paying for a reranking pass was not justified.
- **Nothing committed.** ~20 tickets of other agents' work sit uncommitted alongside this, and
  051/052 are open. Not my call - but whenever a commit happens, the `.gitignore` line must be
  in it or the key protection does not survive a fresh clone.
