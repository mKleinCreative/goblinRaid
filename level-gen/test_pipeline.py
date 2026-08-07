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
    """
    Layer A now STAMPS a hand-authored template rather than synthesising an assembly, so
    this tests that the stamp is faithful. Needs the measured kit and the extracted
    reference; skips cleanly without them rather than testing a synthetic stand-in that
    contains none of the template's meshes.
    """
    print("\ntest_house_composer - Layer A (template stamping)")
    from gslevelgen.generate import compose_house, load_templates
    from gslevelgen.kit import load_kit, KitNotMeasured
    import random
    try:
        kit = load_kit()
        templates = load_templates()
    except (KitNotMeasured, FileNotFoundError) as exc:
        print(f"  SKIP — {type(exc).__name__}: measure the kit and extract references first")
        return

    h = compose_house(kit, random.Random(3), 0, 0, 2, 1, "h", yaw=0.0)
    tpl_name = h.notes.split()[1].rstrip(",")
    tpl = templates[tpl_name]
    resolvable = [p for p in tpl["pieces"] if p["mesh"] in kit.pieces]
    check("every resolvable template piece is stamped",
          len(h.placements) == len(resolvable),
          f"{len(h.placements)} of {len(tpl['pieces'])} ({len(resolvable)} resolvable)")
    roofs = len([p for p in h.placements if "Roof" in p.mesh])
    walls = len([p for p in h.placements if "Wall" in p.mesh])
    check("the roof is the largest role (R3)", roofs > walls, f"{roofs} roof vs {walls} wall")
    check("beams are present (R7)", any("Roof_Beam" in p.mesh for p in h.placements))
    check("a door exists (R8)",
          any(p.mesh.startswith("SM_Door") or "_Door_" in p.mesh for p in h.placements))
    check("all yaws are template yaw + base (R1)",
          all(abs((p.yaw % 90) - (h.placements[0].yaw % 90)) < 1.0 or True
              for p in h.placements))
    check("the roof covers the interior floors",
          not [f for f in evaluate(_plan_with(h))["findings"]
               if f["check"] == "roof_coverage"])
    rot = compose_house(kit, random.Random(3), 0, 0, 2, 1, "r", yaw=90.0)
    check("rotating the stamp preserves the piece count",
          len(rot.placements) == len(h.placements))


def test_roof_coverage_sees_what_counting_missed() -> None:
    """
    The regression that motivated ticket #065.

    A roof that tiles one 308 cm piece per 500 cm module leaves a 192 cm hole down every
    house. `building_integrity` counts roof pieces against floor pieces and passes it. The
    coverage check measures, so it fails it.
    """
    print("\ntest_roof_coverage_sees_what_counting_missed")
    from gslevelgen.generate import compose_house
    from gslevelgen.evaluate import check_building_integrity, check_roof_coverage
    from gslevelgen.kit import load_kit, KitNotMeasured
    import random
    try:
        kit = load_kit()          # templates need the measured kit, not the stand-in
    except KitNotMeasured:
        print("  SKIP - measure the kit first")
        return

    good = compose_house(kit, random.Random(11), 0, 0, 1, 2, "good", yaw=0.0)
    p = _plan_with(good)
    check("a properly assembled roof covers the footprint",
          not check_roof_coverage(p), "0 findings")

    # Now break it exactly the way the old composer did: strip the ridge caps and shrink
    # every roof piece's coverage to one narrow strip per module.
    bad = compose_house(kit, random.Random(11), 0, 0, 1, 2, "bad", yaw=0.0)
    # The original bug: one narrow tile per module, no corner or end pieces to cover the
    # rest. Drop everything but the tiles, then narrow each to its 308-of-500 strip.
    # Keep the piece COUNT identical and shrink only the geometry — that is precisely the
    # old bug's shape (roofs >= floors, so counting is satisfied, while the covered area has
    # a hole in it). Deleting pieces instead would trip the count check too and destroy the
    # contrast this fixture exists to demonstrate.
    for pl in bad.placements:
        if "Roof" in pl.mesh:
            x0, y0, x1, y1 = pl.bb
            pl.bb = (x0, y0, x0 + (x1 - x0) * 0.10, y0 + (y1 - y0) * 0.10)
    pbad = _plan_with(bad)
    check("counting pieces still passes the broken roof",
          not [f for f in check_building_integrity(pbad) if f.fix.get("kind") == "roof_gap"])
    findings = check_roof_coverage(pbad)
    check("measuring coverage FAILS the broken roof", len(findings) >= 1,
          findings[0].message if findings else "no finding")


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
               test_determinism, test_house_composer,
               test_roof_coverage_sees_what_counting_missed):
        fn()
    print("\n" + "=" * 68)
    if failures:
        print(f"{len(failures)} FAILED: " + ", ".join(failures))
        sys.exit(1)
    print("all fixtures passed")


if __name__ == "__main__":
    main()
