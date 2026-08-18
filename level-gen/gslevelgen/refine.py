"""
REFINER — the third phase, and the bounded one.

The rule from the class is explicit: don't start over from scratch, don't guess at unrelated
changes, fix the documented failure. So every fix here is driven by a specific finding's
`fix` payload and touches only what that finding named. A re-roll would be easier and would
teach us nothing about whether the evaluator's rules are satisfiable.

Escalation matches the three passes:
    pass 1  smallest fix that addresses the documented failure
    pass 2  tighten the same correction with the same error context
    pass 3  final attempt
then the caller's circuit breaker stops and hands over a problem statement.
"""

from __future__ import annotations

import math
import random
from copy import deepcopy

from .geom import Disc, Rect, ring_points, seg_intersects_disc, seg_intersects_rect
from .generate import DESTRUCTION, REQUIRED_KINDS, Plan

# Pass-indexed strength. Pass 1 nudges; pass 3 shoves.
COVER_RADIUS = {1: 520.0, 2: 780.0, 3: 1150.0}
NUDGE = {1: 400.0, 2: 900.0, 3: 1600.0}


def _move_building(plan: Plan, bid: str, dx: float, dy: float) -> None:
    for b in plan.buildings:
        if b.id == bid:
            b.rect = Rect(b.rect.x + dx, b.rect.y + dy, b.rect.w, b.rect.h)
            for p in b.placements:
                p.x += dx
                p.y += dy
                # Translate the cached world bounds too. Forgetting this left every moved
                # house's roof bounds at its old location, and the coverage check — correctly
                # — reported 100% of the footprint with nothing above it.
                if p.bb:
                    x0, y0, x1, y1 = p.bb
                    p.bb = (x0 + dx, y0 + dy, x1 + dx, y1 + dy)
            return


MOVE_MARGIN = 200.0          # cm; clearance a relocated building must land with


def _blocked(plan: Plan, bid: str, cand: Rect) -> bool:
    """
    Would a building sitting at `cand` overlap anything other than itself, or sit on a road?

    The road test is not decoration. Without it this function rotated a house up to 1.6 rad
    off the perpendicular to dodge a neighbour and set it back down on the very road the push
    was meant to escape — seed 3 moved house_0 3794 -> 3405 -> 4265 -> 5793 across three
    passes, never clearing road 0, and its final position opened the treeline sightline onto
    opt_0_field that the circuit breaker then escalated. A repair that satisfies the check it
    was aimed at while breaking two others is not a repair.

    Every road is tested, not just the one being escaped, so a move can never author a fresh
    roads_clear failure either. Same inflation the evaluator uses, so the two agree on what
    "on the road" means.
    """
    me = next((b for b in plan.buildings if b.id == bid), None)
    mine = me.objective_id if me else ""
    for b in plan.buildings:
        if b.id == bid:
            continue
        if mine and b.objective_id == mine:
            continue                       # stalls and their market; the windmill and its own
        if cand.overlaps(b.rect, MOVE_MARGIN):
            return True
    for o in plan.objectives:
        if mine and o.id == mine:
            continue
        if cand.overlaps(o.rect, MOVE_MARGIN):
            return True
    for ax, ay, bx, by in plan.roads:
        if seg_intersects_rect(ax, ay, bx, by, cand.inflated(-40.0)):
            return True
    return False


TREELINE_SAMPLES = 48        # must match evaluate.py; a building is cover for these rays


def _exposure(plan: Plan, bid: str = "", cand: Rect | None = None) -> int:
    """
    How many (objective, treeline-ray) pairs are currently unoccluded, optionally with `bid`
    relocated to `cand`.

    Buildings are cover. A house standing between the treeline and the windmill is doing the
    cover guarantee's job whether or not anyone placed it there for that reason, so a repair
    that relocates it can silently destroy a sightline that was passing. Seed 6 lost the
    windmill that way: house_5 was pushed off a road, landed on the windmill, was moved clear
    of the windmill, and the last move took the occluder with it.

    Mirrors check_cover_guarantee's logic exactly, including the `is not obj.rect` identity
    test that stops the windmill from occluding itself.
    """
    ring = ring_points(*plan.center, plan.treeline_radius, TREELINE_SAMPLES)
    rects = [cand if (bid and b.id == bid and cand is not None) else b.rect
             for b in plan.buildings]
    total = 0
    for obj in plan.objectives:
        tx, ty = obj.rect.center
        for sx, sy in ring:
            if any(seg_intersects_disc(sx, sy, tx, ty, d) for d in plan.cover):
                continue
            if any(r is not obj.rect and seg_intersects_rect(sx, sy, tx, ty, r) for r in rects):
                continue
            total += 1
    return total


