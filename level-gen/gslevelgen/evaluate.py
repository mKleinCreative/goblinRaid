"""
EVALUATOR — the second phase, and the one the whole pipeline is really about.

The class framing: *if code can verify it, use code*; a verifier agent handles only what
code cannot. So the checks below are deterministic geometry, and the optional agent pass
(--verifier) is reserved for judgement — does this read as a hamlet, is the silhouette
legible — never for anything a raycast can settle.

Every check cites the GDD section it enforces. None of these rules were invented here;
they are the constraints 2.8 and 3.3 already committed to. That is the difference between
an evaluator and a generic validity check.

Each finding carries `fix` — the specific data the refiner needs to make a *targeted*
correction rather than a blind re-roll.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, asdict

from .geom import Disc, Rect, cell_of, flood_reachable, ring_points, seg_intersects_disc, \
    seg_intersects_rect
from .generate import Plan

TREELINE_SAMPLES = 48        # rays cast per objective from the treeline ring
NAV_CELL = 250.0             # cm; coarse reachability grid
BUILDING_MARGIN = 150.0      # cm; buildings may not sit closer than this


@dataclass
class Finding:
    check: str
    gdd: str
    severity: str            # "fail" | "warn"
    message: str
    fix: dict                # structured payload for the refiner

    def to_dict(self) -> dict:
        return asdict(self)


# --------------------------------------------------------------------------- checks


def check_cover_guarantee(plan: Plan) -> list[Finding]:
    """
    GDD 2.8: broken sightlines guaranteed between the treeline and EVERY objective.

    The headline rule. An objective visible in an unbroken line from the treeline means the
    player is confirmed before the first spark, "First Spark Unseen" (+40, 2.9) is unwinnable,
    and 2.4's quiet half of the raid is dead on that map.
    """
    out: list[Finding] = []
    occluders_r = [b.rect for b in plan.buildings]
    ring = ring_points(*plan.center, plan.treeline_radius, TREELINE_SAMPLES)

    for obj in plan.objectives:
        tx, ty = obj.rect.center
        exposed_idx: list[int] = []
        for i, (sx, sy) in enumerate(ring):
            blocked = any(seg_intersects_disc(sx, sy, tx, ty, d) for d in plan.cover) or \
                      any(r is not obj.rect and seg_intersects_rect(sx, sy, tx, ty, r)
                          for r in occluders_r)
            if not blocked:
                exposed_idx.append(i)
        if not exposed_idx:
            continue

        # Group exposed samples into contiguous ARCS (wrapping at the ring seam). An
        # objective open from 13 of 48 samples is not one hole to plug, it is one or more
        # arcs to close — reporting it as a single ray was why the first refiner could
        # never converge.
        arcs: list[list[int]] = []
        for i in exposed_idx:
            if arcs and (i - arcs[-1][-1]) % TREELINE_SAMPLES == 1:
                arcs[-1].append(i)
            else:
                arcs.append([i])
        if len(arcs) > 1 and (arcs[0][0] - arcs[-1][-1]) % TREELINE_SAMPLES == 1:
            arcs[0] = arcs[-1] + arcs[0]         # stitch across the seam
            arcs.pop()

        for k, arc in enumerate(arcs):
            a0 = math.atan2(ring[arc[0]][1] - ty, ring[arc[0]][0] - tx)
            a1 = math.atan2(ring[arc[-1]][1] - ty, ring[arc[-1]][0] - tx)
            width = (a1 - a0) % (2 * math.pi)
            out.append(Finding(
                check="cover_guarantee",
                gdd="2.8",
                severity="fail",
                message=(f"{obj.id} ({obj.kind}) is in unbroken line of sight from the "
                         f"treeline across an arc of {math.degrees(width):.0f}° "
                         f"({len(arc)}/{TREELINE_SAMPLES} samples)"),
                fix={"kind": "add_cover", "objective": obj.id, "arc": k,
                     "target": [round(tx, 1), round(ty, 1)],
                     "angle_from": round(a0, 4), "angle_to": round(a0 + width, 4),
                     "arc_width_rad": round(width, 4), "samples": len(arc)},
            ))
    return out


def check_objective_mix(plan: Plan) -> list[Finding]:
    """GDD 2.8: three objectives, never more than two of a kind."""
    out = []
    n = len(plan.objectives)
    if n != 3:
        out.append(Finding("objective_mix", "2.8", "fail",
                           f"{n} objectives; the raid assigns exactly three",
                           {"kind": "objective_count", "have": n, "want": 3}))
    counts: dict[str, int] = {}
    for o in plan.objectives:
        counts[o.kind] = counts.get(o.kind, 0) + 1
    for kind, c in sorted(counts.items()):
        if c > 2:
            out.append(Finding("objective_mix", "2.8", "fail",
                               f"{c} x {kind}; never more than two of a kind",
                               {"kind": "objective_dupes", "objective_kind": kind, "count": c}))
    return out


def check_reachability(plan: Plan) -> list[Finding]:
    """
    Every objective must be reachable on foot from the runic site, and so must the way back
    (GDD 2.1: cross the forest, 2.2: exit through the portal). An unreachable objective is
    an unwinnable raid — the deck's own worked example of a constraint violation.
    """
    reach = flood_reachable(plan.blockers(), plan.runic_site, plan.site, NAV_CELL,
                            margin=60.0)
    out = []
    for obj in plan.objectives:
        cx, cy = obj.rect.center
        # Sample just outside the footprint on four sides — you burn it from outside.
        touch = [(cx, obj.rect.y - NAV_CELL), (cx, obj.rect.y2 + NAV_CELL),
                 (obj.rect.x - NAV_CELL, cy), (obj.rect.x2 + NAV_CELL, cy)]
        if not any(cell_of(p, plan.site, NAV_CELL) in reach for p in touch):
            out.append(Finding("reachability", "2.1/2.2", "fail",
                               f"{obj.id} cannot be reached on foot from the runic site",
                               {"kind": "unreachable", "objective": obj.id,
                                "at": [round(cx, 1), round(cy, 1)]}))
    return out


def check_no_overlap(plan: Plan) -> list[Finding]:
    """Buildings may not intersect. Week 1 shipped the physical version of this."""
    out = []
    # (id, rect, movable, linked_objective). Objectives are anchors: the granary sits where
    # the guards are thickest and the field sprawls at the edge (2.8), so a conflict is
    # resolved by moving the house, never the objective.
    items: list[tuple[str, Rect, bool, str]] = (
        [(b.id, b.rect, b.kind not in ("windmill",), b.objective_id) for b in plan.buildings]
        + [(o.id, o.rect, False, o.id) for o in plan.objectives]
    )
    for i in range(len(items)):
        for j in range(i + 1, len(items)):
            ia, ra, ma, oa = items[i]
            ib, rb, mb, ob = items[j]
            if ra is rb:
                continue
            if oa and oa == ob:
                continue                      # the windmill is not overlapping itself
            if not ra.overlaps(rb, BUILDING_MARGIN):
                continue
            # Name the movable one first so the refiner does not try to shove an anchor.
            if ma and not mb:
                a_id, b_id = ib, ia
            elif mb and not ma:
                a_id, b_id = ia, ib
            elif ma and mb:
                a_id, b_id = ia, ib
            else:
                out.append(Finding("no_overlap", "2.8", "fail",
                                   f"{ia} overlaps {ib}, and neither can be moved",
                                   {"kind": "overlap_immovable", "a": ia, "b": ib}))
                continue
            out.append(Finding("no_overlap", "2.8", "fail",
                               f"{a_id} overlaps {b_id}",
                               {"kind": "overlap", "a": a_id, "b": b_id, "move": b_id}))
    return out


def check_roads_clear(plan: Plan) -> list[Finding]:
    """
    A road may not run through a building. This is week 1's literal bug, promoted to a rule:
    the on-foot walkthrough found "a road cutting through a house" (GDD 2.8, 4.5).
    """
    out = []
    for k, (ax, ay, bx, by) in enumerate(plan.roads):
        for b in plan.buildings:
            if seg_intersects_rect(ax, ay, bx, by, b.rect.inflated(-40.0)):
                out.append(Finding("roads_clear", "2.8", "fail",
                                   f"road {k} runs through {b.id}",
                                   {"kind": "road_through_building", "road": k,
                                    "building": b.id}))
    return out


def check_wayfinding(plan: Plan) -> list[Finding]:
    """
    GDD 2.8: the layout owes the player wayfinding — roads converge on the core, signposts
    mark the miles, and the environment leads you there instead of a HUD arrow. Enforced as:
    every objective has a road or signpost within reasonable sight.
    """
    out = []
    for obj in plan.objectives:
        cx, cy = obj.rect.center
        near_road = any(
            seg_intersects_disc(ax, ay, bx, by, Disc(cx, cy, 3000.0))
            for (ax, ay, bx, by) in plan.roads
        )
        near_sign = any(math.dist((cx, cy), s) < 3500.0 for s in plan.signposts)
        if not (near_road or near_sign):
            out.append(Finding("wayfinding", "2.8", "warn",
                               f"{obj.id} has neither a road nor a signpost within reach; "
                               f"the player is told what to burn, not where",
                               {"kind": "add_signpost", "objective": obj.id,
                                "at": [round(cx, 1), round(cy, 1)]}))
    return out


def check_building_integrity(plan: Plan) -> list[Finding]:
    """Layer A: a composed house must have a roof over every module and at least one door."""
    out = []
    for b in plan.buildings:
        if b.kind != "house":
            continue
        roofs = [p for p in b.placements if "Roof" in p.mesh]
        # Floors are the per-module count. Foundations are a perimeter ring — counting those
        # as modules made a 1x1 house look like it needed four roof pieces.
        floors = [p for p in b.placements if "Floor" in p.mesh]
        doors = [p for p in b.placements
                 if p.mesh.startswith("SM_Door") or "_Door_" in p.mesh]
        if floors and len(roofs) < len(floors):
            out.append(Finding("building_integrity", "2.8", "fail",
                               f"{b.id}: {len(roofs)} roof pieces for {len(floors)} modules — "
                               f"the roof does not close",
                               {"kind": "roof_gap", "building": b.id}))
        if not doors:
            out.append(Finding("building_integrity", "2.8", "fail",
                               f"{b.id} has no door", {"kind": "no_door", "building": b.id}))
    return out


# 8%: a correctly-roofed reference house measures ~5% uncovered by this AABB sampling,
# because floor plates edge out past the roof bounds. Below that is noise, not a hole.
ROOF_COVERAGE_TOLERANCE = 0.08


def check_roof_coverage(plan: Plan) -> list[Finding]:
    """
    Does the roof actually cover the house?

    `check_building_integrity` counts roof pieces against floor pieces, and a count cannot
    see a hole. The previous composer tiled one 308 cm roof piece per 500 cm module, leaving
    a 192 cm gap down every house, and passed — until someone looked at it in the editor.

    So this measures. Sample the footprint on a grid and ask whether any placed roof piece's
    world XY bounds contain each point. Not a mesh-accurate test — a pitched roof's AABB is
    generous — but it catches the class of failure that matters: a strip of the building with
    nothing above it at all.
    """
    out = []
    for b in plan.buildings:
        if b.kind != "house":
            continue
        # The __ROOF_TOO_NARROW__ sentinel branch that used to sit here was unreachable: commit
        # 1ba0a7b ("Stop synthesising buildings") deleted the only line that ever emitted the
        # sentinel, leaving a consumer with no producer. Removed in #115.
        roofs = [p for p in b.placements if "Roof" in p.mesh and p.bb]
        if not roofs:
            out.append(Finding("roof_coverage", "2.8", "fail", f"{b.id} has no roof",
                               {"kind": "roof_missing", "building": b.id}))
            continue
        # Sample the FLOORED area, not the bounding rect. Since houses carry their own base
        # yaw (STYLE_GUIDE R1), the rect is a rotated AABB whose corners are outside the
        # building — sampling those reported phantom holes. The real question is "is every
        # floored square roofed", which is rotation-agnostic.
        # INTERIOR floor plates only. "Floor" in the name also catches
        # SM_House_Floor_5x4_Overang (a jetty where the upper storey oversails, which is
        # meant to stick out past the roof) and _Overang_Beam (a bracket, not a floor).
        # Sampling those reported 45-57% "uncovered" on hand-authored houses that are
        # correctly roofed — the check was wrong, not the buildings.
        floors = [p for p in b.placements
                  if "Floor" in p.mesh and p.bb
                  and "Overang" not in p.mesh and "Beam" not in p.mesh
                  and "Terrace" not in p.mesh]
        if not floors:
            continue
        pts = []
        for f in floors:
            x0, y0, x1, y1 = f.bb
            for u in (0.3, 0.7):
                for v in (0.3, 0.7):
                    pts.append((x0 + (x1 - x0) * u, y0 + (y1 - y0) * v))
        uncovered = sum(
            0 if any(r.bb[0] <= px <= r.bb[2] and r.bb[1] <= py <= r.bb[3] for r in roofs)
            else 1
            for (px, py) in pts
        )
        frac = uncovered / float(len(pts))
        if frac > ROOF_COVERAGE_TOLERANCE:
            out.append(Finding("roof_coverage", "2.8", "fail",
                               f"{b.id}: {frac:.0%} of the footprint has no roof above it",
                               {"kind": "roof_hole", "building": b.id,
                                "uncovered_fraction": round(frac, 3)}))
    return out


def check_buildable_ground(plan: Plan) -> list[Finding]:
    """
    Week 1 also shipped "a windmill sited on an unbuildable rock face". Checking that needs
    the terrain heightfield, which lives in the editor and is not available to this solver.

    Reported as SKIPPED rather than silently passing: a check that cannot run must not look
    like a check that passed.
    """
    return [Finding("buildable_ground", "2.8", "warn",
                    "SKIPPED — needs the terrain heightfield, which only the editor has. "
                    "Verify on the in-editor walkthrough.",
                    {"kind": "skipped"})]


DETERMINISTIC = [
    check_cover_guarantee,
    check_objective_mix,
    check_reachability,
    check_no_overlap,
    check_roads_clear,
    check_wayfinding,
    check_building_integrity,
    check_roof_coverage,
    check_buildable_ground,
]


def evaluate(plan: Plan) -> dict:
    findings: list[Finding] = []
    for fn in DETERMINISTIC:
        findings.extend(fn(plan))
    fails = [f for f in findings if f.severity == "fail"]
    return {
        "seed": plan.seed,
        "passed": not fails,
        "fail_count": len(fails),
        "warn_count": len(findings) - len(fails),
        "findings": [f.to_dict() for f in findings],
    }
