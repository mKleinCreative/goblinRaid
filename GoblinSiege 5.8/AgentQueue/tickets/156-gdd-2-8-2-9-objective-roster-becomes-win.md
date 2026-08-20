---
id: 156
title: GDD 2.8/2.9: objective roster becomes Windmill/Market/Statue; granary removed, field demoted to optional
agent: claude-gdd
status: done
claimed: 2026-08-14T22:43Z
build: none
waiting_on:
evaluated: 2026-08-14T22:46:17Z
observed: UNOBSERVED 2026-08-14T22:46:17Z - A design document has no runtime behaviour to watch. Verified by grep: zero granary references remain in the GDD, statue now appears 11 times, one file modified. The DOWNSTREAM consequences are unverified and flagged in the ticket - the banked prompt content still references the granary.
scenario: none - never run
files: 
  - ../goblin-siege-design-document.md
---

## Goal

Michael's ruling, 2026-08-14, in response to a gap analysis that found the granary was the single
largest hole in the tutorial slice: *"update the GDD to get rid of Granary, when initially written I
had meant a different building, but had never created it. Windmill, Market and Statue works fine."*
Followed by a confirmation that the wheat field is **demoted to optional**, not cut.

So the required trio becomes **Market / Statue / Windmill**, with **wheat fields and houses as
optional objectives** worth points but not gating extraction.

**Why this is a good trade and not a retreat.** The granary was MISSING in every sense - no mesh, no
Blueprint, no placed instance, and `AGSDestructibleObjective` (the class that was granary-shaped)
never derived from `AGSBurnObjectiveBase`, so `UGSRaidDirector::UpdateTrackedCarriers` could never
have counted it. Meanwhile the Market objective is already implemented (`GSMarketObjective`, placed
in L_Tutorial_Island) and the Statue lands on `AGSDestructibleObjective` exactly as built - a
GeometryCollection released on destruction, which is what toppling stone wants and what burning
never did. Two of the three required objectives now sit on code that exists.

The Statue is also the strongest of the three thematically: GDD 2.10's whole joke is the propaganda
statue of the king who supposedly wiped out the goblins standing in every village centre. Toppling
it is the tutorial's thesis, not set dressing.

## Generate

Ten edits to `goblin-siege-design-document.md`. Granary appeared in ten places, not the two the
section headings suggest - a grep-first approach is what caught the other eight.

| Line | Was | Now |
|---|---|---|
| 15 | "three **burn** targets... granary, wheat fields, windmill" | "three **raid** targets... market, windmill, king's statue" |
| 17 | Win: "**burn** all three... granary, field, windmill" | Win: "**destroy** all three... market, statue, windmill" |
| 49 | HUD names "granary, field, windmill"; "a granary hides in the village center" | "market, statue, windmill"; both core objectives in the centre - "two of your three targets in the one place you least want to linger" |
| 51 | "each target **burns** differently" | "each target **gives** differently" - market blazes, mill torches, statue must be brought down; fields moved to optional |
| 139 | Core village: "Its burn objective is the village granary" | Core carries **two** of three: market + statue, with the statue's thematic weight spelled out |
| 143 | "all three burn objectives... one granary, one field, one windmill" | "all three **required** objectives... one market, one statue, one windmill" + fields and houses as optional, and *why* (three kinds of destruction: a burn, a set piece, a demolition) |
| 145 | Prompt list: granary, field, windmill | market, statue, windmill, optional fields. Also dropped "and the bind choice" - bind was cut the same day |
| 164 | "Objective **burned** (granary / field / mill) 100 each" | "Objective **destroyed** (market / statue / mill) 100 each" + an explicit TBD line for optional-objective values |
| 217 | "the granary's burn-triggered physics collapse" | "the destructible objective's physics collapse - the geometry-collection class that now carries the statue" |
| 225 | "a granary, a field, and the windmill all locatable" | "the market, the statue, and the windmill all locatable" |
| 330 | W2: "granary collapse, spreading field fire, the mill" | W2: "the market blaze, the statue's collapse, the mill's two stages" |

**The vocabulary change is the substantive part, not a rename.** The document said "burn objectives"
throughout because all three burned. A statue does not burn - it is demolished. Every generic use of
"burn" that covers the whole trio became "destroy" / "raid targets" / "each target gives
differently", while "burn" was kept wherever it is still literally true (the market blaze, the mill,
the field fire). Getting this wrong in one direction would have written the field's fire language
out of the doc; in the other it would have left the statue being set alight.

**Also folded in, from the same day's decisions:** the bind choice was cut, so line 145's "a civilian
pair offering both the takedown and the bind choice" became "offering the takedown choice". Civilians
themselves stay - Michael kept them on the strength of the existing bucket-brigade logic.

## Evaluate

**Verified by grep, which is the right instrument for this job** - a design document has no runtime.
`grep -iE "granary|granaries"` over the file returns nothing. "statue" now appears 11 times where it
previously appeared only in 2.10's propaganda passage. `git status` shows one modified file and
nothing else touched.

**What I did NOT do, deliberately: invent a score for the optional objectives.** GDD 2.9 now carries
`optional objectives (wheat field, house) - value TBD, deliberately below 100 so they never rival the
required trio`. Michael said houses stay as optional objectives; he did not say what they are worth,
and picking a number would have looked like a decision he had made. The constraint (below 100) is
inferable from the required trio's value; the number is not.

**Unresolved tension worth naming, NOT fixed here.** GDD 2.8 still describes the hamlet as
hand-authored, and 2.8 records that decision as made on 2026-07-23 with reasoning about placement-bug
risk. On the same day as this edit Michael decided to *"pick up the map generation for the tutorial
level"* - which contradicts that paragraph. I did not touch it, because reversing a recorded, dated,
reasoned decision is a bigger edit than a roster swap and deserves its own ticket and its own
reasoning in the document. Right now the GDD says hand-authored and the plan says generated. That
should not sit unreconciled for long - it is exactly the "two documents disagreed for months" failure
CLAUDE.md complains about.

**Owed to AGENT_STATE:** the required objective roster changed, and two of the three now sit on code
that already exists (`GSMarketObjective` placed; `AGSDestructibleObjective` for the statue). The
gap analysis that recommended ~1 day of granary work is superseded.

## Refine

**Changed after self-review:** the first pass rewrote line 51 as "each target burns differently" with
the statue awkwardly appended. Reworked to "each target gives differently" so the sentence's own verb
stops contradicting one of its three examples.

**Deliberately left undone:** the hand-authored-vs-generated contradiction above; the optional-objective
score values; and any edit to the four race design briefs or the settlement-generator spec, which may
carry their own granary references - this ticket claimed the GDD alone. A follow-up grep across
`race-design-*.md` and the technical docs is worth doing before those numbers get quoted onward.
