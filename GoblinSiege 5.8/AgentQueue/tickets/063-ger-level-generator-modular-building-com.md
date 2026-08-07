---
id: 063
title: GER level generator: modular building composer + settlement layout with GDD-derived evaluator (Assignment 6)
agent: claude-levelgen
status: review
claimed: 2026-08-07T06:10Z
build: none
waiting_on: ModelContextProtocol.StartServer in the editor console, so kit_manifest.py can measure the kit for real
evaluated: 2026-08-07T06:25:39Z
files: 
  - ../level-gen
  - AgentQueue/QUEUE.md
  - ../content-pipeline/README.md
  - ../coding-agent/gsagent.py
---

## Goal

GER level generator: modular building composer + settlement layout with GDD-derived evaluator (Assignment 6)

## Generate

Coursework (Assignment 6) plus a workflow change that applies to everyone. No C++, no
.uasset, no build. Nothing placed in a level yet.

**Workflow (Part 1):**
- `AgentQueue/QUEUE.md` - rule 3 gains a **circuit breaker**: three passes at a failing
  check, then stop, go `blocked`, and write a problem statement. Added inside rule 3 rather
  than as a seventh rule, because it is the bound on Refine and renumbering 4-6 would churn
  every reference to them.
- `content-pipeline/README.md` - a GER-mapping section. That pipeline already *was*
  Generate/Evaluate/Refine with a circuit breaker; it never used the words. Documentation
  only, no code change.
- `coding-agent/gsagent.py` - split `stage_verify` into `run_verify()` + `print_verify()`,
  and added a `refine` stage: on verify findings, feed them back for a targeted fix, max 3
  passes, then escalate with a problem statement. It had an evaluator and no refiner.

**New `level-gen/` (Part 3):** `PRE_BUILD_DECLARATION.md` (written FIRST, before any pipeline
code, as the assignment requires), `kit_manifest.py`, `gslevelgen/{geom,kit,generate,evaluate,
refine,pipeline}.py`, `apply_in_editor.py`, `test_pipeline.py`, `README.md`.

## Evaluate

**Verified by running it, editor closed throughout:**
- `test_pipeline.py` - 18 fixtures, all pass. Includes the planted failure (a granary in open
  sight of the treeline fails `cover_guarantee` with 3 arc findings citing S2.8, and the
  refiner closes every sightline in 3 passes), an anti-vacuity test (stripping the cover fails
  it again, so PASS means something), circuit-breaker firing, and byte-identical determinism.
- Review gate, 8 seeds: **passed [1,3,4,6,7], escalated [2,5,8]**. Not everything passes and
  not everything fails, which is the only distribution that tells you anything.
- `gsagent.py refine` no-ops correctly on already-clean code.
- With no `kit.json`, the pipeline **refuses to run** rather than guessing dimensions.

**Two bugs of mine that the pipeline caught, both worth recording:**
1. The evaluator reported cover failures as a single *ray*. An objective open from 13 of 48
   treeline samples is an *arc*, and no one disc closes an arc - so the refiner crawled and
   every seed escalated. The tempting fix was a bigger radius, i.e. tuning a number to paper
   over a modelling error. Correct fix: group exposed samples into contiguous arcs (stitched
   across the ring seam) and fill each arc with a belt sized from its angular width. 1/6 seeds
   passing -> 5/8. Same lesson as `gs_buildings.py`'s "distance was never the right question",
   from the other direction.
2. `house_N overlaps obj_0_granary` never cleared: the refiner moved the pair's second member,
   and `_move_building` only searches `buildings`, so moving an *objective* was a silent
   no-op that printed `moved obj_0_granary by (0,0)` three passes running. A silent no-op
   reporting success is precisely what GER exists to prevent, and it lived in my refiner until
   a wider seed sweep made it obvious.

**What is written but has never run:** `kit_manifest.py` and `apply_in_editor.py`. Both need
the editor's MCP server, which is not started (editor is up; `ModelContextProtocol.StartServer`
has not been run). **Everything measured so far used the SYNTHETIC kit** - invented round
numbers that validate the loop's logic and none of its dimensions. Plans from it are stamped
`synthetic: true` and the applier refuses them.

**Touched outside the goal:** nothing. QUEUE.md, the content-pipeline README and gsagent.py
were all in the claim.

**Owed to AGENT_STATE.md:**
- DECISION: the queue now has a circuit breaker - 3 refine passes then `blocked` with a
  problem statement (QUEUE.md rule 3).
- DECISION: generated levels target a scratch map. `apply_in_editor.py` refuses
  `L_Tutorial_Island`, a synthetic-kit plan, and any plan that did not pass evaluation.
- OPEN: the settlement-generator spec the GDD cites (S2.8, S3.3) **does not exist in this
  repo**. Its rules were recoverable from GDD prose, but S6's "five-seed review gate" has no
  written definition - `--seeds N` is my interpretation. Needs a ruling.

## Refine

Changed in response to my own evaluation: the arc-based cover model, the movable/anchor
distinction in the overlap check, the `objective_id` link that stopped the windmill being
reported as overlapping itself, and the direct-separation nudge. All four came from running
eight seeds instead of one - a single seed looked fine and hid every one of them.

Also deliberately made `buildable_ground` report **SKIPPED** rather than pass. It needs the
terrain heightfield, which only the editor has. A check that cannot run must not read as a
check that passed; that is how an evaluator quietly becomes decorative.

Deliberately left undone:
- **The verifier agent is not wired in.** The class describes a two-layer evaluator -
  deterministic checks plus an agent for judgement ("does this read as a hamlet?"). Only the
  deterministic layer exists. Every rule enforced is one a raycast can settle, and none of
  them can tell you the result is *good*. That is the honest gap and it is in the README.
- **No real measurements.** One editor console command away; not mine to run.
- **Nothing placed in a level.** By design - placement is the human-gated step.
