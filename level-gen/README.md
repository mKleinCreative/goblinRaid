# Gameplay-Constrained Level Generation — a GER pipeline

**Goblin Siege** · Assignment #6 · Michael Klein

Generates hamlet layouts and the buildings in them, from the Dreamscape modular kit, and
refuses to emit one that breaks the rules the GDD already committed to.

```bash
python -m gslevelgen.pipeline --seed 7            # one seed through the loop
python -m gslevelgen.pipeline --seed 1 --seeds 8  # the review gate
python test_pipeline.py                           # 9 groups / 33 assertions, offline
```

The whole loop runs **with the editor closed**. Placing a plan is a separate, human-gated
step (`apply_in_editor.py`), because `CLAUDE.md` constraint 2 makes the editor a scarce
resource and a pipeline that needs it to think is a pipeline that runs once a day.

---

## Pre-Build Declaration

Written before any pipeline code — full text in `PRE_BUILD_DECLARATION.md`.

1. **Content generated manually / not at all:** hamlet layouts and their buildings. The
   tutorial hamlet was hand-placed once (GDD §2.8) and the procedural generator was descoped.
2. **The GDD rule every piece must satisfy:** the **cover-guarantee rule** — broken
   sightlines between the treeline and every objective. Plus exactly three required
   objectives, never more than two of a kind. *(The declaration cites §2.8 for both. Checking
   that citation found it wrong: the cover guarantee is **§2.4**, restated at **§3.1**, and
   the two-of-a-kind clause is **§1**. The rules are unchanged; the section numbers in the
   code are now the ones that carry them.)*
3. **What failure looks like:** an objective in unbroken line of sight from the treeline. The
   player is confirmed before the first spark, "First Spark Unseen" (+40, §2.9) becomes
   unwinnable, and §2.4's "quiet half of the raid" is dead on that map — while the level
   still loads and looks fine.

> The declaration as written on 2026-08-06 named that objective the **granary**. The granary
> was removed from the GDD on 2026-08-14 and the **statue** took its place and its position.
> The original text is preserved unedited in `PRE_BUILD_DECLARATION.md` with a dated
> addendum; catch #3 below is the story of the pipeline not noticing.

---

## The four roles

| Role | Where | What it does |
|---|---|---|
| **Generator** | `generate.py` | Layer A composes a house from the modular kit (foundation → floor → walls → corners → roof). Layer B lays out a hamlet: core village, farmstead, windmill, roads, cover, and the §3.3 marker set. Seeded and deterministic. |
| **Evaluator** | `evaluate.py` | Nine deterministic checks plus one reported SKIPPED. Each carries the GDD section it answers to — or `build`, where there is no GDD rule behind it and the check is buildability only. |
| **Refiner** | `refine.py` | Targeted corrections driven by each finding's `fix` payload. Never a re-roll. |
| **Circuit breaker** | `pipeline.py` | Three passes, then stop and emit a problem statement. Falls through to a golden-seed pick — the GDD retired that as a *cut-order* item (§4.5) but §2.8's week-2 timebox makes the seed-review fallback half live again (§157). |

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

A check's `gdd` field is either **a section number** — a rule the GDD actually states, quoted
in that check's docstring — or **`build`**, meaning buildability with no GDD rule behind it.
The distinction is recorded per finding rather than asserted, because an evaluator that
stamps a section number on a generic check is claiming an authority it does not have.

| Check | GDD | Catches |
|---|---|---|
| **cover_guarantee** | **§2.4** | An objective visible from the treeline. Reported as contiguous **arcs**, not rays — see below. Restated at §3.1. |
| objective_mix | §2.8 / §1 | Not exactly three *required* objectives, three of a kind, or an optional type smuggled into the required trio. Roster is §2.8; "never more than two of a kind" is §1. |
| statue_not_burned | §2.8 | A statue marked to burn. "The one target that doesn't burn: it has to be brought down, stone on stone" — and it lands on a different runtime class for exactly that reason |
| reachability | §2.8 | An objective you cannot walk to from the runic site — an unwinnable raid. §2.8's week-2 timebox: "no objective sited somewhere unreachable". |
| roads_clear | §2.8 | A road running through a house — one of the two placement bugs §2.8 names from week 1 |
| wayfinding | §2.8 | An objective with no road or signpost near it |
| buildable_ground | §2.8 | The other week-1 bug, "a windmill on an unbuildable rock face". **Reported SKIPPED** — needs the terrain heightfield, which only the editor has. A check that cannot run must not look like a check that passed. |
| no_overlap | `build` | Buildings intersecting; names which of the pair is movable. **No GDD rule behind it.** |
| building_integrity | `build` | A house whose roof does not close, or that has no door. **No GDD rule behind it** — the GDD states no roof or door rule. |
| roof_coverage | `build` | Measures actual roof coverage over the footprint, which the piece *count* in `building_integrity` cannot see. **No GDD rule behind it.** |

