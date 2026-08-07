# Human Building Style Guide — Dreamscape modular kit

**Derived, not invented.** Every rule below was read out of hand-authored buildings on
`L_Tutorial_Island` by `extract_buildings.py`, which dumps each piece's position relative to
its building origin into `reference/TutorialIsland.json`.

Sources (attached hierarchies — `gs_buildings.py`: *"the subtree IS the building"*):

| Building | Pieces | Role |
|---|---|---|
| `Innbase` | 451 | Michael's tavern, the most elaborate |
| `House_2x1_T9` | 220 | multi-storey house |
| `WaterMill_Closed` | 175 | working structure |
| `House_1x3_10` | 159 | long house |
| `House_2x1_L7_Detailed` | 102 | the simplest complete house — the reference case |

> **Rejected source: the Dreamscape `Showcase` map.** It looks like the obvious teacher and
> is not one. All 141 kit instances are *distinct meshes, each appearing exactly once*, at
> off-grid positions across 29 Z levels. It is a catalogue rack displaying one of everything,
> not a village. Checked and discarded — recorded here so nobody spends the trip twice.

---

## R1 — A building has its own frame, and it is not world-axis-aligned

Every reference house sits at an arbitrary world yaw: `House_2x1_T9` at 15°,
`House_1x3_10` at 25°, `House_2x1_L7_Detailed` at 70°. Within that frame every piece is at
**base + {0, 90, 180, 270}** and nothing else.

**Rule:** pick one base yaw per building, then work entirely in local orthogonal space.
Never place a piece at an arbitrary angle. A village of houses all facing due north reads as
generated; the pack's own villages never do that.

## R2 — Storey height is 400 cm, and houses have more than one

Z clusters in the references land on multiples of ~400: `House_2x1_T9` at 480 / 880 / 1280 /
1860; `House_1x3_10` at 470 / 880; `House_2x1_L7_Detailed` at 40 / 440.

400 cm is exactly the measured height of `SM_House_Wall_5x4_Window_A_01`. The wall *is* the
storey.

**Rule:** storeys stack at the measured wall height. Two storeys is normal, three happens.
A one-storey box is the exception, not the default.

## R3 — Roof is the largest single category, by a wide margin

Piece counts by role in the references:

| Building | roof | floor | wall | corner | stairs | window | door |
|---|---|---|---|---|---|---|---|
| `House_2x1_T9` | **55** | 42 | 30 | 20 | 28 | 11 | 6 |
| `House_1x3_10` | **46** | 41 | 20 | 22 | 8 | 10 | 1 |
| `House_2x1_L7_Detailed` | **33** | 23 | 13 | 11 | 7 | 3 | 2 |

Roof is 25–33% of every building. The old composer spent 4–10 pieces on a roof and the
result looked like a shed, because it *was* one.

**Rule:** if the roof is not the biggest part of the piece list, the roof is wrong.

## R4 — Roof pieces are placed in Base+Top PAIRS at identical coordinates

From `House_2x1_L7_Detailed`, de-rotated into its own frame:

```
SM_House_Roof_01_End_TopExtended   (263, 60, 445)  yaw 0
SM_House_Roof_01_End_Base          (263, 60, 445)  yaw 0     <- same point
SM_House_Roof_01_Tiling_Base       (263,320, 445)  yaw 0
SM_House_Roof_01_Tiling_TopExtended(313,320, 445)  yaw 0
SM_House_Roof_01_Corner_Inner_Extended (313,820,445) yaw 90
SM_House_Roof_01_Corner_Outer_Extended (313,820,445) yaw 90  <- same point
```

The pivots encode the offsets, so a Base and its Top go **at the same position with the same
yaw**. This is why the pieces have pivots far outside their own bounds.

**Rule:** never place a `_Base` without its matching `_Top`/`_TopExtended`. Place both at one
point.

## R5 — The roof is a perimeter ring, not a field of tiles

Every roof piece in the reference sits at **one Z** (445 in `L7_Detailed`) and the placements
walk the building outline: `Tiling` along the runs, `End_Base`/`End_TopExtended` terminating
them, `Corner_Inner_Extended`/`Corner_Outer_Extended` at direction changes, `Tiling_Half_*`
for half-module steps. Yaws cycle 0 / 90 / 180 / 270 as the walk turns corners.

It is assembled exactly like the walls: a ring with corners and end caps.

**Rule:** walk the roof outline. Do not tile the interior — a roof is a boundary, and tiling
one piece per floor cell (what the old composer did) leaves holes *and* reads wrong.

## R6 — The Extended variants are the norm, not the exception

Reference roofs are dominated by `_TopExtended`, `_End_TopExtended`,
`_WallBottom_Extended_01`, `_Corner_Outer_Extended`, `_Tiling_Half_*`. The plain
`Tiling_Top`/`Corner_Outer_Standard` pieces appear rarely.

**Rule:** default to the Extended set. The "standard" naming is misleading — measured, the
standard pieces span less and the pack's own buildings mostly do not use them. (This also
retires the earlier conclusion that houses must be one module wide: that was a consequence of
using only the standard set.)

## R7 — Structural beams exist and are load-bearing to the look

`SM_House_Roof_Beam_End`, `SM_House_Roof_Beam_Tiling`, `SM_House_Roof_Beam_Half_Tiling`
appear in every reference roof (3–4 each). They read as exposed rafters under the eaves and
are a large part of why the pack's houses look built rather than extruded.

**Rule:** place a beam with each roof run segment.

## R8 — Walls are a mix, and openings are separate pieces

References use `Wall_5x4_A_*` (blank), `Wall_5x4_Window_A/B/C_*`, `Wall_5x4_Door_C/D_*`,
`Wall_2_5x4_*` (half-width), plus separate `SM_House_Window_*` and `SM_Door_*` inserts.
Windows outnumber doors 3–10 to 1.

**Rule:** vary the wall run — blanks, windows, the occasional door — rather than repeating one
variant. A wall of identical windows reads as generated.

## R9 — Floors outnumber walls

Floors 23–42 against walls 13–30. Because every storey is floored, and floors tile the
interior while walls only ring it.

**Rule:** floor every storey, including the top one under the roof.

## R10 — Corners are ~10–20% of the building

11–22 corner pieces per house — more than four, because every storey has its own corner set,
and `Corner_01`, `Corner_02`, `Corner_02_Front`, `Corner_02_SIde`, `Corner_Beam` are all in
use.

**Rule:** corner every storey, and vary the corner piece.

---

## Checklist for a generated house

1. Pick a base yaw; work in local orthogonal space (R1).
2. Choose a footprint in modules and a storey count of 1–3 (R2).
3. Per storey: floor every cell (R9), ring with mixed wall variants (R8), corner every
   corner with varied pieces (R10).
4. Roof: walk the outline at one Z, placing Base+Top **pairs** (R4), Extended variants by
   default (R6), `End_*` at run terminations, `Corner_*_Extended` at turns, `Half` for
   half-module steps (R5), and a beam per segment (R7).
5. Sanity: the roof should be the largest role in the piece list (R3).

## What this guide does not cover

- **`Innbase` (451 pieces) and `WaterMill_Closed` were not dissected.** They are a tavern and
  a working mill — different building *types* with balconies, terraces, water wheels. The
  rules above are for ordinary houses.
- **Interiors.** The references carry interior walls, stairs, fireplaces and bars. Nothing
  here places them; a generated house is a shell.
- **Exact run-length arithmetic.** R5 says the roof walks the outline; the precise sequence of
  full/half/end pieces for an arbitrary footprint was inferred from one house, not proven
  across all five.
