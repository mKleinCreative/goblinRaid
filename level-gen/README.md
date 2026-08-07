# Gameplay-Constrained Level Generation — a GER pipeline

**Goblin Siege** · Assignment #6 · Michael Klein

Generates hamlet layouts and the buildings in them, from the Dreamscape modular kit, and
refuses to emit one that breaks the rules the GDD already committed to.

```bash
python -m gslevelgen.pipeline --seed 7            # one seed through the loop
python -m gslevelgen.pipeline --seed 1 --seeds 8  # the review gate
python test_pipeline.py                           # 18 fixtures, no editor, no API key
```

The whole loop runs **with the editor closed**. Placing a plan is a separate, human-gated
step (`apply_in_editor.py`), because `CLAUDE.md` constraint 2 makes the editor a scarce
resource and a pipeline that needs it to think is a pipeline that runs once a day.

---

## Pre-Build Declaration

Written before any pipeline code — full text in `PRE_BUILD_DECLARATION.md`.

1. **Content generated manually / not at all:** hamlet layouts and their buildings. The
   tutorial hamlet was hand-placed once (GDD §2.8) and the procedural generator was descoped.
2. **The GDD rule every piece must satisfy:** §2.8's **cover-guarantee rule** — broken
   sightlines between the treeline and every burn objective. Plus exactly three objectives,
   never more than two of a kind.
3. **What failure looks like:** a granary in unbroken line of sight from the treeline. The
   player is confirmed before the first spark, "First Spark Unseen" (+40, §2.9) becomes
   unwinnable, and §2.4's "quiet half of the raid" is dead on that map — while the level
   still loads and looks fine.

---

## The four roles

| Role | Where | What it does |
|---|---|---|
| **Generator** | `generate.py` | Layer A composes a house from the modular kit (foundation → floor → walls → corners → roof). Layer B lays out a hamlet: core village, farmstead, windmill, roads, cover, and the §3.3 marker set. Seeded and deterministic. |
| **Evaluator** | `evaluate.py` | Seven deterministic geometric checks, each citing its GDD section, plus one honestly reported as SKIPPED. |
| **Refiner** | `refine.py` | Targeted corrections driven by each finding's `fix` payload. Never a re-roll. |
| **Circuit breaker** | `pipeline.py` | Three passes, then stop and emit a problem statement. Falls through to the GDD's own golden-seed fallback. |

**The Generator–Evaluator contract.** The Generator emits a plan of footprints, cover volumes,
roads and markers in centimetres. The Evaluator enforces that every burn objective is
occluded from the treeline, reachable on foot, non-overlapping, road-clear and signposted.
Both work in the same coordinate space against the same GDD sections, so neither can drift
onto rules the other has not seen.

### The Generator is deliberately imperfect

Cover placement is a naive scatter along the approaches — it does not check a single
sightline. That is not a shortcut; it is the point. The class framing is that generation is
"fast output, not perfection" and "broken output is expected". If the generator were
correct-by-construction there would be nothing for the evaluator to catch, and no evidence
that the evaluator works.

---

## What the evaluator enforces

Deterministic, because *if code can verify it, use code*:

| Check | GDD | Catches |
|---|---|---|
| **cover_guarantee** | §2.8 | An objective visible from the treeline. Reported as contiguous **arcs**, not rays — see below. |
| objective_mix | §2.8 | Not exactly three objectives, or three of a kind |
| reachability | §2.1/2.2 | An objective you cannot walk to from the runic site — an unwinnable raid |
| no_overlap | §2.8 | Buildings intersecting; names which of the pair is movable |
| roads_clear | §2.8 | A road running through a house — week 1's literal bug, promoted to a rule |
| wayfinding | §2.8 | An objective with no road or signpost near it |
| building_integrity | §2.8 | A house whose roof does not close, or that has no door |
| buildable_ground | §2.8 | **Reported SKIPPED.** Needs the terrain heightfield, which only the editor has. A check that cannot run must not look like a check that passed. |

---

## Did the pipeline catch something I would have missed?

**Yes — twice, and both were mine, not the generator's.**

### 1. The refiner could not close what the evaluator reported

The first evaluator reported cover failures as a **single ray**: *"obj_2_windmill is in
unbroken line of sight from 13/48 treeline samples"*, with one point to fix. The refiner
dutifully placed one copse per objective per pass and the numbers crawled: 8 fails → 4 → 2 →
**circuit breaker**. Every seed escalated.

The instinct was to enlarge the copse radius. That would have been tuning a number to paper
over a modelling error: an objective open from 13 of 48 directions is not one hole, it is an
**arc**, and no single disc closes an arc. The evaluator now groups exposed samples into
contiguous arcs (stitching across the ring seam) and reports one finding per arc with its
angular width; the refiner fills that arc with a belt of copses sized from the width. Seed 7
went from escalating to passing in 3 passes, and the review gate from 1/6 to **5/8 seeds
passing**.

This is the same lesson `tools/hamlet/gs_buildings.py` records from the building-clustering
work — *"distance was never the right question"* — arriving from the opposite direction. There
it was a threshold standing in for identity; here it was a ray standing in for an arc.

