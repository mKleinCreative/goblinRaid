---
id: 206
title: Supplement to #205 - stack tower stories directly, trim on the wall-to-wall junction
agent: claude-housetower
status: done
claimed: 2026-08-26T03:18Z
build: none
waiting_on:
evaluated: 2026-08-26T03:19:55Z
observed: 2026-08-26T03:19:57Z | get_actor_bounds on both re-stacked junctions (wall_0/wall_1 and wall_1/wall_2) shows exact Z match (735.9699 and 1135.9699 respectively, both sides) - walls now touch directly with the floor pulled back behind them. Two screenshots after the move: a close 3/4 angle on the lower stories shows a continuous shaft with no visible step at any seam; a full-elevation pull-back shows the roof and gallery (which moved down with story3) still sitting correctly on top with no gap introduced there either.
scenario: Editor viewport in L_LevelGen_Scratch_2 after the batch Z-shift script ran and saved; bounds queried programmatically, then two CaptureViewport screenshots opened and inspected directly
files: 
  - Content/Maps/L_LevelGen_Scratch_2.umap
---

## Goal

Supplement to #205 - stack tower stories directly, trim on the wall-to-wall junction

## Generate

User asked to move stories 2/3/4 (my 0-indexed stories 1/2/3) down so each story's trim ring
lands on the top of the wall directly below it, instead of sitting one floor-thickness above it
(the arrangement #203 originally built: wall -> floor sits ON TOP of that wall -> next wall sits
ON TOP of the floor -> trim at that floor-top/wall-bottom line). That original arrangement is why
#205's trim had to bridge a seam that included the floor slab's own footprint at all - if walls
stack directly on each other instead, the floor becomes a purely interior element (embedded
behind the wall line, invisible from outside) and the trim marks a plain wall-to-wall joint.

Computed the required shift per story as a cumulative one: each story previously carried one
extra "floor gap" (35.97, the floor slab's total thickness) of height that doesn't exist once
walls stack directly. Story1 (0-indexed) needed to drop 35.97; story2 needed to drop 71.94 (two
floor-gaps' worth, since it inherits story1's own drop plus its own); story3 needed 107.91 (three
floor-gaps). Read every actor in `House_3_Tower`, matched each label's trailing `_N` story index
(or, for the roof/walltop/balcony pieces which have no index and sit above everything, applied
story3's shift), and moved each one straight down by its story's shift amount via
`set_actor_transform` - X/Y/rotation/scale untouched, only Z changed. 46 of the folder's 58 actors
moved (floor/wall/window/trim for stories 1-3, plus roof x4 and balcony x3); the 12
foundation/floor-0/wall-0/window-0 actors for the ground story correctly stayed put since nothing
above them needed a floor-gap removed there.

Verified the math with `get_actor_bounds` before screenshotting anything: `tower_wall_s_0` now
tops out at z=735.9699 and `tower_wall_s_1` now starts at z=735.9699 - exact match, walls
literally touching with the floor pulled back behind them. Same at the wall_1/wall_2 junction
(1135.97 both sides). `tower_trim_s_1` now spans roughly the wall-to-wall line symmetrically
(705.7 to 753.97, centered near 735.97) rather than sitting above it.

## Evaluate

**Verified, with real evidence:** the bounds check above is exact-value confirmation, not
approximate - both wall faces at both re-stacked junctions land on the identical Z to five decimal
places (differences are the same ~0.005 floating-point noise present everywhere else in this
build, not a new gap). Two screenshots after the move (a close 3/4 angle on the lower stories, and
a full-elevation pull-back including the roof and gallery) show a visually tighter, continuous
shaft with no steps at any seam, and confirm the roof/walltop/balcony - which all moved down by
story3's shift since they sit above it - still sit correctly on top with no new gap introduced
between story3's wall-top and the roof/gallery assembly.

**Not verified:** did not re-screenshot every individual trim ring at point-blank range the way
#205 did for its two test seams - relied on the exact-bounds match plus the two wider shots, which
is a lower bar of visual scrutiny than #205 applied. If a corner notch reappears at extreme
close-up on stories 2 or 3 specifically, it hasn't been ruled out by this ticket, only made
implausible by the bounds match being byte-for-byte the same kind of touch #205 confirmed visually
closes cleanly.

**What I touched outside the stated goal:** nothing else - no mesh swap, no new actors, no folder
change. Purely a Z-translation of existing actors.

**Decision line for AGENT_STATE.md:** for this kit's multi-story stacking, walls should sit
directly on top of each other (zero gap) with the floor slab pulled back to embed behind/within
the wall line rather than sitting on top of it as a visible step - the floor's own thickness
(35.97, kit.json `SM_House_Floor_5x4_01` size_z) is exactly the per-story shift needed to convert
from the old "floor adds height" layout to this one. The `Overang` trim ring from #205 belongs
centered on the wall-to-wall Z line, not on the old floor-top line.

## Refine

No changes after the two verification screenshots - the exact-bounds match at both re-stacked
junctions was strong enough evidence, and both wider shots showed no unintended shift artifact) to
justify closing without a third round of point-blank close-ups. If the user spots a corner issue
on stories 2 or 3 specifically, that is the next thing to check, not something already ruled out
here.
