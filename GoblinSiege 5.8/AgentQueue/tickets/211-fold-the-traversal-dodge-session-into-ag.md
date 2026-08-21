---
id: 211
title: Fold the traversal/dodge session into AGENT_STATE: two dodge systems, the Started pin, the horde follow competition, and what the UNOBSERVED closes leave unproven
agent: claude-statelog
status: done
claimed: 2026-08-20T20:55Z
build: none
waiting_on:
evaluated: 2026-08-20T20:56:36Z
observed: UNOBSERVED 2026-08-20T20:56:36Z - AGENT_STATE.md is agent memory - a markdown file with no runtime, so there is nothing to watch by construction. Its only real test is whether a later session behaves differently, which cannot be seen today. What it RECORDS is observed and cited: Michael signed off dodge, climb, vault and mantle working in PIE on 2026-08-20, and the 63-sample dodge log backs the directional fix.
scenario: none - never run
files: 
  - AGENT_STATE.md
---

## Goal

Fold the traversal/dodge session into AGENT_STATE: two dodge systems, the Started pin, the horde follow competition, and what the UNOBSERVED closes leave unproven

## Generate

Four entries at the top of `AGENT_STATE.md`'s DECISIONS, covering the traversal/dodge session:

1. **Two dodge systems** - the C++ ability was correct all along; `BP_GSPlayerCharacter` held a
   second, complete dodge whose four montage variables all pointed at `AM_GS_Dive_RM`. Includes the
   two traps that cost the most: **Michael was pressing `E`, which was `IA_Traverse`, not dodge**, and
   **a Hold trigger does not stop the `Started` pin firing on press**. Records Michael's sign-off that
   dodge, climb, vault and mantle now all work.
2. **A stamina cost below the regen-during-the-move is free** - the arithmetic behind
   `DodgeStaminaCost = 30`, and why the obvious 8-to-18 range would have cost nothing.
3. **An ordered goblin used to publish a follow target** - the `Follow Summoner` / `Chase Target`
   competition, why it was fixed in the controller rather than the tree, and that **the identical
   shape is still live in Frenzy**.
4. **What the UNOBSERVED closes of #204, #205, #207, #209 and #210 leave unproven** - the line each of
   those closures owes this file.

## Evaluate

**This exists because nothing reads old tickets at run start.** Nine tickets closed today, five of
them UNOBSERVED, and every one of those closures printed the same reminder that it owed this file a
line. Without them the next session re-derives that `E` is traversal, or "fixes" the C++ dodge again.

**The most valuable entry is the one about my own mistakes, not the fix.** Three theories - clip
finishing early, a 1.5 play rate, swapped root-motion axes - were each reasoned from source and each
died on measurement. That pattern is worth more to the next agent than the eventual answer, and it is
recorded as such rather than smoothed into a clean narrative.

**Verified only as text.** The entries are in the file under the right heading; there is no runtime to
watch. Whether they change a later session's behaviour cannot be observed today.

**Deliberately recorded as still-unproven:** #209's cost has never been *seen* to refuse a dodge -
"dodge works" is not "the cost bites" - and #210 is compiled but nobody has watched an ordered goblin
stop oscillating.

## Refine

Wrote the Frenzy sibling into the #210 entry in the imperative ("if the staring happens with no order
issued, that is this") rather than as a footnote, because that is the form most likely to survive
being skimmed by whoever hits the symptom next.