### 2. Two evaluator bugs the review gate surfaced

Running eight seeds instead of one exposed:

- **`windmill_2 overlaps obj_2_windmill`** — a false positive. The windmill building *is*
  that objective; they share a rect. Buildings now carry an `objective_id` and self-overlap
  is skipped.
- **`house_N overlaps obj_0_granary`** never cleared, no matter the pass. The refiner moved
  the pair's second member, and `_move_building` only searches `buildings` — so when that
  member was an *objective* the fix was a silent no-op. It printed `moved obj_0_granary by
  (0,0)` for three passes. The evaluator now decides which of the pair is movable (objectives
  are anchors — the granary sits where the guards are thickest, §2.8) and the refiner pushes
  the two directly apart along the line between them.

A silent no-op that reports success is the exact failure mode the GER pattern is supposed to
prevent, and it survived in my refiner until a wider seed sweep made it obvious.

---

## Evidence

`python test_pipeline.py` — 18 fixtures, no editor, no API key, all passing:

- **The planted failure**: a granary standing in the open fails `cover_guarantee` with 3 arc
  findings citing §2.8, attributed to the right objective, and the refiner closes every
  sightline within 3 passes.
- **The evaluator is not vacuous**: a corrected layout passes, and stripping its cover fails
  it again — so PASS means something.
- **The circuit breaker fires**: seeds 2, 5 and 8 escalate with problem statements rather
  than shipping, and never spend more than 3 passes.
- **Determinism**: same seed → byte-identical plan, refinements included.
- **Layer A**: every module gets a foundation, the roof covers every module, the house has a
  door, the footprint matches the measured module grid.

Review gate over 8 seeds: **passed [1, 3, 4, 6, 7], escalated [2, 5, 8]**. The escalations
are genuine — buildings competing for the same ground, and one case where clearing an overlap
pushed a stall onto a road. Fix-one-break-another is precisely what a bounded loop should
hand back rather than thrash on.

---

## Ground truth before thresholds

No dimension in this pipeline is a number someone typed.

`kit_manifest.py` runs once in the editor and measures the real bounds and pivot of every kit
mesh into `kit.json`. `kit.py` derives the module grid from the measured footprint of
`SM_House_Foundation_5x4` — the piece is *called* 5x4; what that is in centimetres is whatever
the mesh says. If `kit.json` is absent the generator **refuses to run** rather than guessing.

A `--synthetic-kit` exists for the fixtures, with invented round numbers. Plans built from it
are stamped `synthetic: true`, and `apply_in_editor.py` refuses to place them — for exactly
the reason the flag exists.

### Current state, honestly

**The kit has not been measured yet.** The editor is running but its MCP server is not
started, so `kit_manifest.py` cannot reach it. Everything above ran on the synthetic kit,
which validates the *logic* of the loop and none of its *dimensions*. To measure for real,
in the editor console:

```
ModelContextProtocol.StartServer
```

then:

```bash
python gs_ue.py level-gen\kit_manifest.py --timeout 300   # writes kit.json
python -m gslevelgen.pipeline --seed 1 --seeds 8          # real dimensions
python gs_ue.py level-gen\apply_in_editor.py --timeout 600
```

`apply_in_editor.py` refuses three things on purpose: a synthetic plan, a plan that did not
pass evaluation, and `L_Tutorial_Island` — generated actors do not go on the hand-authored
hamlet (§2.8). It is idempotent: every actor it places is tagged and a re-run destroys the
previous set first, the same derived-data rule `gs_buildings.py` established.

---

## What this does not do

- **It does not re-scope the slice.** GDD §2.8 descoped the procedural generator as
  post-slice and funding-gated. This targets a scratch map; the tutorial hamlet stays
  hand-authored.
- **No runtime PCG.** Generation is author-time, so §4.1's baked-navmesh guarantee holds.
- **The verifier agent is not wired in.** The class describes a two-layer evaluator —
  deterministic checks plus an agent for judgement calls ("does this read as a hamlet?").
  Only the deterministic layer exists here. That is the honest gap: every rule enforced is
  one a raycast can settle, and none of them can tell you the result is *good*.
- **`buildable_ground` is unimplemented**, not silently passing — it needs terrain the solver
  cannot see.

## Files

```
PRE_BUILD_DECLARATION.md   the three answers, written first
kit_manifest.py            ONE-TIME, in-editor: measure the kit -> kit.json
gslevelgen/geom.py         2D primitives: rects, discs, segment tests, flood fill
gslevelgen/kit.py          the measured kit; refuses to guess
gslevelgen/generate.py     GENERATOR — Layer A houses, Layer B settlements
gslevelgen/evaluate.py     EVALUATOR — 7 deterministic checks + 1 declared SKIPPED
gslevelgen/refine.py       REFINER — targeted fixes, escalating over 3 passes
gslevelgen/pipeline.py     the loop, the circuit breaker, the golden-seed fallback
apply_in_editor.py         the only step that needs the editor
test_pipeline.py           18 fixtures
out/                       plan.json, evaluation.json
```
