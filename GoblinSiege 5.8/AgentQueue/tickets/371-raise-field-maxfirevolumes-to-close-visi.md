---
id: 371
title: Raise field MaxFireVolumes to close visible gaps between fire patches
agent: claude-fire
status: done
claimed: 2026-08-30T05:22Z
build: none
waiting_on:
evaluated: 2026-08-30T09:08:21Z
observed: 2026-08-30T09:08:06Z | Same as #370 - reverted alongside it. bConsolidateFireVisual defaulted back to false in GSFieldFireObjective.h after Michael watched two failed consolidated-visual attempts live.
scenario: Live PIE test of a burning field, player watching from ground level
files: 
  - Source/GoblinSiege/Destruction/GSFieldFireObjective.h
---

## Goal

Raise field MaxFireVolumes to close visible gaps between fire patches

## Generate

Merged with #370 - full account of the work is written there. Summary specific to this ticket's own
title: `MaxFireVolumes=16` was already committed before this ticket touched anything (raised earlier
this session per HANDOFF-2026-08-30.md, verdict "still rows, not enough"), so there was nothing left
to raise. The `GSFieldFireObjective.h` claim this ticket holds is the file the actual fix (the
consolidated wide fire visual, #370) lives in alongside `GSFieldFireObjective.cpp` (not originally
claimed by either ticket - see #370's Generate for the flag).

## Evaluate

See #370 - same code, same "written not built" status, same build-gate block.

## Refine

See #370 for the full account: two live looks at the consolidated visual both failed (sprite =
"little puffs, hard to tell where it is"; fluid = "sperm shaped objects... even more dangerous"),
reverted `bConsolidateFireVisual` to `false` in this ticket's own claimed file
(`GSFieldFireObjective.h`). Every pooled volume draws its own flame again - the original "rows of
separate fires" complaint this ticket and #370 were both trying to fix is back, unsolved. Closing
alongside #370 rather than leaving one of the pair open with nothing further to do.
