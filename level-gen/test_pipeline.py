#!/usr/bin/env python3
"""
Fixture tests for the GER loop. No pytest, no editor, no API key:

    python test_pipeline.py

The important one is test_planted_cover_failure. Assignment #6 asks whether the pipeline
caught something you would have missed — a claim only worth as much as the demonstration
behind it. So the fixture plants the exact failure from the Pre-Build Declaration (a granary
in open sight of the treeline), proves the evaluator fails it, and proves the refiner clears it.
"""

from __future__ import annotations

import json
import math
import sys

sys.path.insert(0, str(__import__("pathlib").Path(__file__).resolve().parent))
for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass

from gslevelgen.evaluate import evaluate
from gslevelgen.generate import Objective, Plan, generate_settlement
from gslevelgen.geom import Rect
from gslevelgen.kit import synthetic_kit
from gslevelgen.pipeline import run_seed
from gslevelgen.refine import refine

PASS, FAIL = "  PASS", "  FAIL"
failures: list[str] = []


def check(name: str, ok: bool, detail: str = "") -> None:
    print(f"{PASS if ok else FAIL}  {name}{('  — ' + detail) if detail else ''}")
    if not ok:
        failures.append(name)


def bare_plan() -> Plan:
    """A hamlet with one objective and NOTHING to hide it. The planted failure."""
    p = Plan(seed=999, synthetic_kit=True, site=Rect(-12000, -12000, 24000, 24000),
             center=(0.0, 0.0), treeline_radius=9000.0, runic_site=(9500.0, 0.0))
    p.objectives.append(Objective(id="obj_0_granary", kind="granary",
                                  rect=Rect(-500, -400, 1000, 800)))
    p.objectives.append(Objective(id="obj_1_field", kind="field",
                                  rect=Rect(3000, 3000, 3000, 3000)))
    p.objectives.append(Objective(id="obj_2_windmill", kind="windmill",
                                  rect=Rect(-4600, 1200, 1000, 1000)))
    p.roads.append((9000.0, 0.0, 0.0, 0.0))
    p.signposts.append((3000.0, 0.0))
    return p


def test_planted_cover_failure() -> None:
    print("\ntest_planted_cover_failure — the declaration's failure, staged deliberately")
    p = bare_plan()
    ev = evaluate(p)
    cover = [f for f in ev["findings"] if f["check"] == "cover_guarantee"]
    check("evaluator FAILS a granary standing in the open", len(cover) >= 1,
          f"{len(cover)} arc finding(s)")
    check("the failure is attributed to the right objective",
          any(f["fix"]["objective"] == "obj_0_granary" for f in cover))
    check("the finding cites GDD 2.8", all(f["gdd"] == "2.8" for f in cover))

    # Now let the refiner close it, the same way the pipeline would.
    for pass_no in (1, 2, 3):
        fails = [f for f in evaluate(p)["findings"] if f["severity"] == "fail"]
        if not fails:
            break
        p, _ = refine(p, fails, pass_no)
    final = evaluate(p)
    check("refiner closes every sightline within 3 passes", final["fail_count"] == 0,
          f"{final['fail_count']} fail remaining, {len(p.cover)} cover volumes placed")


def test_evaluator_is_not_vacuous() -> None:
    """A check that never fails is not a check. Prove it can also PASS."""
    print("\ntest_evaluator_is_not_vacuous")
    p = bare_plan()
    for pass_no in (1, 2, 3):
        fails = [f for f in evaluate(p)["findings"] if f["severity"] == "fail"]
        if not fails:
            break
        p, _ = refine(p, fails, pass_no)
    check("a corrected layout passes", evaluate(p)["passed"])

    # ...and that removing the cover breaks it again, so PASS means something.
    p.cover.clear()
    check("stripping the cover fails it again", not evaluate(p)["passed"])


