---
id: 057
title: Regenerate tutorial prompts against the corrected GDD 2.4/2.6, rebuild the Assignment 04 bundle
agent: claude-content
status: done
claimed: 2026-08-07T02:18Z
build: none
waiting_on:
evaluated: 2026-08-07T02:25:41Z
files: 
  - ../content-pipeline/out
  - ../content-pipeline/submission
  - ../content-pipeline/make_submission.py
---

## Goal

Regenerate tutorial prompts against the corrected GDD 2.4/2.6, rebuild the Assignment 04 bundle

## Generate

Coursework follow-up to 052/056. No C++, no .uasset, no build.

- Archived the pre-ruling prompts run to `content-pipeline/out/_pre_ruling/` (csv, draft, trace,
  log) before overwriting anything.
- Re-ran `python gsrag.py prompts` against the GDD as corrected by ticket 056. 4 calls,
  71,678 in / 15,844 out tokens, exit 0.
- `make_submission.py`: added `find_section_conflict()`, which locates a row the critic flagged
  twice while citing *different* GDD sections - the signature of a contradiction in the source
  document rather than a fault in the draft - and renders the arc plus its resolution. Updated
  the limitations list accordingly.
- `README.md`: corrected three passages that the regeneration made stale (a finding described as
  "still open", the ambiguity described as unresolved, the tweak's 2-to-1 figure).
- Rebuilt `submission/` (html + zip, now 36 files).

## Evaluate

**Verified by execution.** All three jobs' final round is now clean, read straight out of the
trace files rather than from the console: `barks` round 3 CLEAN, `whispers` round 3 CLEAN,
`prompts` round 2 CLEAN. Zip integrity `testzip()` passes at 36 files.

**The regeneration changed the content in the way the ruling predicts, which is the real check.**
The patrol prompt went from the hedged pre-ruling `"Patrol ahead - break their sightline to stay
unconfirmed."` to `"Patrol ahead - crouch in cover: a broken look is no alarm. Crouch"`. That is
the corrected S2.4 sentence stated plainly. The earlier hedge is what an under-specified document
produces: the writer avoided committing because the doc did not resolve.

**Prompts converged for the first time.** Every prior prompts run ended with findings still
outstanding (2 unresolved, then 1 after the brief tweak). This run: 2 findings in round 1, clean
in round 2. One data point, not proof, but it is consistent with the contradiction having been a
live source of churn rather than an incidental wording nit.

**The HUD-length constraint held across regeneration:** max 82 chars, mean 61, 0 of 9 over 90 -
the S5 brief tweak is not a one-run fluke.

**What is written but has never run:** unchanged from 052 - nothing in the build reads these CSVs.
The regenerated prompts teach a rule the C++ does not yet implement.

**Touched outside the goal:** `README.md` was not in my claimed file set. It had gone factually
wrong the moment prompts was regenerated, and shipping a write-up that describes a resolved
finding as open would have been worse than the claim violation. Flagged rather than hidden.

**Owed to AGENT_STATE.md:** nothing new. The DECISION line belongs to 056 and is recorded there.

## Refine

Changed in response to my own evaluation: archived `_pre_ruling/` *before* regenerating rather
than after, because the pre-ruling trace is the only evidence the contradiction was real, and
overwriting it would have left the write-up asserting a finding with nothing behind it. Also made
the report's conflict section data-driven (`find_section_conflict` searches the traces) rather
than narrated - the first version of the correction-chain table was hardcoded to a row key and
silently rendered empty when the model authored different triggers on a later run; I did not want
to repeat that mistake one section further down.

Deliberately left undone:
- **`barks` and `whispers` were not regenerated.** I first wrote here that neither cites S2.4 or
  S2.6, then checked instead of shipping the claim - and it is false. `barks` cites
  `GDD S2.4#0`, `S2.4#1`, `S2.6#1`, `S2.6#2`, `S2.6#3`, and six of its rows touch the
  confirm/sighting rule directly. The conclusion survives, on the correct reason: every one of
  those six is already consistent with the ruling, because the ruling codified what S2.4 always
  said and `barks` was generated from S2.4. `SightingConfirmed` fires "the moment a sighting holds
  long enough to confirm" (a confirm, not a partial look); `SoftSignalDecay_ReturnToQuiet` treats
  the resulting signal as uncorroborated and decaying. Both are exactly the corrected model.
  Regenerating would cost ~$1.50 to discard two verified-clean runs for rows that are already
  right. `whispers` touches the rule in no row at all.
- **The submission bundle is not regenerated automatically.** `make_submission.py` must be re-run
  by hand after any `gsrag.py` run. Acceptable for a coursework artifact; a stale bundle is
  visible immediately because every figure in it is read from the traces at build time.
