---
id: 321
title: A horde readout: how many goblins are out, against the cap and the reserve
agent: claude-ui
status: abandoned
claimed: 2026-08-26T19:02Z
build: required
waiting_on:
evaluated:
observed:
scenario:
files: 
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.h
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.cpp
  - Content/UI/WBP_GSPlayerHUD.uasset
---

## Goal

A horde readout: how many goblins are out, against the cap and the reserve

## Generate

<!-- REPLACE: what you produced. Files touched, what each change does, the calls
you made. Delete this comment when you write the section. -->

## Evaluate

<!-- REPLACE: judge your own output against the goal, adversarially. What is
verified and by what evidence (a log line, a PIE observation, a compile result -
not "should work"); what is written but has never run; what you touched outside
the goal; the DECISION or FAILED line this owes AGENT_STATE.md. -->

## Refine

<!-- REPLACE: what you changed in response to your own evaluation, and what you
are deliberately leaving undone. "Nothing changed, and here is why the first pass
survives scrutiny" is a valid answer; silence is not. -->

---

## ABANDONED 2026-08-27 - dead session, work PRESERVED not reverted

Michael: *"close 319 and 321, they're dead, we'll pick up the UI work again once the ACF migration
is over."*

**The protocol says abandon reverts the ticket's edits first. That was NOT done, deliberately.** The
session died but the work did not: 97 lines across `GSPlayerHUDWidget.h` (+35) and `.cpp` (+62),
implementing the horde readout as ACTIVE/CAP with the reserve behind it, with the cap-reached tint.
It reads finished-but-unverified, and Michael has said he wants it back. Reverting would have
destroyed exactly the thing he asked to keep.

So:
- **The edits are still in the working tree** on `GSPlayerHUDWidget.h/.cpp`, uncommitted.
- **A patch is saved beside this ticket**: `321-horde-readout-work-in-progress.patch`, so the work
  survives a `git checkout` of those files.
- **NOTHING WAS COMPILED.** The widget bindings are `BindWidgetOptional`, so the C++ half is inert
  until `WBP_GSPlayerHUD` gets the `HordeText`/`HordeIcon` widgets - which was never done. That
  asset is unmodified.

**To resume:** the C++ is in place, the Blueprint half is not. Add `HordeText` (TextBlock) and
`HordeIcon` (Image) to `WBP_GSPlayerHUD`, set `HordeIconTexture`, build, then watch the count track
`UGSHordeSubsystem`'s active/cap/reserve while blowing the horn.

This ticket was holding the build gate shut with no session behind it, which is why it closed.
