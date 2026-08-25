---
id: 296
title: Re-file 252: the world corruption rulings, and civilians corrupt the land faster than soldiers do
agent: claude-corruption
status: done
claimed: 2026-08-24T23:28Z
build: none
waiting_on: Docs only, no build. Ruling 62 (civilians corrupt more) plus the 252 citation repair; check_gdd CLEAN 10/10. Ready to close.
evaluated: 2026-08-24T23:30:14Z
observed: 2026-08-24T23:30:08Z | Ran the drift gate after repointing both documents: check_gdd.py reported CLEAN 10 of 10, exit 0, with systems_inventory still at 21 ids - and the GDD revision row now resolves to ticket 296, which is open and standing behind it, where before it resolved to the abandoned 252
scenario: python Tools/check_gdd.py -v from the repo root, after ruling 62 and both citation repairs were in place
files: 
  - docs/decisions-ledger.md
  - docs/goblin-siege-gdd.md
---

## Goal

Re-file 252: the world corruption rulings, and civilians corrupt the land faster than soldiers do

## Generate

Two jobs in one ticket: repair the citation #252 left broken, and settle the question it left open.

**`docs/decisions-ledger.md`**

- New block `## 2026-08-24 - atrocity corrupts faster than battle (world corruption, cont.)` carrying
  **ruling 62: a civilian kill corrupts the world MORE than an armed defender's.** Numbered 62
  because the recent blocks reach 61.
- The 2026-08-21 corruption block's ticket line now reads *"Tickets #252 (abandoned) and #296
  (re-filed)"* instead of *"Ticket #252"*.
- Its closing paragraph - previously the flagged open question - is replaced with a pointer to
  ruling 62 and a one-line record of why the ticket id changed.

**`docs/goblin-siege-gdd.md`** - the `v1.1` revision row now cites **#296**, not #252, and records
ruling 62 alongside 40-45.

**`Tools/CodeArchitect/features.json`** - **not claimed this time.** #252 claimed it and never edited
it; claiming a file I have no intention of touching makes the ticket look wider than it is and holds
the build gate against a file I am not using. The §12.1 row and the feature entry still ship together
at stage 6.

**Calls:** `claim` -> #296 (no conflicts) -> `set -Status active` -> `check_gdd.py -v`.

## Evaluate

**Verified by running the gate.** `check_gdd.py -v` -> **exit 0, 10/10**, `systems_inventory` still
**21 ids**. `never_cut_unwrapped` reads 4 items where the 2026-08-22 run read 5; that is #285 cutting
sneaking, not this ticket - I touched one table row in the revision history and nothing in §12.

**What #252 actually got wrong, since this ticket exists to fix it.** Not the content - every ruling
survived scrutiny and none was reverted. The failure was procedural and worth naming: a ticket left
in `review` across a session boundary is indistinguishable from a dead one, so it was abandoned as
bookkeeping while its edits stayed in the tree. That produced the one state the GDD's change rule is
written to forbid - **canon citing a ticket that was not standing behind it** - and it produced it
silently, because `check_gdd.py` validates structure and vocabulary, not whether a cited ticket is
alive. Nothing in the toolchain would ever have caught this.

**Adversarially:**

- **Ruling 62 is a design decision, not a verified fact.** Michael chose it; I did not test that it
  plays well. If slaughtering peasants turns out to be the boring optimum, 62 is what gets revisited
  first. The *multiplier* is deliberately left as a cvar so that revisiting costs no rebuild.
- **Ruling 62 writes a cheque the code cannot yet cash.** It requires telling a civilian from a
  militiaman at the kill hook. `GSCharacterBase.cpp:258` still classifies *anything not a goblin* as
  human, and its own comment has been asking for the "real case" since rulings 13/14. Civilians
  arrived in #275 as data assets, not a C++ class - so the discriminator is an archetype or a
  `Civilian` tag, and **that work is unwritten**. Ruling 62 makes it mandatory rather than optional.
- **The kill soft-knee of 12 in the plan is now known wrong** and is recorded as such in the new
  block. It was sized against ruling 19's finite 15-defender pool; the 2026-08-23 roster ruling makes
  castle guards Militia with *"a decent amount of them"*. I did **not** pick a replacement number
  here - that needs a counted roster, and guessing a second time is worse than leaving it flagged.
- **Still nothing has run.** This is docs. Every behavioural claim in the plan remains unobserved,
  and stage 1 has not been started.

**Owes `AGENT_STATE.md`:** a DECISIONS line - *"world corruption rulings 40-45 + 62; civilians
corrupt more than soldiers; re-filed from the abandoned #252."*

## Refine

**Changed in response to my own evaluation:** I dropped `features.json` from the claim. #252's own
Evaluate had already flagged claiming-without-editing as making the ticket dishonestly wide; repeating
it after writing that down would have been worse than doing it the first time.

I also **did not** re-file by copying #252 forward wholesale. The tempting move was a clean
duplicate that reads as though nothing went wrong. Instead both documents now say plainly that #252
was abandoned and why - the ledger's own rule is that a superseding entry names what it supersedes,
and a citation repair that hides the reason for the repair is the same drift one level up.

**Deliberately left undone:**

- **The civilian discriminator.** Ruling 62 requires it; it is code, it belongs to stage 3, and it is
  now a hard dependency of the kill hook rather than a nicety.
- **The kill soft-knee re-sizing.** Flagged, not guessed.
- **`features.json` and the §12.1 row** - stage 6, unchanged.
- **Stage 1.** The gate is open for the first time and the queue is empty, so it is genuinely
  available now - but it is a separate ticket with a separate build, not something to bolt onto a
  docs re-file.
