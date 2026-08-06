---
id: 031
title: One building per ROOF: cover every roofed structure on the map
agent: claude-raid
status: done
claimed: 2026-08-06T09:56Z
build: none
waiting_on:
evaluated:
files: 
  - tools/hamlet/gs_buildings.py
  - Content/Maps/L_Tutorial_Island.umap
---

## Goal

One building per ROOF: cover every roofed structure on the map

## Generate

Rewrote tools/hamlet/gs_buildings.py to cluster ROOF pieces (XY 250 / Z 1200) instead of all
shell pieces, on the reasoning that roofs do not touch between neighbours even where walls do.
Produced 71 building objectives from 245 roof pieces, up from 7. Saved L_Tutorial_Island.
Committed as 6913b39.

## Evaluate

SHIPPED, THEN FOUND WRONG BY MICHAEL'S ANNOTATED SCREENSHOT. Recording both halves.

What was verified: 71 objectives created, 0 adopting zero pieces, window ownership 107 -> 112
of 113, adopt radii min 626 median 1130. Those numbers are real.

What was NOT verified, and is the failure: that a cluster corresponds to a BUILDING. I said so
myself in the commit message - "my validation could not tell a split tavern from a tavern plus
fourteen neighbours" - and shipped anyway. It was a split tavern. Michael's annotation shows 47
downtown houses; buildings 16-27 and 41-69 sit 360-660 uu apart at z 2516-4085, which is one
structure cut into ~30 objectives. GS_Building_43 was placed on the windmill, the one thing
Michael crossed out on the map.

Root cause, found by asking the LEVEL instead of tuning a radius: the premise was false. The
245 "roof pieces" are roof TILES belonging to a handful of detailed kitbashed buildings -
House_2x1_T9 alone is one building with 46 of them. The village's actual houses are
SM_MERGED_House_* actors: ONE actor per whole house, 61 of them, with no separate roof piece at
all. No clustering distance could ever have found them.

DECISION for AGENT_STATE.md: a building on L_Tutorial_Island is a merged house actor or an
attached kit hierarchy - an identity carried by the level, not a distance to be tuned.

## Refine

Superseded by #033, which replaces distance clustering with the level's own grouping. Leaving
the 71 in place only until #033 rebuilds them, since they are derived data and #033 destroys
and rebuilds the set on every run.
