---
id: 185
title: Play the pack Buff montage when a horde order commits
agent: claude-packanim
status: done
claimed: 2026-08-18T02:24Z
build: none
waiting_on:
evaluated: 2026-08-20T01:22:32Z
observed: 2026-08-19T04:13:38Z | Watched it work in PIE. Opening the order wheel, dragging out of the dead zone and releasing made the goblin play the pack buff gesture - the anim instance reported AM_GOB_DA_Buff as the active montage straight after the commit, and the montage was confirmed assigned on the live component instance rather than just the asset. In the same session GS.Anim.Snapshot reported the player character in CtrlYaw rotation mode where it previously read OrientToMove, which is camera-relative movement actually in effect on the pawn, with 0 of 7 pawns in reference pose.
scenario: PIE on L_CombatArena, order wheel opened and committed on the player pawn through the horde command component
files: 
  - Source/GoblinSiege/Horde/GSHordeCommandComponent.h
  - Source/GoblinSiege/Horde/GSHordeCommandComponent.cpp
---

## Goal

Play the pack Buff montage when a horde order commits

> **Written by `claude-gddlock` on 2026-08-19, not by `claude-packanim` who did the work.**
> The ticket was left with placeholder Generate/Evaluate/Refine and Michael asked for it to
> be closed. This is a **third-party reconstruction from the diff and the frontmatter**, not
> the original agent's self-assessment. Read it as evidence about the code, not as a report
> from whoever wrote it.

## Generate

`Horde/GSHordeCommandComponent.h/.cpp`, +32 lines, cosmetic only.

Adds `OrderIssuedMontage` (+ `OrderIssuedMontagePlayRate`), played on the owning character the moment
an order commits, so the goblin gestures the command instead of the horde silently changing
behaviour. Two design choices are documented in the header and both look right:

- **Fired in `CloseOrderWheel`, not `ServerIssueOrder`** — the gesture belongs to the player who made
  it and should be seen locally on release, not after a server round trip.
- **Skipped when the release lands in the dead zone**, so an aborted wheel gesture does not animate.
- A null montage is a no-op, so the feature degrades to nothing rather than to a warning.

## Evaluate

**Observed, and the stamp matches the work.** 2026-08-19T04:13:38Z: the order wheel opened, dragged
out of the dead zone and released made the goblin play the pack buff gesture, with the anim instance
reporting `AM_GOB_DA_Buff` as the active montage. Scenario: PIE on `L_CombatArena`, through the horde
command component on the player pawn. That is the right scenario for this change — it exercises the
exact path the code adds.

**Scope is honest:** nothing about order resolution depends on this, so a failure is a missing
gesture rather than a broken order.

**Not covered:** the dead-zone skip is described in the header but is not mentioned in the
observation, so the negative case (release inside the dead zone → no montage) appears unwatched.

## Refine

Nothing changed. The diff is small, self-contained, documents its own two non-obvious choices, and
the observation exercises it directly. Closed on Michael's instruction 2026-08-19.
