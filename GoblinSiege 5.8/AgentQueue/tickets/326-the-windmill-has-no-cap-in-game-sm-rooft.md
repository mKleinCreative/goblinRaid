---
id: 326
title: The windmill has no cap in game: SM_RoofTIles2 renders in the editor and not at runtime
agent: unassigned
status: abandoned
claimed: 2026-08-26T23:16Z
build: none
waiting_on:
evaluated:
observed:
scenario:
files: []
---

## Goal

The windmill has no cap in game: SM_RoofTIles2 renders in the editor and not at runtime

## Report (not started - found during #324, nobody has picked it up)

**The ground windmill has no roof in game.** `SM_RoofTIles2` - the conical cap with the spire - draws
in the editor viewport and does not draw in PIE. Michael, 2026-08-26: *"the rooftiles is still
invisible in game"*, having circled the cap in a screenshot.

**What is MEASURED, and it is worth reading before theorising:**

- The actor exists in the level, is not hidden, and is exactly where it should be:
  `SM_RoofTIles2`, mesh `SM_RoofTIles`, horizontal offset **0** from the tower, spanning
  **z 5389..7929** (the tower `SM_WIndmill_Base2` spans 604..5596, so the cap sits on top of it).
- `hidden_in_game` reads **False** on its StaticMeshComponent.
- **THE MESH AND MATERIALS ARE FINE.** This is the strongest clue and it was an accident: #324
  fractures the roof into `GC_RoofTIles` and releases it when the mill collapses, and Michael's
  report was *"the RoofTiles ended up appearing during the break apart"*. The same geometry, with
  the same materials, renders correctly as a geometry collection in the same PIE session. So
  whatever suppresses it belongs to THAT ACTOR, not to the asset.

**Not investigated.** Nothing below is a diagnosis - these are the places a first look should go,
in the order that costs least:

- Per-instance visibility on the placed actor: `bHiddenInGame` is False, but `bVisible`, `bIsEditorOnly`
  and the actor's own `bHidden` were not checked.
- **`bIsEditorOnly` is the prime suspect** given the exact symptom "draws in editor, absent at
  runtime", and it is one checkbox.
- World Partition / HLOD / cull distance - the cap is high up and small in silhouette, so a
  MaxDrawDistance or a level-of-detail rule could plausibly drop it. Note #313 attempted a World
  Partition conversion on this map and it did NOT complete, so the map is not partitioned.
- Whether the OTHER mill and the other three `SM_WIndmill_Base` instances have roofs at all - if
  none of them render, this is one setting on a shared prefab rather than one broken actor.

**Why it matters more than it looks.** Every screenshot of the ground windmill is missing its cap,
and the windmill is the GDD's loudest objective. It also confused #324 for several rounds: a piece
hanging in mid-air after the collapse was assumed to be a physics fault, and four measurements went
into chasing unmoved indices inside the geometry collections before Michael's screenshot showed the
real object was a third actor nobody had touched.

**Related:** #324 (the mill sink, which now fractures and drops this actor), #323 (the mill's window
trigger), #313 (the World Partition attempt on this map, closed as blocked).

## Generate

<!-- not started -->

## Evaluate

<!-- not started -->

## Refine

<!-- not started -->

> 2026-08-26T23:17Z Bug report, unowned. Mesh proven healthy - it renders as a geometry collection in the same session. Suspect bIsEditorOnly on the placed actor.

---

## CLOSED 2026-08-27 by Michael

Michael: *"close 326."*

**The defect is not fixed.** `SM_RoofTIles2` still renders in the editor and not at runtime; nothing
in this session investigated or changed it. Closed as a scope decision, not as work completed - the
windmill collapse (#324) was observed and signed off with Michael's *"the collapse looked wonderful
and the RoofTiles ended up appearing during the break apart"*, so the cap's absence before the
collapse is cosmetic and did not block the beat it was raised against.

Re-open it if the mill cap's absence starts mattering - most likely if a mill is ever seen intact
from close range in a shipping shot.
