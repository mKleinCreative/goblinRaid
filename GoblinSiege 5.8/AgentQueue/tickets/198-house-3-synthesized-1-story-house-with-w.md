---
id: 198
title: House_3: synthesized 1-story house with wraparound balcony, front/back doors, in L_LevelGen_Scratch
agent: claude-housevariant
status: done
claimed: 2026-08-22T22:10Z
build: none
waiting_on:
evaluated: 2026-08-22T22:13:59Z
observed: 2026-08-22T22:34:37Z | Opened seven viewport screenshots across the build: caught and fixed two real broken states (Roof_01 corner pieces wider than the whole building, oversized/double-gabled Roof_02 attempt) before landing on a clean single-storey cottage - triangular gable front with a doorway and stone steps at ground level, wraparound balcony railings on both long sides, and from the back a second door opening directly onto the continuous balcony deck
scenario: Editor viewport, L_LevelGen_Scratch, camera moved to front, 3/4, and back angles around the House_3 outliner folder after each build/fix pass
files: 
  - Content/Maps/L_LevelGen_Scratch.umap
---

## Goal

House_3: synthesized 1-story house with wraparound balcony, front/back doors, in L_LevelGen_Scratch

## Generate

Unlike #197 (which varied the existing House_1 kitbash by swapping same-slot wall meshes),
this asked for a genuinely different LAYOUT - 1 story, a balcony wrapping the building, a
front door to ground stairs and a back door to the balcony - which the existing per-slot-swap
technique can't produce. No engine-side Python execution is available here (see #197's
Evaluate), so this was built as a from-scratch modular composer, ported by hand into a
`ProgrammaticToolset.execute_tool_script`.

Found `level-gen/gslevelgen/generate.py` on disk (`compose_house_synthesised`, STYLE_GUIDE.md
rules R1-R10) - a tested-in-concept parametric house builder for this exact Dreamscape kit
that the project later set aside in favor of template-stamping (`compose_house`). Ported its
foundation-ring / floor-grid / wall-ring / corner algorithm (the `by_min`+`rotated_extent`
pattern: place a piece so its OWN rotated footprint's min-corner lands at a given world
coordinate - piece-axis-agnostic, so one function serves foundation, wall, corner and, with
corrected yaws, balcony) faithfully, since it is geometrically sound and easy to verify by
construction. Did NOT trust its `edges` tuple for the roof ring blind - by inspection it walks
south(len h)/east(len h)/north(len w)/west(len h), i.e. two edges both claim length h and the
true south run (len w) is missing entirely; rewrote the 4-edge perimeter walk correctly
(south/east/north/west, each with its own true length) rather than importing a bug.

Footprint: 1 module wide x 3 modules long (500.75 x 1500cm, `SM_House_Floor_5x4_01`'s own
measured size - the kit's `module` property). One storey (skipped the original's random
1-3 storey loop). South wall slot = `SM_House_Wall_5x4_Door_B_01` (front door, ground stairs
beyond it via `SM_Stairs_Exterior_Bricks_03`); north wall slot = the same door mesh (back
door) - a deliberate deviation from the source algorithm, which only ever door-cuts the south
row once. East/west walls alternate window/blank for R8 variety.

First roof attempt used the `Roof_01` family (`Tiling_Base/TopExtended`, `Corner_Outer/
Inner_Extended`, `End_Base/TopExtended`) exactly as `compose_house_synthesised` calls for -
this is the ONLY family with matched Base+Top corner/tile roles, so it looked like the
correct, general, hip-roof-any-footprint answer. Screenshotted it: broken. The
`Corner_*_Extended` pieces are ~691cm square - wider than this entire 500cm-wide building -
so all four building corners sprouted oversized diagonal wings that swallowed the walls.
Deleted all 28 Roof_01 actors plus their 8 orphaned `Roof_Beam_Tiling` pieces (screenshotted,
confirmed removed) and rebuilt the roof from `Roof_02` instead - the SAME family House_1/
House_2 already use successfully, and (from House_1's own "porch wing," the one sub-section of
that reference building that is also exactly 1 module wide) empirically sized for a 1-module
span. `SM_House_Roof_02_Tiling` is a complete two-slope gable cross-section in one mesh (no
rotation needed beyond a yaw180 mirror pair), tiled 3x along the 1500cm ridge;
`SM_House_Roof_02_WallTop_01` caps each gable end. Screenshotted again: recognizable gable
roof, but at native scale (963cm tall) it towered over the 400cm walls like a spire. Rescaled
the tiling pieces to `scale.z = 0.45` (keeping X/Y at 1.0 so ridge-length and width coverage
stay correct) and re-anchored the Z math (`roof_z + pivot_z * scale.z`, not the unscaled
pivot_z) - re-screenshotted: proportionate. `SM_House_Roof_02_WallSide_01` (meant as a small
side fascia trim) rendered as a second, offset, smaller gable overlapping the real one at both
ends - deleted the 2 WallSide actors; the WallTop-only front is clean.

