---
id: 202
title: Supplement to 200 - repoint the bark machine's RAG corpus at the canonical GDD (it still reads the frozen assignment doc)
agent: claude-gddlock
status: done
claimed: 2026-08-19T22:01Z
build: none
waiting_on:
evaluated: 2026-08-19T22:09:28Z
observed: 2026-08-19T22:09:27Z | the bark machine drew on both documents in their intended roles - the statue rows it produced cite GDD section 6 and section 4 for the grapple-and-haul mechanic and A02 section 2.10 for the register - where before the repoint it could only ever have reached the frozen document
scenario: python driver invoking gsstyle.run_case over a 149-chunk corpus in D:\goblinRaid\content-pipeline, statue brief, three refinement rounds scoring 7 then 8 then 10 out of 10
files: 
  - content-pipeline/gsrag.py
---

## Goal

Supplement to 200 - repoint the bark machine's RAG corpus at the canonical GDD (it still reads the frozen assignment doc)

## Generate

One edit, to `content-pipeline/gsrag.py`'s `SOURCES` and `AUTHORITY`.

The RAG corpus that feeds every bark, whisper and tutorial prompt read **only** the repo-root
`goblin-siege-design-document.md` — the document #198 froze this morning as the submitted Assignment
#02 artifact. Ruling 1 made the export canonical and nothing told the bark machine.

- `SOURCES` now leads with the **canonical GDD** (`GoblinSiege 5.8/docs/goblin-siege-gdd.md`) and the
  **decisions ledger**, keeps the frozen A02 document under a renamed key labelled *"tone and register
  only, NOT authoritative"*, and keeps the four race briefs.
- `AUTHORITY` restated as an explicit order — **ledger > canonical GDD > A02 > race briefs** — and
  names the four specific things A02 still asserts that are no longer true: bind/capture and prisoner
  rope chains (cut), the watchtower/bell/well (deferred), the granary (removed), and a control map
  from before three remaps.

## Evaluate

**Verified:** all seven sources resolve on disk (checked each `Path.exists()` after the edit), and
the corpus loads **149 chunks**. The proof it is working is in the generated output rather than the
config: the statue rows cite `GDD§6#6` and `GDD§4#0` for the mechanic and `A02§2.10` for the tone —
exactly the split the new `AUTHORITY` describes, drawn from both documents in their intended roles.

**This is the root cause of #200's whole sweep, not a tidy-up.** The prisoner whispers, the bell
lines and the `Torch: Q` binding were not sloppy writing — the generator was faithfully grounded in a
document that had stopped being the design. Fixing the rows without fixing the corpus would have left
the next generation run to reintroduce every one of them.

**Not verified:** the ledger has never been retrieved *against* — no query has yet pulled a `LEDGER`
chunk, so its chunking and usefulness as a retrieval source are untested. The statue run drew from
`GDD` and `A02` only. It will get exercised the first time someone generates against a ruling.

## Refine

Kept A02 in the corpus rather than dropping it, which was my first instinct. The canonical GDD is an
**export** — it summarises the tone chapter away, and the tone chapter is what a bark is actually
written from. Removing it would have made every future bark factually safer and noticeably worse.
Demoting it to register-only with an explicit authority order gets both.

**Left undone:** the `LEDGER` source is configured but unproven, as above.