---

## Did the pipeline catch something I would have missed?

**Yes — four times, and all four were mine, not the generator's.**

### 0. The refusal to guess caught a wrong grid the moment it could

This is the one I would never have found by reading. `kit.py` derived the module from
`SM_House_Foundation_5x4` — it is named for the grid, and "foundation" sounds like the plate
a house stands on. Measured, it is **[500, 50, 300]**: fifty centimetres deep and three
metres tall. It is a perimeter foundation *wall*. A module derived from it is 500 × 50, and
no house could ever sit on it.

The piece that actually tiles a floor is `SM_House_Floor_5x4_01`, measuring
**[500.75, 500.0, 36]**. **The module is square — 500 × 500 — and "5x4" describes neither
dimension.** My synthetic kit had guessed 500 × 400 from the name, and been wrong in both
the shape and the piece.

The composer was wrong in the same way: it laid a foundation under every module cell. The
real kit wants foundations **ringing the perimeter** and floor plates tiling the interior.

Measuring also found the prefabs are nothing like their placeholders — the windmill base is
**1709 × 1607** where I had assumed 900 × 900, and the sail sweeps **5354**. The objective
footprint is now taken from the measured prefab, because a 2-module box would have let the
overlap check happily pass a windmill whose sails scythe through a farmhouse.

None of this cost a debugging session, because the generator **refuses to run without
`kit.json`** rather than falling back on a plausible number.

### 0a. Then placing it caught what no check could

Seed 1 went onto a scratch map (`L_LevelGen_Scratch`, 130 meshes + 19 markers, nothing
dropped) and I looked at it. The houses were wrong: **walls missing from faces, roofs
hovering off-centre, foundation beams sticking out past the footprint as loose bars on the
ground.**

