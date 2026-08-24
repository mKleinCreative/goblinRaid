---
id: 288
title: "RULING: the primary player experience is the bar"
agent: claude-warren
status: done
claimed: 2026-08-24T21:40Z
build: none
waiting_on:
evaluated: 2026-08-24T21:20:01Z
observed: UNOBSERVED 2026-08-24T21:20:02Z - A triage principle recorded in the ledger. Nothing runnable. AGENT_STATE carries what is unproven - whether the bar is right for the whole project or only for the demo it was set during.
scenario: none - never run
files: 
  - docs/decisions-ledger.md
---

## Goal

Michael, closing #287: *"the scout looks like it has an axe on its back as well, but honestly? that's
fine for the moment. It doesn't bother the primary player and that's how we want to judge things from
now on. Primary player experience."*

That is a standing triage principle, not a comment about one axe, so it goes where it can be cited.

## Generate

**Ruling 59**: a defect is judged by what the **primary player** experiences. If it does not reach the
person holding the controller, it is not urgent - whatever it looks like in an outliner, a log or a
details panel.

The entry is careful to call it a **triage rule, not a quality rule**. It decides what gets fixed
*now*; it does not license shipping things that are wrong.

**Also recorded below the line, and explicitly not to be fixed in passing:** the scout carries a
visible second axe on his back while the primary weapon is drawn. Cause not investigated, because
Michael judged it not worth the time - which under 59 is the correct call rather than a shortcut.

## Evaluate

`check_gdd` CLEAN, 10 passed. No GDD change: 59 is how the team decides what to work on, not a scope
or content claim, so §12.4's table and the change rule are untouched.

**#287 produced one of each kind in a single session, which is why the ruling is worth having.** The
swinging axe blocking the camera probe was **above** the line - it happened to the player, on every
swing, and it was fixed the moment it was reported. The spare axe on the back is **below** it: visible
from a debug angle, invisible to the person playing. The old instinct treats both as defects because
both are wrong. 59 says only the first is a defect *today*.

**The entry spends three bullets on what 59 does NOT mean**, because a rule this quotable is easy to
misuse:

- It does not weaken *"somebody must have WATCHED it run"* - if anything it sharpens it, since the
  observation gate asks what a thing did and 59 says whose experience decides whether that mattered.
- It does not license silent breakage. Ruling 53's warning - a migration that changes how weapons
  feel has failed even if it equips - holds *because* feel is primary-player experience.
- It does not make below-the-line items vanish. They are **recorded**, not ignored. The difference
  between "accepted, with a reason" and "nobody noticed" is the entire value of writing it down.

**Not established:** whether this is the right bar for the whole project. It was taken while cutting
scope toward a demo, and a rule that is right for a demo can be wrong for a shipping game where an
outliner full of stray actors becomes a performance problem. Nothing about 59 forbids revisiting it;
it forbids *quietly* revisiting it.

## Refine

**The first draft made it a quality standard**, along the lines of "only fix what the player sees".
That is a materially different and worse rule - it reads as permission to leave things broken as long
as they are hidden, which is not what was said and not what #287 did. Rewritten as triage: the same
items exist, the same items are wrong, and the ordering changes.

**The spare axe was deliberately NOT investigated.** Reflexively finding the cause "while I am here"
would have been the exact behaviour ruling 59 exists to stop, in the ticket that records it.
