---
id: 066
title: Learn building composition from hand-authored examples (Showcase + tutorial island kitbashes), write a style guide, rebuild the composer to follow it
agent: claude-styleguide
status: done
claimed: 2026-08-07T08:20Z
build: none
waiting_on: settlement packing for template-sized buildings: 1/8 seeds pass, handed back rather than tuned a fourth time
evaluated: 2026-08-07T08:36:01Z
files: 
  - ../level-gen/gslevelgen/generate.py
  - ../level-gen/STYLE_GUIDE.md
  - ../level-gen/extract_buildings.py
  - ../level-gen/reference/
---

## Goal

Learn building composition from hand-authored examples (Showcase + tutorial island kitbashes), write a style guide, rebuild the composer to follow it

## Generate

Michael's correction: stop building from nothing, read how the existing buildings are made.

- `extract_buildings.py` (new) — opens a reference map, groups kit meshes into buildings by
  attachment hierarchy (falling back to link-distance clustering at one measured module), and
  dumps every piece's position RELATIVE to its building origin.
- `reference/TutorialIsland.json` — 44 buildings, incl. `Innbase` (451 pieces),
  `House_2x1_T9` (220), `WaterMill_Closed` (175), `House_1x3_10` (159),
  `House_2x1_L7_Detailed` (102).
- `STYLE_GUIDE.md` (new) — ten rules R1-R10 derived from those buildings.
- `generate.py` — `compose_house` now STAMPS a template; the synthesised version is kept as
  `compose_house_synthesised` rather than deleted.
- `kit.py` — added roles the classifier was dropping into "other": `roof_beam`,
  `roof_wallmid`, `wall_blank`, `wall_door`, `wall_half`.
- `evaluate.py` — `check_roof_coverage` samples interior floor plates only.
- `test_pipeline.py` — composer fixtures rewritten for template stamping.

## Evaluate

**The Dreamscape Showcase map is a catalogue, not a village.** All 141 kit instances are
distinct meshes, each appearing exactly once, off-grid, across 29 Z levels. It displays one
of everything. Checked and rejected — recorded in `extract_buildings.py` so nobody repeats it.

**Three synthesis attempts failed, and the third failed worst.** (1) one roof tile per floor
cell -> a 192 cm hole per house; (2) two slopes at a ridge -> closed but read as a shed;
(3) a perimeter ring of Base+Top pairs following my own STYLE_GUIDE R3/R5 -> a mass of
overlapping geometry radiating outward, which the capture showed as an exploded starfish.

That is the circuit breaker, and the lesson is specific: **the style guide's rules are
statistical** — piece ratios, Base+Top pairing, storey heights, yaw discipline. Every one is
true. None of them says where a roof piece goes. The spatial grammar is only in the reference
buildings, so the composer now reads it out and stamps it.

**Verified:** 21 fixtures pass, including a faithful-stamp test (every resolvable template
piece placed, roof is the largest role, beams present, door present, rotation preserves the
count) and the count-vs-geometry contrast. Placed on `L_LevelGen_Scratch`: **1090 meshes**,
and the capture shows multi-storey timber-framed houses with dormers, jetties and pitched
roofs — see `out/templates_placed.png`.

**Two of my own checks were wrong about human-authored buildings, and the buildings were
right:** `check_roof_coverage` counted `SM_House_Floor_5x4_Overang` (a jetty, meant to
oversail) and `_Overang_Beam` (a bracket) as floor needing cover, reporting 45-57% uncovered
on correct houses. Now interior plates only, tolerance 8% (a correct reference measures ~5%).

**NOT RESOLVED — the settlement pass rate is 1/8.** Escalations are `no_overlap` (17 across 8
seeds), `roads_clear` (4), `roof_coverage` (13). Templates are far larger than the module
footprints the packing was written for, and the refiner's nudges do not converge at this
density. This is handed back rather than tuned a fourth time.

**Owed to AGENT_STATE.md:** DECISION — generated houses are stamped from hand-authored
templates in `reference/TutorialIsland.json`, not synthesised. Re-deriving the assembly was
attempted three times and failed each time.

## Refine

Changed in response to my own evaluation: the coverage floor set (my check was wrong, not the
buildings), the packing probe that measures a template before reserving space for it, and the
fixture break that shrinks geometry without changing piece counts — deleting pieces tripped
the count check too and destroyed the contrast the fixture exists to show.

Deliberately left undone:
- **`House_1x3_10` is excluded from the template set.** It is L-shaped and
  `check_roof_coverage` cannot tell its unroofed wing from a hole (~55% on a building a human
  built correctly). Shipping only what the evaluator can verify beats loosening the evaluator
  to admit a building it cannot check. Re-add it with a footprint-polygon test.
- **Settlement packing for template-sized buildings.** The open problem above.
- **`Innbase` and `WaterMill_Closed` were not dissected** — a tavern and a mill are different
  building types, and the style guide says so rather than pretending to cover them.
