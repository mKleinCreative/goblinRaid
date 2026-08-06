---
id: 033
title: A building is a merged house actor or an attached kit hierarchy - not a cluster radius
agent: claude-raid
status: review
claimed: 2026-08-06T10:18Z
build: none
waiting_on:
evaluated: 2026-08-06T10:24Z
files: 
  - tools/hamlet/gs_buildings.py
  - Content/Maps/L_Tutorial_Island.umap
  - Source/GoblinSiege/Destruction/GSBuildingObjective.h
  - Source/GoblinSiege/Destruction/GSBuildingObjective.cpp
---

## Goal

A building is a merged house actor or an attached kit hierarchy - not a cluster radius

## Generate

tools/hamlet/gs_buildings.py - replaced distance clustering entirely. A building is now discovered
from identity the LEVEL already carries:
  1. SM_MERGED_House_* -> one actor IS one whole house. 61 of them.
  2. an attached hierarchy containing roof tiles -> the subtree IS the building. 6 of them
     (Innbase=the tavern, House_2x1_T9, House_2x1_L7_Detailed, House_1x3_10, WaterMill_Closed,
     SM_House_Window_A).
Excludes 21 Distant_* backdrop houses (off-island, unreachable), wells, bridges, fences, and
anything within 1500 uu of an existing objective. 67 buildings created; level saved.

GSBuildingObjective.h/.cpp - a merged house has no roof ACTOR and no window actor, so nothing could
ever light 61 of the 67. AdoptPieces now derives bHasRoofPieces from the adopted pieces, and for a
building with none, ContainsWorldLocation becomes the roof TEST: inside the mesh footprint and in
the top RoofZoneFraction (0.34) of its bounds. IgniteAtLocation then lights it as source Roof.
Kitbashed buildings still return false, so a torch on a facade is still refused.

Deliberately did NOT touch GSTorchProjectile.cpp - #030 holds it and has right of way. Routing
through the two virtuals the torch already calls made that unnecessary.

## Evaluate

VERIFIED by evidence:
  - 67 buildings created, 61 merged + 6 hierarchy (script output).
  - PIE adoption: 67 buildings, min 1 / median 6 / max 440 pieces, ZERO adopting nothing.
  - The only ground truth available passes for the first time. All three buildings Michael
    identified by eye resolve to exactly ONE objective: the tavern is Innbase (498 pieces, one
    objective, matching "the tavern is just one objective"); the two others turn out to be
    WaterMill_Closed and House_1x3_10.
  - GS_Building_43 on the windmill is gone - the >=8 piece floor drops its 2-piece hierarchy and
    the 1500 uu objective clearance is the backstop.
  - 61 merged houses vs Michael's 47 downtown annotations: consistent, since 61 covers the whole
    island including outskirts, and his stroke count was downtown only.

WRITTEN BUT NEVER RUN: the C++. The build gate is shut on #030 and #032, so ContainsWorldLocation's
roof-region test has not compiled, let alone been thrown a torch. Until it builds, 61 of the 67
buildings still have no way in. This is the whole point of the ticket and it is UNPROVEN.

NOT FIXED, and worth Michael knowing: a merged house has its windows baked into the mesh, so it has
no breakable window - those 61 are roof-entry only. Michael's flow wanted "a torch into a window";
that half only exists on the 6 kitbashed buildings, which own all 113 window actors.

DECISION for AGENT_STATE.md: a building on this map is a merged house actor or an attached kit
hierarchy. Four sessions of tuning a cluster radius produced 7, 29, 52, 28 and 71 buildings and
never once produced a building; the level knew the answer the whole time.

## Refine

Changed in response to my own evaluation: hoisted the roof-piece count out of a UE_LOG lambda
argument (macro expansion plus lambda is a compile risk for no benefit), and took centres/spans for
hierarchies from ROOF tiles only rather than the whole subtree - Innbase's subtree spans 10,722 uu
because fences are attached to the same root, which would have adopted half the village and set a
threshold it could never meet. Capped at 3500.

Leaving undone: the 440-piece Inn needs 66% burnt, which is the market's unwinnability risk in a new
place. Not adjusting it blind - it wants a PIE burn test after the build, not another guess.