def _move_building_clear(plan: Plan, bid: str, dx: float, dy: float) -> bool:
    """
    Move a building, but land it somewhere free.

    The blind version made the refiner oscillate: it would clear one overlap by shoving a
    house straight into another, so the fail count went 1 -> 2 -> 1 across the three passes
    and the circuit breaker fired on a layout that was satisfiable. The fix is not more
    passes. A move that has to be undone next pass was never a fix, so the destination is
    checked before it is taken — the same "spatial packing is arithmetic" point the
    generator's rejection sampler already makes, applied on the repair side.

    Falls back to the requested move if nothing nearby is clear, so a crowded layout still
    makes progress and still escalates honestly rather than silently doing nothing.
    """
    me = next((b for b in plan.buildings if b.id == bid), None)
    if me is None:
        return False
    base = math.hypot(dx, dy) or 1.0
    bearing = math.atan2(dy, dx)
    before = _exposure(plan)
    fallback = None
    for scale in (1.0, 1.5, 2.0, 2.6):
        for turn in (0.0, 0.5, -0.5, 1.0, -1.0, 1.6, -1.6):
            d = base * scale
            nx, ny = d * math.cos(bearing + turn), d * math.sin(bearing + turn)
            cand = Rect(me.rect.x + nx, me.rect.y + ny, me.rect.w, me.rect.h)
            if _blocked(plan, bid, cand):
                continue
            if fallback is None:
                fallback = (nx, ny)          # geometrically clear, cover not yet judged
            if _exposure(plan, bid, cand) <= before:
                _move_building(plan, bid, nx, ny)
                return True
    # Nothing preserved the sightlines. Take the geometrically clear spot if there was one -
    # an overlap is a harder failure than an exposed ray, and the cover findings that result
    # are themselves refinable by add_cover on a later pass.
    nx, ny = fallback if fallback else (dx, dy)
    _move_building(plan, bid, nx, ny)
    return fallback is not None


def _away_from_center(plan: Plan, x: float, y: float, dist: float) -> tuple[float, float]:
    ax, ay = x - plan.center[0], y - plan.center[1]
    n = math.hypot(ax, ay) or 1.0
    return (ax / n * dist, ay / n * dist)


