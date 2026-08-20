---
id: 158
title: Re-export the CodeArchitect GDD: roster, controls, map ruling and real build status
agent: claude-gddexport
status: done
claimed: 2026-08-14T23:05Z
build: none
waiting_on:
evaluated: 2026-08-14T23:07:52Z
observed: UNOBSERVED 2026-08-14T23:07:52Z - A design-document export has no runtime of its own. The one thing that would settle it - running the Code Architect against the reshaped file to confirm ca/ still parses it - has not been done. The build statuses are transcribed from the 2026-08-14 survey, not re-derived.
scenario: none - never run
files: 
  - Tools/CodeArchitect/docs/goblin-siege-gdd.md
---

## Goal

Michael: *"Tools/CodeArchitect/docs/goblin-siege-gdd.md we need to reupdate the GDD to match modern
realities of where we are."*

#157 flagged this file as a stale duplicate to delete. **That was wrong, and checking first is what
caught it.** It is not a duplicate: it is a summarized *export* (168 lines against the canonical's
345), and `ca/config.py:48-54` reads it as the Code Architect's design input with a fallback path.
Deleting it breaks the tool. It needed re-exporting, not removing.

## Generate

Full rewrite, 168 -> 272 lines. The growth is status honesty, not prose.

**THE FINDING THAT MATTERS MOST: the export was AHEAD of the canonical doc, not behind it.** Line 19
of the old export already read *"three objectives means three TYPES: at least one windmill, at least
one field, and the market"*, and line 63 recorded *"Granary deferred out of the tutorial set
(Q-29)"*. So a July ruling deferred the granary, the code was built to Field/Windmill/Market, and
the canonical GDD was never updated to match. Today's decision did not overturn a design - it
reconciled the canonical doc with a ruling made and implemented weeks ago. **The canonical document
was the stale one.** Worth remembering the next time an export and a canon disagree: check which is
older, not which is derived.

**What changed, by section:**

- **Header** - re-export date; and corrected a claim that had quietly gone false: the canonical doc
  is no longer "the Claude project's copy", it lives in this repo at the root. An export that
  misnames its own source is how a stale copy survives.
- **New status-honesty preamble** - states that the 2026-08-04 export described what was *designed*,
  and that §12.1 now grades BUILT/WIRED/SKELETON/MISSING off the 2026-08-14 survey (6 of 122 built).
- **§1 Vision** - "co-operative... you and your goblin warband" -> singular. Win condition rewritten
  to Market/Statue/Windmill with field and houses optional, and a note that the code change is
  smaller than the prose change because only one type actually swaps.
- **§4** - retitled "classes & weapons" -> "controls & kit" and the whole control map rebuilt. The
  old export had `Q` torch toss, `G` horn, `E` interact; #039-#041 took Q for the weapon wheel and
  retired IA_ThrowTorch, #058/#061 moved interact to F and gave E to traversal, #122 moved block to
  RMB and retired G. **Every binding in that section was wrong.**
- **§5** - order-wheel reality (Attack/Follow work, Hold/Loot/Smash inert); pool-size conflict
  between this export's "cap 10" and the canonical's 20 flagged rather than silently resolved.
- **§6** - the map section rewritten around the week-2 timebox; the honest description of
  L_Tutorial_Island as the Dreamscape demo island and its 185 MB per-save LFS cost; per-objective
  behaviour including the windmill's two stages and a note that neither exists in code.
- **§7** - bind cut recorded; the perception-component-on-zero-actors defect called out.
- **§10/§12** - scoring defects (AddLoot has no callers; houses pay ~1620) and the four-rung
  inventory.
- **§12.2** - build plan re-cut into eight blocks that match the current critical path, each with a
  watchable exit test.
- **§13** - Michael's 2026-08-14 rulings appended to the ledger pointers.

**Five live defects are now recorded in the document itself**, as blockquotes next to the system
they break, rather than living only in ticket history: the unset `InteractAction`,
`UGSSightPerceptionComponent` on zero actors, `AddLoot` with no callers, the missing `CarrySocket`,
and houses dominating the score. The Code Architect reads this file; it should not be told the
interact framework is "designed" when the input action has never been bound.

## Evaluate

**Verified by grep.** Every surviving "granary" mention is deliberate and records the change
(superseded / replaced / needs-regenerating). No "co-operative", no `SHELVED`, no old bindings.
One file modified; `git status` clean otherwise.

**NOT verified, and this is the honest limit:** the build statuses in §12.1 are transcribed from the
2026-08-14 survey workflows, not re-derived. That survey graded 122 pieces and downgraded 10 under
adversarial verification, so it is better evidence than a guess - but I did not re-check a single
one of them while writing this file. If a status here is wrong, it is wrong because the survey was
wrong, and it will be quoted onward as fact by a tool that cannot tell the difference.

**Nobody has run the Code Architect against the new export.** The file parses as markdown and keeps
the section numbering the old one used, but whether `ca/` actually consumes the reshaped §12 tables
is untested. The §12.1 column headers changed from "Status" prose to four fixed grades, which is
exactly the kind of thing a parser might care about.

**Owed to AGENT_STATE:** a derived export outlived its source's accuracy by ten days and was nearly
deleted as redundant. The rule that falls out: before deleting a duplicate document, find out who
reads it and which copy is older.

## Refine

**Changed after self-review:** the first pass carried the old export's "Scaffolded" status word into
the new inventory for three systems. Replaced - "scaffolded" is precisely the word that lets a class
that compiles pass as a system that runs, and this project has paid for that conflation repeatedly.
Four grades with a stated definition, or nothing.

**Deliberately left undone:**

- **The pool-size conflict (10 vs 20)** is flagged in §5, not resolved. It is a design number and
  Michael's to settle.
- **The optional-objective score value** stays TBD, matching #156.
- **`out/runs/*/blackboard.md`** - four historical Code Architect run records also mention the
  granary. They are records of what those runs saw at the time and should NOT be retconned.
- **The banked prompt content** still needs regenerating against the new roster (#156 flagged it;
  §12.1 row 15 now records it in the export too).
- **No re-run of the Code Architect** to confirm the new file parses. That is the one thing that
  would move this ticket off UNOBSERVED.
