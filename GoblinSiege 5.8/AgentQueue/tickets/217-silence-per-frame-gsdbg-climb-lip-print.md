---
id: 217
title: Silence per-frame GSDBG climb/LIP print spam in BP_GSPlayerCharacter
agent: claude-acf
status: done
claimed: 2026-08-20T23:39Z
build: none
waiting_on:
evaluated: 2026-08-20T23:42:01Z
observed: 2026-08-21T00:09:19Z | A fresh 3,253-line log from a full horde fight contains ZERO GSDBG lines. The previous session log was 29,761 lines of which 24,936 (83.8%) were GSDBG|CLIMB and GSDBG|LIP.
scenario: GS.Horde.SpawnTest then GS.Horde.Order Attack on L_CombatArena, 2026-08-21 00:07, editor rebuilt.
files: 
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
---

## Goal

Silence per-frame GSDBG climb/LIP print spam in BP_GSPlayerCharacter

## Generate

Two `Print String` nodes in `BP_GSPlayerCharacter`'s EventGraph had `bPrintToLog=true` and fired
every frame:

| node_id | pos | emitted |
|---|---|---|
| `6FDB86A4419B036A73C502B5C8B172E9` | x=4786 y=20673 | `GSDBG|CLIMB|...` |
| `A00B0A494BB027199387E2A3B7CEBC74` | x=4986 y=20933 | `GSDBG|LIP|...` |

Measured cost: **24,936 of 29,761 lines in `Saved/Logs/MyProject.log` — 83.8% of the file**, 12,468
lines each. Every other Print String in that graph (8 of them) already had `bPrintToLog=false`, so
these two were the outliers, not the convention.

Both pins set to `false`. Compiled and saved.

## Evaluate

**NON-DESTRUCTIVE ON PURPOSE.** The nodes and their string-build chains are untouched; only
`bPrintToLog` flipped, and both already carried `bPrintToScreen=false`. Re-enabling is one pin, so
this suppresses the readout rather than deleting somebody's instrument — which matters, because the
climb work these were built for (#072/#076) is exactly the kind of system that has no other runtime
readout.

**Verified by read-back only, NOT by a watched run.** After compile the graph reports
`log=false screen=false` on both. That proves the asset, not the behaviour - the honest test is one
PIE session with a fresh log.

Noted while saving, NOT caused by this change and not investigated:
`RuntimeWarning: Divide by zero: Divide_DoubleDouble` fires during `save_loaded_asset` on this
Blueprint. Pre-existing; worth its own ticket if it turns out to matter.

**What this cost us before it was fixed:** the spam buried the `LogGSAI` event stream while
diagnosing the horde Attack order, and had to be filtered out line-by-line to read the result. That
is the argument for the fix, not tidiness.

## Refine

Left the string-building chains connected. They are pure nodes, so they still evaluate each frame to
feed a print that now discards the result - real but small waste next to the log I/O just removed.
If the climb readout is wanted again it should come back as a cvar-gated C++ log beside
`GS.Climb.LogLedge` rather than an unconditional per-frame Blueprint print, and at that point the
chains should be deleted rather than re-enabled.
