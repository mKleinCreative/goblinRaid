"""
The GER loop: Generate -> Evaluate -> Refine, bounded by a circuit breaker.

    python -m gslevelgen.pipeline --seed 7
    python -m gslevelgen.pipeline --seeds 5            # golden-seed review gate
    python -m gslevelgen.pipeline --seed 7 --synthetic-kit   # tests only

Runs with the editor closed, on purpose (CLAUDE.md constraint 2). The output is a plan;
placing it is `apply_in_editor.py`, a separate human-gated step.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass

from .evaluate import evaluate
from .generate import generate_settlement
from .kit import Kit, KitNotMeasured, load_kit, synthetic_kit
from .refine import refine

OUT = Path(__file__).resolve().parent.parent / "out"
MAX_PASSES = 3


def run_seed(kit: Kit, seed: int, max_passes: int = MAX_PASSES, quiet: bool = False) -> dict:
    """One seed through the loop. Always ends on an evaluation, never on a refinement."""
    def log(m):
        if not quiet:
            print(m)

    plan = generate_settlement(kit, seed)
    history = []
    ev = evaluate(plan)
    log(f"  seed {seed}: generated — {ev['fail_count']} fail, {ev['warn_count']} warn")

    for p in range(1, max_passes + 1):
        if ev["passed"]:
            break
        fails = [f for f in ev["findings"] if f["severity"] == "fail"
                 or f["fix"].get("kind") == "add_signpost"]
        plan, changes = refine(plan, fails, p)
        ev = evaluate(plan)
        history.append({"pass": p, "changes": changes,
                        "fails_after": ev["fail_count"]})
        for c in changes:
            log(f"    pass {p}: {c}")
        log(f"  seed {seed}: after pass {p} — {ev['fail_count']} fail, "
            f"{ev['warn_count']} warn")

    outcome = "passed" if ev["passed"] else "escalated"
    result = {
        "seed": seed,
        "outcome": outcome,
        "passes_used": len(history),
        "evaluation": ev,
        "history": history,
        "plan": plan.to_dict(),
    }
    if outcome == "escalated":
        remaining = [f for f in ev["findings"] if f["severity"] == "fail"]
        result["problem_statement"] = (
            f"Seed {seed}: {max_passes} refine passes did not clear {len(remaining)} "
            f"finding(s): " + "; ".join(f["message"] for f in remaining) +
            ". The layout may be unsatisfiable rather than merely unrefined — a human should "
            f"decide whether to take another seed or relax a rule."
        )
        log(f"  CIRCUIT BREAKER — {result['problem_statement']}")
    return result


def golden_seed(results: list[dict]) -> dict:
    """
    GDD 2.8/§7: if no seed clears a fair verdict, fall back to the seed with the fewest and
    least severe unfair markings, to be hand-patched exactly once.

    Implementing a decision the design document already made, not inventing a policy.
    """
    def severity_score(r):
        ev = r["evaluation"]
        return (ev["fail_count"], ev["warn_count"])

    best = min(results, key=severity_score)
    return {
        "type": "golden_seed_fallback",
        "chosen_seed": best["seed"],
        "reason": (f"No seed passed. Seed {best['seed']} carries the fewest and least severe "
                   f"markings ({best['evaluation']['fail_count']} fail, "
                   f"{best['evaluation']['warn_count']} warn). Per GDD 2.8/§7 it is to be "
                   f"hand-patched exactly once — nudge a cover prop, move a guard post — and "
                   f"that patch logged as a scoped exception to the no-hand-edits rule."),
    }


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--seeds", type=int, default=0,
                    help="run N consecutive seeds from --seed (the review gate)")
    ap.add_argument("--passes", type=int, default=MAX_PASSES)
    ap.add_argument("--synthetic-kit", action="store_true",
                    help="fake measurements; TESTS ONLY, the plan is not placeable")
    args = ap.parse_args()

    try:
        kit = synthetic_kit() if args.synthetic_kit else load_kit()
    except KitNotMeasured as exc:
        sys.exit(f"\n{exc}\n")

    if kit.synthetic:
        print("!! SYNTHETIC KIT — invented dimensions. Plans are NOT placeable.\n")
    print(f"kit: {len(kit.pieces)} pieces, module grid {kit.module[0]:.0f} x "
          f"{kit.module[1]:.0f} cm (measured)\n")

    OUT.mkdir(parents=True, exist_ok=True)
    seeds = range(args.seed, args.seed + args.seeds) if args.seeds else [args.seed]
    results = [run_seed(kit, s, args.passes) for s in seeds]

    passed = [r for r in results if r["outcome"] == "passed"]
    summary = {
        "seeds_run": len(results),
        "passed": [r["seed"] for r in passed],
        "escalated": [r["seed"] for r in results if r["outcome"] == "escalated"],
        "synthetic_kit": kit.synthetic,
    }
    if not passed:
        summary["fallback"] = golden_seed(results)

    chosen = passed[0] if passed else None
    if chosen:
        (OUT / "plan.json").write_text(
            json.dumps(chosen["plan"], indent=2), encoding="utf-8")
    (OUT / "evaluation.json").write_text(
        json.dumps({"summary": summary, "results": results}, indent=2), encoding="utf-8")

    print(f"\n{'='*68}")
    print(f"seeds run: {summary['seeds_run']}   passed: {summary['passed']}   "
          f"escalated: {summary['escalated']}")
    if chosen:
        print(f"wrote out/plan.json (seed {chosen['seed']}, "
              f"{chosen['passes_used']} refine pass(es))")
    else:
        print("NO SEED PASSED — " + summary["fallback"]["reason"])
    print("wrote out/evaluation.json")


if __name__ == "__main__":
    main()
