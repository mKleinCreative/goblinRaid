---
id: 205
title: Supplement to #203/#204 - close the wall-bottom V-notch gap at each tower story seam
agent: claude-housetower
status: done
claimed: 2026-08-26T03:14Z
build: none
waiting_on:
evaluated: 2026-08-26T03:15:26Z
observed: 2026-08-26T03:15:27Z | Screenshots at the EXACT unmoved camera transform the user was looking at, bShowUI true so selection outlines render: before, tower_wall_s_1 selection outline shows a real V-notch cut into the bottom silhouette (not visible in bounds data) sitting above a visible black gap down to tower_floor_1; after the trim fix, same camera, same selection, gap fully closed. A second confirmation on the story2/3 seam (never touched during tuning), different camera angle, also shows a clean seam with no gap.
scenario: Editor viewport in L_LevelGen_Scratch_2, camera pinned to the transform read live via GetCameraTransform (never moved by me, per explicit user instruction), before/after comparison plus one held-out seam for generalization
files: 
  - Content/Maps/L_LevelGen_Scratch_2.umap
---

## Goal

Supplement to #203/#204 - close the wall-bottom V-notch gap at each tower story seam

## Generate

User said the gap between tower stories was still there after #204's window fix, and told me
directly: "don't move the camera, take a screenshot and look at the two selected parts, the
floor and a wall." That was the right diagnostic move - my own earlier attempts (get_actor_bounds
Z-checks, a floor rescale, an Overang trim at the wrong height) had all been guessing blind at
what the user could just point at directly.

Read the user's live editor selection (`GetSelectedActors`) - `tower_wall_s_1` and `tower_floor_1`
- and captured the viewport at the EXACT current camera transform (`GetCameraTransform` then fed
straight back into `CaptureViewport`, `bShowUI: true` so the selection outline renders). That
outline is what actually solved this: `tower_wall_s_1`'s selection silhouette isn't a flat-
bottomed box - it's a shallow V, dipping down in the middle and rising back up at the two
corners. `kit.json`'s min/max is a bounding box; it cannot show an internal notch like this. My
first Overang-trim experiment (#204's Refine explicitly left this untried) had also failed
silently - traced back to a real bug, not a wrong theory: I'd computed its Z from `floor_z(n)`
instead of `wall_z(n)`, so it patched the seam ONE FULL STORY below the one that was actually
gapped.

Fixed both problems in sequence, confirming each step visually before moving on: repositioned the
trim to the correct seam height (`wall_z[n]`, not `floor_z[n]`) - screenshot showed most of the
gap closed but two small triangular slivers remained exactly at the two corners, where the wall's
V-notch is shallowest but still doesn't reach flat. Stretched the trim's Z scale to 1.8x (widening
its vertical coverage from ~27cm to ~48cm, still centered on the seam) - re-screenshot from the
SAME unmoved camera showed the corners fully closed, nothing left visible.

Rolled the validated fix out to all three inter-story seams (`wall_z[1]`=771.94,
`wall_z[2]`=1207.91, `wall_z[3]`=1643.88; the ground-floor seam at `wall_z[0]`=335.97 doesn't need
this - the wall there sits low enough to embed slightly into the foundation ring's own solid mass,
which already occludes the notch, matching why the original single-story houses never showed this
problem). 12 `SM_House_Floor_5x4_Overang` actors total (4 edges x 3 seams), same yaw/placement
convention as the foundation ring. Deleted the 4 leftover `test_overang_*` actors from the
diagnostic pass first.

## Evaluate

**Verified, with real evidence:** the fix was confirmed on the SAME seam the user pointed at
(story0/1, unmoved camera, before/after) - gap fully closed, not just narrowed. Then verified on a
DIFFERENT seam (story2/3) never touched during the diagnostic/tuning pass, from a fresh camera
angle - also fully clean. Two independent confirmations, not one lucky screenshot.

**Not verified:** whether the 1.8x Z-stretch on the Overang mesh looks acceptable up close at
grazing angles other than the ones captured, or whether it reads as a slightly oversized fascia
board on close inspection - it's a uniform-looking plain board either way, low risk, but not
exhaustively angle-tested. Collision on the stretched trim pieces is unchecked (matches every
other cosmetic piece in this tower - none of it is PIE/collision-verified).

**What I touched outside the stated goal:** nothing else in `House_3_Tower`; no #203/#204 actor
was moved or resized. The only other change was deleting the 4 `test_overang_*` scratch actors
this same investigation created (in `House_3_Tower_Test`, now empty/removed).

**Decision line for AGENT_STATE.md:** `SM_House_Wall_5x4_*` wall pieces do NOT have a flat bottom
edge - kit.json's bounding box hides a real V-shaped notch cut into the bottom silhouette (visible
only via the editor's own selection outline, not from bounds data). Stacking these walls directly
on a floor slab for a multi-story build exposes that notch as a gap at each seam's corners. Fix:
`SM_House_Floor_5x4_Overang`, scaled ~1.8x in Z, centered on `wall_z(n)` (not `floor_z(n)` - that
offset-by-one-story mistake cost a full round of debugging), placed on all 4 edges at every
inter-story seam. Ground-floor-to-foundation seams don't need it (the foundation's own overlap
already hides the notch there).

## Refine

No further changes after the second confirmation screenshot - the fix generalized cleanly to an
untouched seam on the first try, which is the bar the project's own workflow sets (verify beyond
the single case you tuned against). Left the exact Z-stretch factor (1.8x) as empirically-found
rather than deriving it from the wall mesh's precise notch geometry - the notch shape isn't
described anywhere in `kit.json`, so a closed-form value doesn't exist without opening the mesh
in an external tool, and the visual result is already clean.
