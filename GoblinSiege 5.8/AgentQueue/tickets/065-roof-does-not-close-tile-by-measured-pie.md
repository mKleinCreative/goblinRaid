---
id: 065
title: Roof does not close: tile by measured piece width, and give the evaluator a geometric coverage check that can see it
agent: claude-roofgen
status: active
claimed: 2026-08-07T08:10Z
build: none
waiting_on:
evaluated:
files: 
  - ../level-gen/gslevelgen/generate.py
  - ../level-gen/gslevelgen/evaluate.py
  - ../level-gen/test_pipeline.py
  - ../level-gen/README.md
---

## Goal

Roof does not close: tile by measured piece width, and give the evaluator a geometric coverage check that can see it

## Generate

Handed over from #063: the roof did not close, and the check that should have caught it
counted pieces instead of measuring.

**Ground truth first.** Dumped all 30 `Roof_01` pieces from `kit.json`. The pivots encode the
assembly: `Tiling_Base` occupies X -383..-75 relative to its pivot, `Tiling_Top` occupies
X -109..+2 and Z +319..+518. Placed at the SAME point they form one slope plus its ridge cap
reaching 383 cm from the ridge; mirrored at yaw 180 that is a 766 cm gable.

- `generate.py` - roof rebuilt as a gable: `Base`+`Top` at the ridge line per section along
  Y, both slopes, `End_Base` caps. Added `put_at()` (place by pivot, which is how the roof
  set is authored), `rotated_extent()`/`world_bb()`, and `Placement.bb` - each piece's world
  XY bounds carried on the plan so the evaluator can measure instead of count.
- **Houses are now 1 module wide**, 1-3 long. The measured set spans 766 cm, which roofs a
  500 cm module with proper eaves and cannot reach across 1000. A constraint the kit imposed,
  discovered by measuring, not a preference. A house too wide emits a
  `__ROOF_TOO_NARROW__` sentinel rather than shipping a hole.
- `evaluate.py` - new `check_roof_coverage`: rasterises the footprint 12x12 and asks whether
  any roof piece's world bounds sit above each sample. Fails over 2% uncovered.
- `test_pipeline.py` - a fixture that breaks a roof the old way and asserts BOTH that piece
  counting still passes it and that coverage fails it.
- `apply_in_editor.py` - skips `__` sentinels.
- `make_scratch_map.py` - made idempotent (see Evaluate).

## Evaluate

**Verified by running and by looking:**
- 21 fixtures pass. The new one is the point: on a deliberately broken roof,
  `building_integrity` (counting) **passes** and `check_roof_coverage` (measuring)
  **fails at 62% uncovered**. That is the regression stated as a test.
- Review gate 6/8 (`passed [2,3,4,5,7,8]`, `escalated [1,6]`). Both escalations are genuine
  packing conflicts - two long houses, and the 1709x5354 windmill against a stall - not roof
  faults.
- Re-placed seed 2 on `L_LevelGen_Scratch`: 244 meshes + 21 markers, nothing dropped, and the
  capture shows closed pitched gables with eaves where the previous pass had scattered tiles.

**Two further bugs found while doing this, both mine:**
1. `_move_building` translated `rect` and each placement's x/y but **not** the cached `bb`.
   Every refiner-moved house kept its roof bounds at the old location and the new check
   correctly reported "100% of the footprint has no roof above it". Pass rate 1/8 until
   fixed, then 6/8. **The new check found it on its first run** - a count-based check could
   not have.
2. `make_scratch_map.py` spawned the ground and a full lighting rig every run, so re-opening
   the map stacked a second sun. The editor said so: "Multiple directional lights are
   competing to be the single one used for forward shading." Now checks by label first, and
   5 duplicate actors were cleaned out of the scratch map.

**Workflow note:** `gsqueue done -Id 063` **refused** - #063 kept working after its Evaluate
was written, exactly what ticket #024 added that guard for, and its Evaluate did claim things
("kit not measured", "nothing placed") that were no longer true. Re-read, superseded in place,
closed with `-Reaffirm`. Worth recording: a ticket that claims `AgentQueue/QUEUE.md` can never
close normally, because rendering the board writes that file and trips the staleness check.

**Not verified:** nobody has walked it on foot yet. Everything above is a top-down and an
oblique capture, which is what missed the road-through-house in week 1.

**Owed to AGENT_STATE.md:** DECISION - generated houses are one module wide because the
measured standard roof set spans 766 cm. Wider houses need the `_Extended` roof pieces, which
are not implemented.

## Refine

Changed in response to my own evaluation: the `bb` translation in `_move_building` (found by
the very check this ticket added) and the idempotence guard in `make_scratch_map.py` (found by
an editor warning I would have scrolled past).

Deliberately left undone:
- **The `_Extended` roof set is unimplemented**, so houses cannot be wider than one module.
  Recorded in the code as a sentinel and a hard evaluator failure rather than a silent
  narrowing, so the next person meets it as an error and not as a mystery.
- **Coverage is measured against AABBs, not meshes.** A pitched roof's bounding box is
  generous, so this catches a missing strip and would not catch a small pierced hole. Named
  in the code comment; a mesh-accurate test needs the editor and this must run headless.
- **The roof does not vary.** Every house gets the same Base/Top pair; the kit has Half,
  Extended, WindowFrame and dormer-window variants that would break up the silhouette. Purely
  cosmetic and outside this ticket.