def test_objective_mix_rule() -> None:
    print("\ntest_objective_mix_rule — GDD 2.8: three, never more than two of a kind")
    p = bare_plan()
    p.objectives.append(Objective(id="obj_3", kind="granary", rect=Rect(6000, 0, 800, 800)))
    ev = evaluate(p)
    check("four objectives is a failure",
          any(f["fix"].get("kind") == "objective_count" for f in ev["findings"]))
    p.objectives = [Objective(id=f"o{i}", kind="granary", rect=Rect(2000 * i, 0, 800, 800))
                    for i in range(3)]
    ev = evaluate(p)
    check("three of a kind is a failure",
          any(f["fix"].get("kind") == "objective_dupes" for f in ev["findings"]))


def test_circuit_breaker_fires() -> None:
    print("\ntest_circuit_breaker_fires")
    kit = synthetic_kit()
    escalated = [run_seed(kit, s, quiet=True) for s in (2, 5, 8)]
    got = [r for r in escalated if r["outcome"] == "escalated"]
    check("at least one seed escalates rather than shipping", len(got) >= 1,
          f"{len(got)}/3 escalated")
    check("every escalation carries a problem statement",
          all(r.get("problem_statement") for r in got))
    check("escalations never spend more than 3 passes",
          all(r["passes_used"] <= 3 for r in escalated))


def test_determinism() -> None:
    print("\ntest_determinism")
    kit = synthetic_kit()
    a = json.dumps(generate_settlement(kit, 42).to_dict(), sort_keys=True)
    b = json.dumps(generate_settlement(kit, 42).to_dict(), sort_keys=True)
    c = json.dumps(generate_settlement(kit, 43).to_dict(), sort_keys=True)
    check("same seed produces a byte-identical plan", a == b)
    check("a different seed produces a different plan", a != c)
    r1 = json.dumps(run_seed(kit, 7, quiet=True)["plan"], sort_keys=True)
    r2 = json.dumps(run_seed(kit, 7, quiet=True)["plan"], sort_keys=True)
    check("the whole loop is deterministic, refinements included", r1 == r2)


def test_house_composer() -> None:
    print("\ntest_house_composer — Layer A")
    from gslevelgen.generate import compose_house
    import random
    kit = synthetic_kit()
    h = compose_house(kit, random.Random(3), 0, 0, 2, 2, "h")
    # Foundations ring the perimeter; FLOORS are the per-module count. Measuring the kit
    # corrected this: SM_House_Foundation_5x4 is 500 x 50 x 300, a wall segment, not a plate.
    floors = len([p for p in h.placements if "Floor" in p.mesh])
    founds = len([p for p in h.placements if "Foundation" in p.mesh])
    roofs = len([p for p in h.placements if "Roof" in p.mesh])
    check("every module gets a floor plate", floors == 4, f"{floors}")
    check("the foundation rings the perimeter, not the cells", founds == 2 * 2 + 2 * 2,
          f"{founds} segments for a 2x2")
    check("the roof covers every module", roofs >= floors, f"{roofs} roof vs {floors} modules")
    check("the house has a door",
          any(p.mesh.startswith("SM_Door") for p in h.placements))
    check("the footprint matches the measured module grid",
          math.isclose(h.rect.w, kit.module[0] * 2) and math.isclose(h.rect.h, kit.module[1] * 2))
    check("integrity check passes a well-formed house",
          not [f for f in evaluate(_plan_with(h))["findings"]
               if f["check"] == "building_integrity"])


def _plan_with(building) -> Plan:
    p = bare_plan()
    p.buildings.append(building)
    return p


def main() -> None:
    print("=" * 68)
    print("GER level-generation fixtures — no editor, no API key")
    print("=" * 68)
    for fn in (test_planted_cover_failure, test_evaluator_is_not_vacuous,
               test_objective_mix_rule, test_circuit_breaker_fires,
               test_determinism, test_house_composer):
        fn()
    print("\n" + "=" * 68)
    if failures:
        print(f"{len(failures)} FAILED: " + ", ".join(failures))
        sys.exit(1)
    print("all fixtures passed")


if __name__ == "__main__":
    main()