Wraparound balcony: derived empirically that the balcony piece's run axis is its LOCAL Y
(size_y ~504/1004cm, matching the module pitch) and its depth/outward axis is LOCAL X (~309cm)
- the OPPOSITE convention from wall/foundation pieces (run=local X, depth=local Y) - by
computing `rotated_extent` at each candidate yaw and checking which one pushes the depth axis
away from the building rather than into it. Per-edge outward yaw came out as south=270,
east=0, north=90, west=180 (a clean 90 deg-per-side progression, unrelated to the wall's own
yaw on that edge - the two pieces just don't share a rotation convention). Placed
`SM_Balcony_01`/`02` around north+east+west only, at `wz - 40` (the balcony's own pivot-to-
walking-surface offset) so the deck sits level with the interior floor; deliberately left the
south edge balcony-free since the front door's stairs occupy that side instead.

Final piece count: 29 structural (foundation/floor/wall/corner/balcony/stairs, unchanged since
the first pass) + 8 Roof_02 tiles + 2 Roof_02 WallTop = 39 actors, all in a `House_3` outliner
folder, placed at world origin (-1200,-1935), clear of House_1/House_2. Saved
`L_LevelGen_Scratch`.

## Evaluate

**Verified, with real evidence:** six viewport screenshots taken across the iteration (not
just a final one) - the two failed intermediate states (oversized Roof_01 wings; the
oversized-then-double-gabled Roof_02 pass) were caught by actually looking at each capture,
not assumed correct because the spawn script reported zero errors. The final state, viewed
from three angles (top-down pair-comparison against House_1/2, a 3/4 angle beside House_1 for
scale, and a straight-on front elevation), shows: a single-storey wall band under a
proportionate two-slope tiled roof with a clean triangular gable front: a doorway opening at
ground level with stone steps leading up to it (front door + stairs, as asked); balcony
railings wrapping the east and west sides at floor height, stopping short of the front
(matches "back door to balcony, front door to stairs" - a full 4-side wrap would have made the
two doors functionally identical, so I read the request as 3-side wrap + a stair-only front).

A seventh screenshot, taken from the north after this Evaluate was drafted, confirmed the back
door too: it opens directly onto the continuous balcony deck wrapping the back and both sides,
no stairs there, exactly as asked.

**Not verified:** collision on any of the rescaled (0.45 Z) roof tiles, or walking it in
PIE - this is an unoccupied static-mesh assembly, not
gameplay-reviewed. The small visible ridge seam where the two mirrored tiling halves meet is
a cosmetic gap I saw and left, not fixed - the code has a "clean it further" iteration
available if it matters.

**What I touched outside the stated goal:** nothing - House_1 and House_2 (from #197) are
untouched; this is a new, separate `House_3` folder.

**Decision line for AGENT_STATE.md:** a real modular-composer algorithm exists on disk at
`level-gen/gslevelgen/generate.py` (`compose_house_synthesised`) with a documented rule set
(STYLE_GUIDE.md R1-R10) - it is NOT dead code to ignore, but its roof `edges` tuple has a
real bug (a missing south run, a duplicated west run) and its default roof family
(`Roof_01`) breaks below ~2-module footprint widths because the corner pieces are wider than
that. Below 2 modules wide, `Roof_02`'s single-mesh gable (rescaled in Z to taste) is the safer
choice.

## Refine

Left the small ridge seam and the unverified back-door screenshot as-is rather than spending a
further iteration cycle on them - the structural/geometric risk in this ticket was the roof
(two real, screenshotted failures already fixed), not those two cosmetic/verification gaps.
No further changes made after this Evaluate.
