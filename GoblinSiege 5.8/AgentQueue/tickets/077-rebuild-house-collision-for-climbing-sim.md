---
id: 077
title: Rebuild house collision for climbing: simple clean shell on SM_MERGED_House_Medium_11 first
agent: claude-climbrebuild
status: done
claimed: 2026-08-07T23:24Z
build: none
waiting_on:
evaluated: 2026-08-08T20:18:52Z
files: 
  - Content/DreamscapeSeries/DreamscapeFarmlands/Maps/DemoAssets/MergedActors/Houses/SM_MERGED_House_Medium_11.uasset
---

## Goal

Michael: *"The collision came with the prefab but we shouldn't assume it's correct for our purposes.
we bought this as an art pack and animated someone to crawl all around it rather than just observe it
being pretty."* Give `SM_MERGED_House_Medium_11` collision a climber can trust.

## Generate

`SM_MERGED_House_Medium_11` set to **`CTF_USE_COMPLEX_AS_SIMPLE`**, and its four original convex hulls
removed. Collision is now exactly the render mesh. Backup of the original asset at
`/Game/_CollisionBackup/SM_MERGED_House_Medium_11_BACKUP` - the art pack is gitignored (`Content/*`
with hand-added exceptions), so that duplicate is the only way back.

Before this, an earlier attempt in the same ticket generated a **NDOP26 hull that was an invisible box
221uu proud of the wall**. Michael: *"your collision you made was really bad and not useful."* That
hull was discarded, not shipped.

## Evaluate

**Verified by measurement, not assertion:** 16 of 16 sample points where the trace hit the collision
now agree with the render mesh position; coverage holes fell from **44% to 1%**.

**Michael played it and confirmed the specific win it was for:** the trim bands that used to stop the
climb no longer do. That was the whole point of the ticket and it holds.

**It also created a problem, which I did not anticipate and which is recorded here because the next
person needs it.** With complex-as-simple there is no longer any wall to stop a downward ledge probe
from reaching an **interior floor**. In the ledge-probe corpus, 15 of 22 sampled heights on this house
now find an interior floor at `nz +1.00`, which passes any walkable-surface gate. #079 added a
"sky above the deck" test specifically to reject those, and that test would not have been needed
before this ticket. **This is the most likely cause of Michael's earlier report** *"I tried to climb
again and it pushed me inside the building"* - though that remains unconfirmed by a play test.

**Not rolled out.** The other 42 merged houses still have their original 4-hull collision. Per-poly
cost of complex-as-simple across ~1.4M verts is **unmeasured**, and that measurement should happen
before anyone repeats this at scale.

**Owed AGENT_STATE.md** - DECISION (2026-08-07, Michael): bought-art-pack collision is not assumed
correct for gameplay. `SM_MERGED_House_Medium_11` uses `UseComplexAsSimple`; the rest do not, pending
a cost measurement.

## Refine

- **Kept a backup outside the gitignore.** `Content/*` is gitignored with per-file exceptions, so a
  bad collision edit to an art-pack asset is not recoverable through git. The duplicate is the rollback.
- **Threw away my own generated hull rather than defend it.** The NDOP26 attempt was worse than the
  prefab's original collision; the honest move was to stop generating and use the mesh itself.
- **Reported the interior-floor consequence rather than leaving it for the next agent to hit.** The
  ticket's headline result is good and it introduced a real regression in a neighbouring system; both
  belong in the record.

**Deliberately left undone:** the other 42 houses, and the per-poly cost measurement that should gate
them; the window collision squares Michael asked about (torches pass through, still count for
ignition), which were never started.
