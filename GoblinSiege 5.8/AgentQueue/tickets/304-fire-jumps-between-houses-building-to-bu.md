---
id: 304
title: Fire jumps between houses: building-to-building spread
agent: claude-warren
status: done
claimed: 2026-08-25T05:58Z
build: required
waiting_on: Michael: torch a village house and watch it spread - does it READ as fire travelling?
evaluated: 2026-08-25T06:27:26Z
observed: 2026-08-25T06:36:44Z | Michael played it and said the fire spread works great - torching a village house now carries the fire house to house across the village instead of stopping at the first gap, which is what he asked for when he wondered whether house fires travel like the wheat does.
scenario: Michael playing in PIE on L_Tutorial_Island, torching a house in the village cluster and watching the fire carry between buildings.
files: 
  - Source/GoblinSiege/Destruction/GSBuildingObjective.h
  - Source/GoblinSiege/Destruction/GSBuildingObjective.cpp
  - Source/GoblinSiege/Destruction/GSFlammableComponent.h
  - Source/GoblinSiege/Destruction/GSFlammableComponent.cpp
---

## Goal

Michael: *"Do house fires travel like the wheat does? it'd be quicker to burn house blocks if they
did."*

## Generate

**They already did - for about a third of the village - and nobody knew.** The whole of
`UGSFlammableComponent`'s spread system is implemented and correct: radius, a progress gate so a
glancing torch cannot chain the village instantly, an attempt interval, a per-attempt chance biased by
the neighbour's fire resistance, and a hard material gate. `TrySpread` overlaps `AllObjects`, so no
collision channel to get wrong.

**The only thing wrong was one number.** `SpreadRadius` defaults to **450uu**, and measured on Tutorial
Island the nearest PIECE-to-piece gap between neighbouring village houses is:

```
min 86    median 745    75% 1396    max 2056  uu
```

So at 450 only ~36% of village houses can ever reach a neighbour and a torched village stops at the
first gap.

**`AGSBuildingObjective::PieceSpreadRadius`** (default **1200**), applied to every piece the building
adopts - including pieces that already had a flammable component, because a cart standing inside the
adopt radius is part of that house as far as fire is concerned.

**On the BUILDING, not on the component's default.** Fences, haycarts and market stalls keep the
tighter 450/600 they were tuned with, so giving houses village-sized reach cannot silently turn every
hedgerow into a 12-metre firebomb. `UGSFlammableComponent::SetSpreadRadius` was added for it rather
than making the field public - it is authored data, and the one legitimate moment to change it is
adoption.

## Evaluate

**Two runs, and the second is the one that counts** because the first set the radius by hand at
runtime and the second used the built-in value with nothing touched:

```
runtime override 1200   village 21/36   island 24/67
BUILT-IN 1200           village 20/36   island 23/67 (34%)   fire travelled 134 m
```

**One torch. Ninety seconds. Twenty houses.** That is the picture Michael asked for.

**Radii after the change, with nothing set by hand:** `{1200: 1195 pieces, 600: 67 market tables}` -
the market keeps its own value, which is the point of putting this on the building.

**Calibration for the house percentage**, which was the reason this was asked for: a 40% requirement
needs 27 of 67 houses and one well-placed torch gets **23**. So a single fire nearly does it and the
player must light a second or collect outliers - it rewards choosing where to torch rather than
counting doors.

**A correction to my own earlier analysis, recorded because it nearly killed the feature.** I first
measured objective CENTRES and reported a median gap of 2,345uu, concluding there were "no house
blocks on this island" and that spread would read as houses igniting at random. That was wrong -
centres are not footprints. The buildings' walls come within 86uu of each other. **Measuring the wrong
two points produced a confident recommendation to not build the thing that turned out to work.**

**NOT ESTABLISHED: Michael has not watched a village burn.** Every number above is counted from
`is_complete()`, not seen. What nobody has judged is whether it LOOKS like fire spreading or like
houses popping - the timing, whether the front moves believably, whether 134 m in 90 seconds is too
fast to read.

## Refine

**Deliberately not done:** no building-to-building spread code. It was the obvious implementation and
would have been entirely wasted - the pieces already spread to each other across house boundaries, and
`EGSBuildingIgnitionSource::Spread` (an enum value with no producer) is a leftover from an earlier
design, not a gap to fill.

**Found and not fixed:** two village houses are each split into TWO objectives, one per storey -
`GS_Building_39`/`52` (914uu apart, 456uu vertical) and `GS_Building_17`/`53` (1217uu, 645uu vertical).
This is the residue of the multi-storey split `InteriorNameFilters` was written to fix; it caught most
of them. Michael saw it in play - *"some of the village houses count as multiple pieces"* - and it is
the reason the house objective must be a **percentage** rather than a flat count: with a house that
counts twice, "burn 8 houses" is a lie the player can see.