`compose_house` placed every piece at a grid corner as if its pivot were at the mesh's min
corner. It is not — `kit_manifest.py` measures `pivot_from_min` for precisely this reason and
the composer ignored it. Rotation compounded it: yaw spins a piece about its pivot, so a
centre-pivoted wall turned 90° lands half its length away. Fixed by rotating the local box and
offsetting by where its min corner actually ends up. (The `origin_for()` helper that first did
this went unused once `compose_house` moved to stamping hand-authored templates, and was deleted
in #115; `rotated_extent` / `world_bb` carry the same pivot-aware maths.)

**Every deterministic check passed throughout.** Roof count ≥ floor count, door present,
footprint on the module grid — all counts, and a count cannot see a wall 250 cm out of place.
That is the honest limit of this evaluator, found by doing what week 1 did: looking at it
instead of trusting the report.

### 0b. …and then the real numbers broke the layout

Swapping the synthetic kit for the measured one dropped the review gate from **5/8 seeds
passing to 2/8**, drowning in overlap findings. The spacing constants had been tuned against
invented footprints; real 500 × 500 modules and a 1709-wide windmill do not fit the ring I
had sized for them.

The fix was not a bigger radius. Spatial packing is arithmetic — the generator now places
buildings by rejection against what is already down, with anchors (objectives) placed first
because the statue stands where the guards are thickest and cannot be shoved. That returned
the gate to **8/8 at that time** (the roster change in catch #3 below then cost it, and the
repair-coupling fixes in catch #4 won it back — see Evidence), and — the part that matters — the evaluator still catches **12–18
failures per seed** on generation, now almost entirely cover-guarantee violations. The signal
is the declared gameplay rule instead of noise about floor space.

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
- **`house_N overlaps obj_0_<objective>`** never cleared, no matter the pass. The refiner
  moved the pair's second member, and `_move_building` only searches `buildings` — so when
  that member was an *objective* the fix was a silent no-op. It printed `moved obj_0_… by
  (0,0)` for three passes. The evaluator now decides which of the pair is movable (objectives
  are anchors — the statue stands where the guards are thickest, §2.8) and the refiner pushes
  the two directly apart along the line between them.

A silent no-op that reports success is the exact failure mode the GER pattern is supposed to
prevent, and it survived in my refiner until a wider seed sweep made it obvious.

### 3. The evaluator was enforcing a rule the GDD no longer contained

This is the one with the sharpest lesson, and the pipeline did **not** catch it. A human
reading the rubric did.

On **2026-08-14**, queue #156 removed the granary from the GDD. Michael's ruling: *"when
initially written I had meant a different building, but had never created it. Windmill,
Market and Statue works fine."* The required trio became Market / Statue / Windmill and the
wheat field was demoted to optional. Ten edits, and the GDD came out of it with zero granary
references.

`check_objective_mix` went on enforcing `granary | field | windmill` for three days. Every
seed passed it. Nothing in the loop so much as flickered — because the check was internally
consistent, the generator emitted exactly what the evaluator expected, and **both of them
were wrong about the same document.** The evaluator's authority is a section number in a
comment; a section number does not notice when the section changes underneath it.

Worse than a stale rule, it was a *stale rule that still passed*: the pipeline was
confidently certifying layouts against a roster the game had abandoned. One of the three
objectives it insisted on could never be built at all — the granary had no mesh, no Blueprint
and no placed instance, and `AGSDestructibleObjective` (the granary-shaped class) never
derived from `AGSBurnObjectiveBase`, so the raid director could not have counted it. Every
layout the gate certified was a third unbuildable, and it was also missing the market and the
statue, which are two thirds of what the raid now actually assigns. #156's own ticket closed
with "the DOWNSTREAM consequences are unverified." This was one of them.

Two things came out of fixing it:

- **The statue check exists because of it.** §2.8 says the statue "doesn't burn: it has to be
  brought down, stone on stone", and that is not cosmetic — it decides which runtime class
  the objective lands on. `check_statue_is_toppled` now fails a statue marked to burn. That
  rule only became checkable when the granary left.
- **Swapping the roster broke the packing, and the fix was arithmetic again.** Three required
  objectives plus one or two optional fields is more anchored area than the old trio, and the
  gate fell to 5/8 with houses shoved into objectives. Two causes, both the generator
  creating work for the refiner rather than the refiner failing: `free_spot` rejected against
  buildings but never against **objectives**, and the house ring was a typed constant
  (`mw * 3.0`) that overlapped the village square, walling the statue in until reachability
  correctly called it an unwinnable raid. The ring is now measured off where the core
  anchors actually landed. Same lesson as the kit: derive it, don't type it.
- **And the refiner was making things worse.** Fail counts oscillated 1 → 2 → 1 across the
  three passes: `_move_building` shoved a house clear of one overlap and straight into
  another, because nothing checked the destination. A move that has to be undone next pass
  was never a fix. `_move_building_clear` now checks where it is about to land, and the gate
  went 5/8 → **7/8** (and catch #4 took it the rest of the way).

**The honest conclusion is a limitation, not a win.** Every rule in this evaluator cites a
GDD section, and *nothing verifies that the citation still says what the check thinks it
says.* A pipeline whose authority is a living document needs a check on the document, not
just on the output — and this one does not have one. That is the gap I would close next, and
it is a more interesting gap than any of the geometry.

### 4. Three repairs that each broke something else

Getting the gate from 7/8 to 8/8 was not a tuning exercise. It was three separate instances of
one bug: **a repair that satisfies the check it was aimed at while breaking a check that was
already passing.** All three were found by instrumenting a failing seed and printing what the
refiner actually received and did, not by reading the code — reading it, I had been about to
enlarge the copse radius, which would have fixed nothing.

- **The refiner walked a house along the road it was escaping.** `_move_building_clear` rotates
  the push bearing up to ±1.6 rad to dodge a neighbour, and nothing checked the *road*. Seed 3
  moved `house_0` 3794 → 3405 → 4265 → 5793 across all three passes and never cleared road 0 —
  and its final resting place opened the treeline sightline the breaker then escalated. The
  0° arc was a symptom; the road push was the disease. `_blocked` now tests every road, using
  the same inflation the evaluator does.
- **Buildings are cover, and moving one destroys it silently.** A house standing between the
  treeline and the windmill is doing the cover guarantee's job whether or not anyone placed it
  there for that reason. Seed 6 lost `obj_2_windmill` exactly that way. Candidate destinations
  are now scored with `_exposure` and rejected if they increase the number of unoccluded
  (objective, ray) pairs.
- **One collision was reported twice, so the house was moved twice.** The windmill *building*
  shares its objective's `Rect` **object**, and stalls sit inside the market's footprint, so
  `house_5 vs windmill_2` and `house_5 vs obj_2_windmill` were two findings for one overlap.
  The first move resolved it; the redundant second move carried the occluder away. Buildings
  that belong to an objective are no longer listed separately in `check_no_overlap` — the
  objective already stands for that footprint.

**The sharpest detail is *when* the last one landed.** It happened on pass 3, and the loop
always terminates on an evaluation — which is correct, and which means a failure *introduced*
by the final pass can never be repaired. So a refiner does not only have to be effective, it
has to be **conservative**: the last pass is the one where a clever fix has nowhere left to go.

---

## Evidence

`python test_pipeline.py` — **9 fixture groups, 33 assertions**, no editor, no API key, all
passing:

- **The planted failure**: a statue standing in the open fails `cover_guarantee` with arc
  findings citing §2.8, attributed to the right objective, and the refiner closes every
  sightline within 3 passes.
- **The evaluator is not vacuous**: a corrected layout passes, and stripping its cover fails
  it again — so PASS means something.
- **The objective roster**: four required objectives fails, three of a kind fails, and six
  optional fields alongside the trio is legal — optional objectives do not count toward the
  three (§2.8).
- **The statue does not burn**: a statue marked `fire` fails citing §2.8, and the refiner
  sets it back to `topple`.
- **The circuit breaker fires**: escalates with problem statements rather than shipping, and
  never spends more than 3 passes.
- **Determinism**: same seed → byte-identical plan, refinements included (verified by hash).
- **Layer A**: every module gets a foundation, the roof covers every module, the house has a
  door, the footprint matches the measured module grid.
- **Roof coverage sees what counting missed**: a correctly assembled roof reports 0 findings,
  a broken roof still passes the piece *count*, and measuring coverage fails it. This is the
  fixture that exists because counting pieces once hid a wall 250 cm out of place.
- **The stamp carries the full transform**: mirrored pieces stay mirrored (51/51), every
  non-unit scale survives (69/69), chimney segments stack flush, the foundation course sits at
  ground level.

Review gate over 8 seeds **against the measured kit**, run 2026-08-18:

| | |
|---|---|
| Passed | **8 of 8**, each within 3 refine passes |
| Findings at generation | **10–25 per seed** |
| Runtime | ~1s for the whole gate, offline |

The loop is doing real work on every seed; it is not passing them because there was nothing
wrong. **And the circuit breaker is not dead code just because no live seed reaches it** — the
fixture drives it with layouts that cannot be packed at all, and 3 of 3 escalate with written
problem statements inside the 3-pass cap. A breaker that never fires on a real seed and cannot
be shown to fire on a hard one would be worth nothing.

---

## Ground truth before thresholds

No dimension in this pipeline is a number someone typed.

`kit_manifest.py` runs once in the editor and measures the real bounds and pivot of every kit
mesh into `kit.json`. `kit.py` derives the module grid from the measured footprint of
`SM_House_Foundation_5x4` — the piece is *called* 5x4; what that is in centimetres is whatever
the mesh says. If `kit.json` is absent the generator **refuses to run** rather than guessing.

A `--synthetic-kit` exists for the fixtures, with invented round numbers. Plans built from it
are stamped `synthetic: true`, and `apply_in_editor.py` refuses to place them — for exactly
the reason the flag exists. Its guesses were wrong about the module shape, the role of the
foundation piece, and every prefab footprint, which is the best argument available for why
the real pipeline is not allowed to make any.

### Current state

**The kit is measured.** `kit_manifest.py` ran in the editor and measured **187 meshes, zero
skipped** — every piece of the modular House set and every whole-prefab Structure. All results
above are against those real dimensions; `--synthetic-kit` now exists only for the fixtures.

```bash
python gs_ue.py level-gen\kit_manifest.py --timeout 300   # done: 187 meshes -> kit.json
python -m gslevelgen.pipeline --seed 1 --seeds 8          # done: 8/8
python gs_ue.py level-gen\apply_in_editor.py --timeout 600   # not yet run
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
- **The roof still does not close.** `SM_House_Roof_01_Tiling_Base` measures 308 × 501 against
  a 500 × 500 module, so tiling one per cell leaves a ~192 cm gap along X. The integrity check
  counts roof pieces against floor pieces and passes it, which is the same count-vs-geometry
  blind spot that hid the pivot bug. Closing it means tiling by measured piece width rather
  than per module — known, not yet done, and visible in the capture.

## Files

```
SUBMISSION.md              the short ReadMe: declaration, rule, results, what it caught
PRE_BUILD_DECLARATION.md   the three answers, written first (+ a dated addendum)
kit_manifest.py            ONE-TIME, in-editor: measure the kit -> kit.json
gslevelgen/geom.py         2D primitives: rects, discs, segment tests, flood fill
gslevelgen/kit.py          the measured kit; refuses to guess
gslevelgen/generate.py     GENERATOR — Layer A houses, Layer B settlements
gslevelgen/evaluate.py     EVALUATOR — 7 deterministic checks + 1 declared SKIPPED
gslevelgen/refine.py       REFINER — targeted fixes, escalating over 3 passes
gslevelgen/pipeline.py     the loop, the circuit breaker, the golden-seed pick
apply_in_editor.py         the only step that needs the editor
test_pipeline.py           9 fixture groups, 33 assertions
out/                       plan.json, evaluation.json
```
