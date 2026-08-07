---
id: 056
title: GDD 2.6: half-confirmed means confirmed-but-uncorroborated (Michael's ruling on the 2.4/2.6 ambiguity)
agent: claude-content
status: done
claimed: 2026-08-07T02:13Z
build: none
waiting_on:
evaluated: 2026-08-07T02:14:52Z
files: 
  - ../goblin-siege-design-document.md
---

## Goal

GDD 2.6: half-confirmed means confirmed-but-uncorroborated (Michael's ruling on the 2.4/2.6 ambiguity)

## Generate

Docs only - no C++, no .uasset, no build. One file: `goblin-siege-design-document.md`.

The ambiguity was surfaced by the content pipeline (ticket 052) and ruled on by Michael:
**"half-confirmed means confirmed but uncorroborated."** Two edits, both wording:

- **S2.6 (line 112)** - "a goblin half-confirmed" -> "a goblin **confirmed but uncorroborated**",
  with a parenthetical defining it: a sighting that held the full ~1.5 seconds per S2.4, one guard
  certain and nothing else backing him up, and an explicit statement that a look broken before the
  confirm lands is not a signal at all.
- **S2.4 (line 85)** - the detection rule now says an early-broken look "is not a partial signal,
  it is *no* signal, and it costs you nothing", and that a confirm "becomes exactly one
  uncorroborated soft signal (S2.6)" requiring a second signal to escalate.

## Evaluate

**This was a naming bug, not a mechanics bug.** S2.4 already said "get confirmed and the town
moves to Suspicious", which is exactly Michael's ruling - the two sections were consistent in
substance the whole time. The only defect was the phrase "half-confirmed", which reads as
"partially confirmed" (a look that did *not* hold 1.5s) when it meant "confirmed by one witness,
uncorroborated by any second signal". No numbers, thresholds, phases or scoring changed, and no
other section referenced the term.

**Verified:** `grep -c "half-confirmed"` returns 0 - the misleading phrase is gone from the
document. Both edited lines re-read in full afterwards; each is a single grammatical sentence in
the doc's existing voice, and the two now cross-reference each other (S2.4 -> S2.6 and back), so
a reader landing on either section gets the whole rule.

**Not verified, because it cannot be here:** nothing in the running build reads this text. The
detection model is C++/BT work owned by the Stealth & Town agents; this ticket changed the spec
they build from, not their code. If `~1.5s confirm -> one soft signal -> second signal escalates`
is already implemented differently, this edit does not fix that - it just means the doc now
states the intended rule unambiguously enough to check against.

**Owed to AGENT_STATE.md:** DECISION (2026-08-07, Michael) - a sighting that breaks before the
~1.5s confirm produces **no** signal and costs the player nothing; a sighting that confirms
produces **exactly one** uncorroborated soft signal (S2.6), and escalation past SUSPICIOUS still
requires a second signal. This closes the OPEN QUESTION filed in ticket 052.

## Refine

Nothing changed on review, and here is why the first pass survives scrutiny: the smallest
possible edit was tempting - swap the two words in S2.6 and stop. I did not, because the pipeline
demonstrated that a reader (the critic agent) could take S2.4 alone and reach the *opposite*
conclusion in good faith. Fixing only S2.6 would have left S2.4 still readable as "unconfirmed
looks are free" without saying what a confirmed one costs. Both halves now state the same rule.

Deliberately left undone:
- **The generated tutorial prompts were not regenerated.** `out/prompts.csv` row
  `Prompts_05` was written against the pre-ruling GDD, and the trace where the critic argues
  itself in circles is currently the clearest evidence in the Assignment 04 bundle that the
  pipeline found a real doc ambiguity. Regenerating is ~$0.80 and would produce a correct row but
  overwrite that evidence; it is Michael's call, not mine, and it is offered rather than done.
- **AGENT_STATE.md not edited.** Not claimed by this ticket, and another agent (055) is live in
  the queue. The DECISION line above is written out here for the orchestrator to fold in.
