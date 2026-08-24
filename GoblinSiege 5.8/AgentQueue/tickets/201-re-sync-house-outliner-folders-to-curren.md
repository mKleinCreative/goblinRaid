---
id: 201
title: Re-sync house outliner folders to current actor positions after user edits
agent: claude-housevariant
status: done
claimed: 2026-08-23T02:46Z
build: none
waiting_on:
evaluated: 2026-08-23T02:48:11Z
observed: 2026-08-23T02:48:38Z | Re-ran the folder census after reassignment: House_1/2/3/4 counts are 172/181/37/50 summing to 440, matching what the clustering pass itself reported producing, and the 6 House_2-to-House_3 plus 2 House_2-to-House_4 plus 2 House_3-to-House_4 moves match actors whose logged x,y position sits inside the destination folders spatial cluster, not the source
scenario: Editor MCP queries against the live L_LevelGen_Scratch level, before and after the k-means folder reassignment
files: 
  - Content/Maps/L_LevelGen_Scratch.umap
---

## Goal

Re-sync house outliner folders to current actor positions after user edits

## Generate

Between #200 and now the user made hand edits in the live editor (extra doors, a re-skinned
window pass, an extra bay on House_3, House_3 relocated) that included duplicating pieces -
duplicates keep their source's folder tag by default, so a handful of new actors ended up
physically sitting in one house's footprint while still tagged with another house's folder
(confirmed in the prior turn: House_2's folder spanned x -4878..888, overlapping House_3 and
House_4 entirely). User asked to re-sync.

Rather than hand-listing which of ~440 actors moved where (not tractable to eyeball), ran a
4-cluster k-means over every actor currently in House_1-4, seeded from each folder's CURRENT
median position (median, not mean, so the existing outliers couldn't drag their own seed
toward themselves) and iterated 4 times to convergence. Reassigned `FolderPath` only for
actors whose nearest-cluster label differs from their current folder - 10 of 440.

Moves: 6 stray House_2 duplicates (a window, two pillars, two brick-stair pieces) relocated
into House_3; 2 stray House_2 duplicates relocated into House_4; and 2 House_3 pieces
(`wall_win_20`, `floor_13` - the user's new 4th bay, extended east) reassigned to House_4,
because that bay now sits spatially inside House_4's footprint (closer to House_4's center
than to House_3's own core) rather than House_3's original 3-bay footprint. Saved the level.

## Evaluate

**Verified:** re-ran the same census after reassignment - final per-folder counts (House_1:172,
House_2:181, House_3:37, House_4:50, summing to 440, the same total as before) match what the
clustering pass itself reported it produced, and the 10 reassigned actors' new folder was
confirmed via `set_actor_folder`'s own actor argument (not inferred).

**Not verified / judgment call surfaced to the user rather than decided silently:** the
`wall_win_20`/`floor_13` reassignment from House_3 to House_4 is defensible by distance alone
but is genuinely ambiguous - the user built that bay as an extension of House_3, and pure
spatial clustering has no way to know that "structurally belongs to" and "closest to" can
disagree. Said so in my reply rather than treating the cluster result as unquestionably
correct.

**What I touched outside the stated goal:** nothing but `FolderPath` on the 10 reassigned
actors - no transform, mesh, or unmoved actor was touched.

## Refine

Left the House_3/House_4 boundary call as the clustering produced it and flagged it explicitly
to the user, rather than overriding the algorithm with my own guess about design intent -
folder membership is cheap to fix again if they disagree, and I have less information than
they do about whether that bay is meant to read as part of House_3 or House_4.