def refine(plan: Plan, findings: list[dict], pass_no: int) -> tuple[Plan, list[str]]:
    """Apply one targeted correction per finding. Returns a new plan and what changed."""
    p = deepcopy(plan)
    rng = random.Random(plan.seed * 1000 + pass_no)
    log: list[str] = []

    for f in findings:
        if f["severity"] != "fail" and f["fix"].get("kind") != "add_signpost":
            continue
        fix = f["fix"]
        kind = fix.get("kind")

        if kind == "add_cover":
            # Close the reported ARC, not one ray. Copses sit in a short belt just outside
            # the objective — that is the hamlet's own cover language (2.4: hedgerows,
            # fences, haycarts near the thing you are sneaking up on), and it is also where
            # a given radius occludes the widest angle for the least scenery.
            tx, ty = fix["target"]
            a0, a1 = fix["angle_from"], fix["angle_to"]
            width = max(fix["arc_width_rad"], 1e-3)
            r = COVER_RADIUS[pass_no]
            d = {1: 1500.0, 2: 1250.0, 3: 1050.0}[pass_no]   # belt distance from objective
            step = 2.0 * math.asin(min(0.95, r / d)) * 0.8   # overlap adjacent copses
            n = max(1, int(math.ceil(width / step)))
            for i in range(n):
                a = a0 + width * (i + 0.5) / n
                p.cover.append(Disc(tx + d * math.cos(a), ty + d * math.sin(a), r))
            log.append(f"cover: {n} copse(s) r={r:.0f} closing a "
                       f"{math.degrees(width):.0f}° arc on {fix['objective']}")

        elif kind == "overlap":
            # The evaluator names which of the pair is movable; moving the other is a silent
            # no-op, which is exactly how the first version failed to converge.
            target = fix.get("move", fix["b"])
            b = next((b for b in p.buildings if b.id == target), None)
            if b is None:
                log.append(f"overlap: {target} is an anchor (objective) — cannot move it")
                continue
            other = next((o.rect for o in p.objectives if o.id == fix["a"]),
                         next((x.rect for x in p.buildings if x.id == fix["a"]), None))
            if other is not None:
                # Push directly apart along the line between them: the shortest move that
                # separates them, rather than a generic outward shove.
                ax, ay = other.center
                bx, by = b.rect.center
                vx, vy = bx - ax, by - ay
                n = math.hypot(vx, vy) or 1.0
                need = (max(other.w, other.h) + max(b.rect.w, b.rect.h)) / 2.0 + 200.0
                d = max(NUDGE[pass_no], need - n + 200.0)
                dx, dy = vx / n * d, vy / n * d
            else:
                dx, dy = _away_from_center(p, *b.rect.center, NUDGE[pass_no])
            clean = _move_building_clear(p, target, dx, dy)
            log.append(f"overlap: moved {target} {math.hypot(dx, dy):.0f}cm clear of "
                       f"{fix['a']}" + ("" if clean else " (no clear spot; nudged anyway)"))

        elif kind == "road_through_building":
            b = next((b for b in p.buildings if b.id == fix["building"]), None)
            if b:
                ax, ay, bx, by = p.roads[fix["road"]]
                # Push perpendicular to the road, which is the shortest way off it.
                rx, ry = bx - ax, by - ay
                n = math.hypot(rx, ry) or 1.0
                px, py = -ry / n, rx / n
                side = 1.0 if ((b.rect.center[0] - ax) * px + (b.rect.center[1] - ay) * py) >= 0 else -1.0
                d = NUDGE[pass_no]
                _move_building_clear(p, b.id, px * side * d, py * side * d)
                log.append(f"road: pushed {b.id} {d:.0f}cm off road {fix['road']}")

        elif kind == "unreachable":
            # Open the approach: shrink whatever sits between the objective and the core.
            ox, oy = fix["at"]
            worst, best_d = None, 1e18
            for b in p.buildings:
                d = math.dist(b.rect.center, (ox, oy))
                if d < best_d and b.kind in ("house", "market"):
                    worst, best_d = b, d
            if worst:
                dx, dy = _away_from_center(p, *worst.rect.center, NUDGE[pass_no])
                _move_building_clear(p, worst.id, dx, dy)
                log.append(f"reach: moved {worst.id} out of the approach to {fix['objective']}")

        elif kind == "objective_dupes" or kind == "objective_not_required":
            # Re-kind one offender to whichever required type is missing. Only required
            # objectives are in play here — an optional field is not a duplicate of anything.
            have = [o.kind for o in p.objectives if o.required]
            missing = [k for k in REQUIRED_KINDS if k not in have]
            if missing:
                for o in p.objectives:
                    if o.required and o.kind == fix["objective_kind"]:
                        o.kind = missing[0]
                        o.destruction = DESTRUCTION[missing[0]]
                        o.id = o.id.rsplit("_", 1)[0] + "_" + missing[0]
                        log.append(f"mix: re-kinded a {fix['objective_kind']} "
                                   f"to {missing[0]}")
                        break

        elif kind == "wrong_destruction":
            # 2.8 fixes the destruction method per objective type; it is not a free choice,
            # so this is a one-line correction rather than a placement change.
            for o in p.objectives:
                if o.id == fix["objective"]:
                    o.destruction = fix["want"]
                    log.append(f"destruction: {o.id} set to {fix['want']} (2.8)")
                    break

        elif kind == "objective_count":
            log.append("mix: objective count is a generator fault, not a placement one — "
                       "cannot be refined in place")

        elif kind == "add_signpost":
            ox, oy = fix["at"]
            dx, dy = _away_from_center(p, ox, oy, -1200.0)
            p.signposts.append((ox + dx, oy + dy))
            log.append(f"wayfinding: signpost near {fix['objective']}")

        elif kind == "roof_gap" or kind == "no_door":
            log.append(f"building_integrity: {fix['kind']} on {fix.get('building')} is a "
                       f"composer fault — regenerate that building, do not patch it here")

    return p, log
