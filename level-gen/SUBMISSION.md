# Assignment #6 — Submission

**Game:** *Goblin Siege* — a third-person raid game where you are the goblin. Capstone project.
**Student:** Michael Klein · **Pipeline:** `level-gen/` · **Run date:** 2026-08-18

Full engineering write-up, with every catch and its evidence, is in **`README.md`**. This page
is the short version the brief asks for.

---

## Pre-Build Declaration

Written 2026-08-06, before any pipeline code. Preserved unedited in
`PRE_BUILD_DECLARATION.md`, with a dated addendum covering a GDD change that landed after it.

**1. What content type does my game currently generate manually, inconsistently, or not at all?**

Hamlet layouts, and the buildings inside them. The tutorial hamlet was hand-placed once
(GDD §2.8) and the procedural generator was descoped. Every additional settlement is manual
work that does not exist.

**2. What specific rule from my GDD must every piece of that content satisfy?**

§2.8's **cover-guarantee rule**: broken sightlines between the treeline and every objective.
Secondary, same section: exactly three required objectives, never more than two of a kind.

*[Reproduced as written. Verifying the citation for this submission found the section number
wrong: the cover guarantee is stated at **§2.4** and restated at **§3.1**, and "never more
than two of a kind" is **§1**. §2.8 governs the hamlet but carries neither sentence. The rule
is unchanged and the code now cites the sections that actually contain it.]*

**3. What does a failure look like — concretely, in my game's terms?**

An objective standing in unbroken line-of-sight from the treeline. The player is confirmed
before the first spark, "First Spark Unseen" (+40, §2.9) becomes unwinnable, and §2.4's
"quiet half of the raid" is dead on that map — while the level still loads and looks fine.
Week 1 shipped the physical version twice: a road through a house, a windmill on an
unbuildable rock face.

*(The declaration named the example objective the **granary**. The granary was removed from
the GDD on 2026-08-14 and the **statue** took its place and its central position. See below —
that change is the most interesting thing in this submission.)*

---

## What the pipeline generates

Hamlet layouts for *Goblin Siege*, and the buildings in them, assembled from the project's
real **Dreamscape modular kit** — 187 meshes measured in-editor into `kit.json`. A plan is a
document of building footprints, piece-level placements, cover volumes, roads, and the §3.3
marker set (guard posts, patrol loops, civilian anchors, signposts), all in centimetres.

```bash
python -m gslevelgen.pipeline --seed 7            # one seed through the loop
python -m gslevelgen.pipeline --seed 1 --seeds 8  # the review gate
python test_pipeline.py                           # 9 groups / 33 assertions, offline
```

| GER role | File | What it does |
|---|---|---|
| **Generator** | `generate.py` | Composes houses from the kit and lays out a hamlet. Seeded and deterministic. **Deliberately naive on cover** — it scatters copses without checking a single sightline, so the evaluator has something real to catch. |
| **Evaluator** | `evaluate.py` | Nine deterministic checks, plus one reported SKIPPED rather than silently passed. Each carries the GDD section it answers to — or `build` where it is buildability with no GDD rule behind it. |
| **Refiner** | `refine.py` | Targeted corrections driven by each finding's `fix` payload. Never a re-roll. |
| **Circuit breaker** | `pipeline.py` | Three passes, then stop. The loop always ends on an *evaluation*, never a refinement, and escalates with a written problem statement instead of shipping. |

## The rule the evaluator enforces

**GDD §2.4, the cover guarantee: broken sightlines between the treeline and every objective.**

This is not a rule invented to have something to check. The GDD states it twice, and both
times says explicitly that a *generated* layout has to enforce it in code:

> **§2.4 (Stealth)** — "the tutorial hamlet guarantees broken sightlines between the treeline
> and every objective. If the hamlet is hand-placed that guarantee is a review pass; **if the
> generator authors it, the cover-placement rule has to be enforced algorithmically from day
> one**."

