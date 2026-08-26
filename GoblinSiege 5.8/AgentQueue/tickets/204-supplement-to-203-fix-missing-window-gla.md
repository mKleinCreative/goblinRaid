---
id: 204
title: Supplement to #203 - fix missing window glazing on House_3_Tower
agent: claude-housetower
status: done
claimed: 2026-08-26T02:57Z
build: none
waiting_on:
evaluated: 2026-08-26T02:58:34Z
observed: 2026-08-26T02:58:36Z | Two close-up CaptureViewport screenshots at the identical camera position/angle, before and after: before shows a bare black window cutout with no glazing next to a plain wall band; after shows a proper teal-lattice glazed window matching House_1/House_2/House_4 own windows. get_actor_bounds queried on every story transition confirmed zero Z gap (floor bottom exactly equals wall top below, sub-centimeter) before concluding the gap was a glazing/visual issue rather than a geometry issue. A wider full-elevation shot after the fix shows all 4 stories consistently glazed.
scenario: Editor viewport in L_LevelGen_Scratch_2, same camera transform captured twice (before/after the window-insert script ran) plus a pulled-back full-elevation shot, all opened and inspected directly
files: 
  - Content/Maps/L_LevelGen_Scratch_2.umap
---

## Goal

Supplement to #203 - fix missing window glazing on House_3_Tower

## Generate

User reported two problems with #203's tower: "missing windows" and "a gap between each floor."

Investigated with engine-truth rather than re-deriving by hand: pulled `get_actor_bounds` (real
computed AABBs, not actor pivots) for every actor in `House_3_Tower`. Z bounds confirmed each
floor's bottom face lands exactly on the wall-top below it at every story transition (sub-
centimeter, e.g. story0 wall top 735.9699 vs floor_1 bottom 735.9700) - there is no actual void
anywhere in the stack. So "gap" wasn't a hole; a close-up screenshot at one seam showed why it
reads that way: the wall openings I placed (`SM_House_Wall_5x4_Window_B_01`) are just structural
CUTOUTS - the actual glazed window (`SM_House_Window_B_Alternate`, the teal-lattice mesh visible
on House_1/House_2/House_4) is a separate decorative insert that #203 never placed. Every window
was rendering as a bare black hole, which (combined with the plain, undecorated seam band each
floor slab exposes) is what read as "gaps" - a wall of black holes and plain bands, not a
recognizable multi-story facade.

Fix: added 15 `SM_House_Window_B_Alternate` inserts (scale 0.95, matching every other placement
of this mesh project-wide) - one per Window_B_01 opening (north/west/east x4 stories, south x3
stories since south-story0 is the door). Positioned using the REAL measured center of each wall's
own `get_actor_bounds` result (not hand-derived local-mesh math this time - the wall bounds were
already on hand from the investigation, and centering on real engine output removes an entire
class of the pivot-offset mistakes #203 fought), at a Z offset of wall_z+150.5 (the vertical
offset observed on the kit's own real usage of this exact mesh elsewhere in the project). Also
looked for a purpose-built floor-jetty trim (kit.json has `SM_House_Floor_5x4_Overang` /
`_Overang_Beam`) to dress the seam bands themselves, but after the window fix the seams read as
ordinary Tudor-style datum beams (consistent with the timber-frame style already used everywhere
else in this kit), not gaps - so did not add the untested trim piece on top of a problem that
turned out not to need it.

## Evaluate

**Verified, with real evidence:** `get_actor_bounds` on every tower actor before touching
anything, confirming zero Z gap at all three story transitions (not assumed from the placement
formula - measured from the engine's own computed bounds). Two close-up screenshots (same camera
angle, before/after) show the exact same seam: before, a bare black window hole next to a plain
wall band; after, a proper teal-lattice glazed window matching House_1/2/4's own windows. A wider
full-elevation screenshot after the fix shows all four stories with consistent glazing and the
seam bands reading as intentional trim, not damage.

**Not verified:** whether the user's "gap" perception is fully resolved - it was diagnosed from
screenshots, not from the user confirming the fix looks right to them. If a real gap surfaces
under some other camera angle or in PIE, it isn't the Z-stacking (measured, confirmed clean).

**What I touched outside the stated goal:** nothing else - the 15 new window actors are additive,
filed into the same `House_3_Tower` folder; no existing #203 actor was moved, resized, or reassigned.

**Decision line for AGENT_STATE.md:** `SM_House_Wall_5x4_Window_*` pieces are bare cutouts with no
glazing - always pair one with a `SM_House_Window_B_Alternate` (or sibling) insert at scale 0.95,
`wall_z + 150.5`, centered on the wall's own `get_actor_bounds` center, or it renders as an empty
black hole. This is easy to miss because the wall mesh alone looks structurally complete in the
editor's asset thumbnail.

## Refine

Left the `Overang` jetty-trim experiment undone - tried reasoning through its placement, but
after the window fix the seams no longer looked like a defect worth spending an unverified new
piece on. If the user still sees a seam they don't like after this fix, that trim piece is the
next thing to try, not a first resort.