> **§3.1 (the Settlement Generator Agent)** — "Its cover-placement rule (broken sightlines
> guaranteed between treeline and every objective) is what makes the stealth layer fair, and
> **it is the rule a generated layout has to enforce algorithmically rather than by review**."

`check_cover_guarantee` casts 48 rays per objective from the treeline ring and groups the
exposed samples into **contiguous arcs**, stitching across the ring seam, reporting one
finding per arc with its angular width. The refiner fills each arc with a belt of copses
sized from that width.

Six further checks answer to a stated GDD rule: the objective roster (§2.8 for
Market/Statue/Windmill, §1 for "never more than two of a kind"), the statue's destruction
method (§2.8), reachability (§2.8's week-2 timebox — "no objective sited somewhere
unreachable"), roads clear of houses and buildable ground (§2.8's two named week-1 placement
bugs), and wayfinding (§2.8). `buildable_ground` needs the terrain heightfield and is reported
**SKIPPED** — a check that cannot run must not look like a check that passed.

**Three checks are not GDD rules and say so.** `no_overlap`, `building_integrity` and
`roof_coverage` carry `gdd: "build"` instead of a section number: the GDD states no
building-intersection rule, no roof rule and no door rule. They are worth running and they are
not evidence that this pipeline enforces the design document. An earlier version of the
evaluator stamped all ten checks `2.8` and asserted in its module docstring that none of the
rules were invented here. Three of them were, and diluting the real rule with generic ones
made the real rule harder to believe.

## Results

| | |
|---|---|
| Review gate, 8 seeds, measured kit | **8 passed**, each within 3 refine passes |
| Findings at generation | **10–25 per seed** |
| Fixtures | 9 groups, 33 assertions, all passing, offline |
| Runtime | ~1s for the whole gate |

**The circuit breaker is not dead code just because no live seed reaches it.** The fixture
drives it with layouts that cannot be packed, and 3 of 3 escalate with written problem
statements inside the 3-pass cap. A breaker that never fires and cannot be shown to fire would
prove nothing.

---

## Did the pipeline catch something I would have missed?

Yes, repeatedly — five times in `README.md`, all of them my errors rather than the
generator's. The two that matter most:

**The refiner could not close what the evaluator reported.** Cover failures were first
reported as a single ray with one point to fix. The refiner placed one copse per objective
per pass and the numbers crawled — 8 fails → 4 → 2 → circuit breaker, on every seed. The
instinct was to enlarge the copse. That would have been tuning a number to paper over a
**modelling** error: an objective open from 13 of 48 directions is not one hole, it is an
*arc*, and no single disc closes an arc. Modelling it as arcs took seed 7 from escalating to
passing in 3 passes and the gate from 1/6 to 5/8.

**And the one the pipeline missed, which is the more useful finding.** On 2026-08-14 the
granary was removed from the GDD entirely and the required trio became Market / Statue /
Windmill. `check_objective_mix` went on enforcing `granary | field | windmill` for three days
and **every seed passed it**. Nothing flickered, because the generator emitted exactly what
the evaluator expected and both were wrong about the same document. The pipeline was
confidently certifying layouts against a roster the game had abandoned — including one
objective with no mesh, no Blueprint and no placed instance, and missing the two the raid
now actually assigns.

The lesson generalises past this project: **an evaluator's authority is a citation, and
nothing was checking that the citation still said what the check believed it said.** A GER
loop whose rules come from a living document needs a check on the document, not just on the
output. This one still does not have one, and that is the honest gap — a more interesting one
than any of the geometry.

Fixing the roster also earned a new rule the old one had made uncheckable. §2.8: *"The statue
is the one target that doesn't burn: it has to be brought down, stone on stone."* That is not
flavour — it decides which runtime class the objective lands on, `AGSDestructibleObjective`
rather than `AGSBurnObjectiveBase`. `check_statue_is_toppled` now fails a statue marked to
burn.
