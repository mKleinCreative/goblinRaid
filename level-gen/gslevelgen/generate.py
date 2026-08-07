"""
GENERATOR — the first phase of the GER loop.

Two layers:
  A. compose_house()      - a building from the modular kit (foundation/floor/walls/corners/roof)
  B. generate_settlement() - a hamlet: modules, roads, objectives, cover, and the marker set

Deliberately NOT correct-by-construction. The class framing is explicit that this phase is
"fast output, not perfection" and that "broken output is expected" — the whole point of the
loop is that the Evaluator catches what this misses. In particular the cover placement here
is naive (a scatter of copses along the approach), so some seeds *will* leave a burn objective
in open sight of the treeline. That is the failure the evaluator exists to find.

Everything is seeded and deterministic: same seed, same plan, byte for byte.
"""

from __future__ import annotations

import math
import random
from dataclasses import dataclass, field, asdict

from .geom import Disc, Rect
from .kit import Kit

# --------------------------------------------------------------------------- data model


@dataclass
class Placement:
    """One kit mesh in world space. cm, degrees."""

    mesh: str
    x: float
    y: float
    z: float
    yaw: float = 0.0


@dataclass
class Building:
    id: str
    kind: str                      # house | barn | coop | market | well | watchtower | barracks
    rect: Rect
    placements: list[Placement] = field(default_factory=list)
    notes: str = ""
    # Set when this building IS an objective's physical structure (the windmill). Without it
    # the overlap check reports the windmill colliding with itself.
    objective_id: str = ""


@dataclass
class Objective:
    id: str
    kind: str                      # granary | field | windmill
    rect: Rect


@dataclass
class Plan:
    seed: int
    synthetic_kit: bool
    site: Rect
    center: tuple[float, float]
    treeline_radius: float
    runic_site: tuple[float, float]
    buildings: list[Building] = field(default_factory=list)
    objectives: list[Objective] = field(default_factory=list)
    cover: list[Disc] = field(default_factory=list)
    roads: list[tuple[float, float, float, float]] = field(default_factory=list)
    guard_posts: list[tuple[float, float]] = field(default_factory=list)
    patrol_loops: list[list[tuple[float, float]]] = field(default_factory=list)
    civilian_anchors: list[tuple[float, float]] = field(default_factory=list)
    signposts: list[tuple[float, float]] = field(default_factory=list)

    def blockers(self) -> list[Rect]:
        return [b.rect for b in self.buildings] + [o.rect for o in self.objectives]

    def to_dict(self) -> dict:
        d = asdict(self)
        d["site"] = asdict(self.site)
        d["buildings"] = [
            {**asdict(b), "rect": asdict(b.rect)} for b in self.buildings
        ]
        d["objectives"] = [{**asdict(o), "rect": asdict(o.rect)} for o in self.objectives]
        d["cover"] = [asdict(c) for c in self.cover]
        return d


# --------------------------------------------------------------------------- layer A


def compose_house(kit: Kit, rng: random.Random, ox: float, oy: float,
                  mods_w: int, mods_h: int, hid: str, yaw: float = 0.0) -> Building:
    """
    Compose one house from the modular kit on the measured module grid.

    Order matches how the kit is authored: foundation -> floor -> perimeter walls ->
    corners -> roof. Wall pieces are chosen from the measured window variants so houses
    differ from each other without any piece being invented.
    """
    mw, mh = kit.module
    w, h = mods_w * mw, mods_h * mh
    rect = Rect(ox, oy, w, h)
    pl: list[Placement] = []

    # Measurement corrected this: the foundation is a perimeter WALL segment
    # (500 x 50 x 300), not a floor plate. Floors tile the interior; foundations ring it.
    found = next((p for p in kit.role("foundation") if p.name.endswith("_5x4")),
                 kit.one("foundation"))
    floor = next((p for p in kit.role("floor") if p.name.endswith("_5x4_01")),
                 kit.role("floor")[0])
    walls = kit.role("wall_window") or kit.role("corner")
    corner = kit.role("corner")[0] if kit.role("corner") else None
    roof_tiles = kit.role("roof_tile")
    roof_end = kit.role("roof_end")

    found_h = found.size[2]

    def put(piece, min_x: float, min_y: float, z: float, ang: float) -> None:
        wx, wy = origin_for(piece, min_x, min_y, z, ang)
        pl.append(Placement(piece.name, wx, wy, z + piece.pivot_from_min[2], ang))

    # Foundation ring: one segment per perimeter module, standing on the ground. The
    # south/north runs use the piece as-authored; east/west are turned 90 degrees, which is
    # where pivot handling matters most.
    for i in range(mods_w):
        put(found, ox + i * mw, oy, 0.0, 0.0)
        put(found, ox + i * mw, oy + h - found.size[1], 0.0, 0.0)
    for j in range(mods_h):
        put(found, ox, oy + j * mh, 0.0, 90.0)
        put(found, ox + w - found.size[1], oy + j * mh, 0.0, 90.0)

    # Floor plates tile the footprint, on top of the foundation ring.
    for i in range(mods_w):
        for j in range(mods_h):
            put(floor, ox + i * mw, oy + j * mh, found_h, 0.0)

    # Perimeter walls, sitting on the floor.
    wall_z = found_h + floor.size[2]
    for i in range(mods_w):
        wa, wb = rng.choice(walls), rng.choice(walls)
        put(wa, ox + i * mw, oy, wall_z, 0.0)
        put(wb, ox + i * mw, oy + h - wb.size[1], wall_z, 0.0)
    for j in range(mods_h):
        wa, wb = rng.choice(walls), rng.choice(walls)
        put(wa, ox, oy + j * mh, wall_z, 90.0)
        put(wb, ox + w - wb.size[1], oy + j * mh, wall_z, 90.0)

    if corner:
        for (cx, cy) in ((ox, oy), (ox + w - corner.size[0], oy),
                         (ox + w - corner.size[0], oy + h - corner.size[1]),
                         (ox, oy + h - corner.size[1])):
            put(corner, cx, cy, wall_z, 0.0)

    # A door on the south face, in a module chosen by the seed.
    doors = kit.role("door")
    door_mod = rng.randrange(mods_w)
    if doors:
        d = doors[0]
        put(d, ox + door_mod * mw + (mw - d.size[0]) / 2.0, oy - d.size[1] * 0.25,
            wall_z, 0.0)

    # Roof: tile every cell, then cap the two ends.
    roof_z = wall_z + walls[0].size[2]
    if roof_tiles:
        for i in range(mods_w):
            for j in range(mods_h):
                tile = roof_tiles[(i + j) % len(roof_tiles)]
                put(tile, ox + i * mw, oy + j * mh, roof_z, 0.0)
    if roof_end:
        e = roof_end[0]
        put(e, ox - e.size[0] * 0.5, oy + (h - e.size[1]) / 2.0, roof_z, 0.0)
        put(e, ox + w - e.size[0] * 0.5, oy + (h - e.size[1]) / 2.0, roof_z, 180.0)

    return Building(id=hid, kind="house", rect=rect, placements=pl,
                    notes=f"{mods_w}x{mods_h} modules, door on south module {door_mod}")


def origin_for(piece, min_x: float, min_y: float, z: float, yaw: float) -> tuple[float, float]:
    """
    World origin for a piece whose ROTATED footprint should start at (min_x, min_y).

    Kit pivots are not at the mesh's min corner — `kit_manifest.py` measures
    `pivot_from_min` for exactly this reason, and the first composer ignored it, placing
    every piece at a grid corner as if the pivot were there. On screen that put walls
    inside their own floor, roofs off-centre, and foundation beams sticking out past the
    footprint. Every count-based check passed the whole time.

    Rotation compounds it: yaw spins a piece about its pivot, so a centre-pivoted wall
    turned 90 degrees lands half its length away. So: rotate the local box, find where its
    min corner ends up, and offset by that.
    """
    px, py, _ = piece.pivot_from_min
    lx0, ly0 = -px, -py                       # local min corner relative to the pivot
    lx1, ly1 = lx0 + piece.size[0], ly0 + piece.size[1]
    rad = math.radians(yaw)
    c, s = math.cos(rad), math.sin(rad)
    xs, ys = [], []
    for (lx, ly) in ((lx0, ly0), (lx1, ly0), (lx1, ly1), (lx0, ly1)):
        xs.append(lx * c - ly * s)
        ys.append(lx * s + ly * c)
    return (min_x - min(xs), min_y - min(ys))


def place_prefab(kit: Kit, role: str, x: float, y: float, bid: str, kind: str,
                 yaw: float = 0.0) -> Building | None:
    """Drop a whole-prefab structure (barn, coop, well, market stall...) at x,y."""
    if not kit.role(role):
        return None
    p = kit.one(role)
    fw, fh = p.footprint
    return Building(
        id=bid, kind=kind, rect=Rect(x - fw / 2, y - fh / 2, fw, fh),
        placements=[Placement(p.name, x, y, 0.0, yaw)], notes=f"prefab {p.name}",
    )


# --------------------------------------------------------------------------- layer B


def generate_settlement(kit: Kit, seed: int) -> Plan:
    """
    A hamlet: core village, one or two farmsteads, a windmill, roads, cover and markers.

    GDD 2.8 fixes the objective mix at three, never more than two of a kind. The tutorial
    hamlet carries one of each; a generated one may roll two of a kind, which is legal.
    """
    rng = random.Random(seed)
    mw, mh = kit.module

    extent = 12000.0
    site = Rect(-extent, -extent, extent * 2, extent * 2)
    center = (rng.uniform(-800, 800), rng.uniform(-800, 800))
    treeline_radius = 9000.0

    ang = rng.uniform(0, 2 * math.pi)
    runic = (center[0] + treeline_radius * 1.05 * math.cos(ang),
             center[1] + treeline_radius * 1.05 * math.sin(ang))

    plan = Plan(seed=seed, synthetic_kit=kit.synthetic, site=site, center=center,
                treeline_radius=treeline_radius, runic_site=runic)

    # --- core village: houses in a loose ring around the well -------------------------
    well = place_prefab(kit, "well", center[0], center[1], "well", "well")
    if well:
        plan.buildings.append(well)

    # Ring placement with rejection against what is already down. Spatial packing is
    # arithmetic, and an evaluator drowning in overlap findings cannot show you anything
    # about the rule that actually matters. The generator stays naive where it counts —
    # cover — and stops fighting itself over floor space.
    #
    # The radii below are derived from measured footprints, not chosen. Tuning them by hand
    # against the synthetic kit is what dropped the pass rate to 2/8 the moment real
    # dimensions arrived.
    def free_spot(rect_w: float, rect_h: float, r_lo: float, r_hi: float,
                  tries: int = 60) -> tuple[float, float] | None:
        for _ in range(tries):
            a = rng.uniform(0, 2 * math.pi)
            r = rng.uniform(r_lo, r_hi)
            cx, cy = center[0] + r * math.cos(a), center[1] + r * math.sin(a)
            cand = Rect(cx - rect_w / 2, cy - rect_h / 2, rect_w, rect_h)
            if not any(cand.overlaps(b.rect, 250.0) for b in plan.buildings):
                return (cand.x, cand.y)
        return None

    # --- objectives: exactly three, max two of a kind ---------------------------------
    kinds = ["granary", "field", "windmill"]
    if rng.random() < 0.35:                       # sometimes roll a duplicate, still legal
        kinds = rng.sample(["granary", "field", "windmill"], 2) + [rng.choice(["granary", "field"])]
        rng.shuffle(kinds)

    used_angles: list[float] = []
    for i, kind in enumerate(kinds):
        while True:
            a = rng.uniform(0, 2 * math.pi)
            if all(abs(((a - u + math.pi) % (2 * math.pi)) - math.pi) > 0.9 for u in used_angles):
                break
        used_angles.append(a)
        if kind == "granary":
            r = rng.uniform(900, 1800)            # 2.8: granary sits where guards are thickest
            size = (mw * 2, mh * 2)
        elif kind == "field":
            r = rng.uniform(4200, 6200)           # fields sprawl at the hamlet's edges
            size = (mw * 6, mh * 6)
        else:
            r = rng.uniform(3400, 5200)           # the windmill is the landmark
            # Sized from the MEASURED prefab, not from the module grid. The base is
            # 1709 x 1607 and the sail sweeps 5354 — a 2-module box would have let the
            # overlap check pass a windmill whose sails scythe through a farmhouse.
            base = kit.role("windmill")
            if base:
                fw = max(p.footprint[0] for p in base)
                fh = max(p.footprint[1] for p in base)
                size = (fw, fh)
            else:
                size = (mw * 2, mh * 2)
        ox = center[0] + r * math.cos(a) - size[0] / 2
        oy = center[1] + r * math.sin(a) - size[1] / 2
        plan.objectives.append(Objective(id=f"obj_{i}_{kind}", kind=kind,
                                         rect=Rect(ox, oy, size[0], size[1])))
        if kind == "windmill":
            wm = place_prefab(kit, "windmill", ox + size[0] / 2, oy + size[1] / 2,
                              f"windmill_{i}", "windmill")
            if wm:
                wm.rect = plan.objectives[-1].rect   # the objective owns the footprint
                wm.objective_id = plan.objectives[-1].id
                plan.buildings.append(wm)


    n_houses = rng.randint(5, 8)
    house_ring = (mw * 3.0, mw * 7.0)
    for i in range(n_houses):
        mods_w, mods_h = rng.randint(1, 2), rng.randint(1, 2)
        spot = free_spot(mods_w * mw, mods_h * mh, *house_ring)
        if spot is None:
            continue                     # the ring is full; fewer houses is not a failure
        b = compose_house(kit, rng, spot[0], spot[1], mods_w, mods_h, f"house_{i}")
        plan.buildings.append(b)

    if kit.role("market"):
        sw, sh = kit.one("market").footprint
        for i in range(rng.randint(2, 4)):
            spot = free_spot(sw, sh, mw * 1.2, mw * 2.6)
            if spot is None:
                continue
            st = place_prefab(kit, "market", spot[0] + sw / 2, spot[1] + sh / 2,
                              f"stall_{i}", "market")
            if st:
                plan.buildings.append(st)

    # --- farmstead: barn + coop out past the houses -----------------------------------
    for role, kind in (("barn", "barn"), ("coop", "coop")):
        if not kit.role(role):
            continue
        fw, fh = kit.one(role).footprint
        spot = free_spot(fw, fh, mw * 9.0, mw * 13.0)
        if spot:
            b = place_prefab(kit, role, spot[0] + fw / 2, spot[1] + fh / 2, f"{kind}_0", kind)
            if b:
                plan.buildings.append(b)

    # --- roads: treeline -> village core, converging (2.8 wayfinding) ------------------
    for i in range(rng.randint(2, 3)):
        a = 2 * math.pi * i / 3 + rng.uniform(-0.3, 0.3)
        ex = center[0] + treeline_radius * math.cos(a)
        ey = center[1] + treeline_radius * math.sin(a)
        plan.roads.append((ex, ey, center[0], center[1]))
        plan.signposts.append((center[0] + 3000 * math.cos(a), center[1] + 3000 * math.sin(a)))

    # --- cover: NAIVE ON PURPOSE ------------------------------------------------------
    # A scatter of copses on the approaches. This does not check sightlines, which is
    # exactly why some seeds fail the cover-guarantee rule and need refining.
    for i in range(rng.randint(6, 11)):
        a = rng.uniform(0, 2 * math.pi)
        r = rng.uniform(2600, 7200)
        plan.cover.append(Disc(center[0] + r * math.cos(a), center[1] + r * math.sin(a),
                               rng.uniform(420, 900)))

    # --- the 3.3 marker set: Generator owns *where*, Town owns behaviour and counts ----
    for i in range(4):
        a = 2 * math.pi * i / 4 + rng.uniform(-0.2, 0.2)
        plan.guard_posts.append((center[0] + 1500 * math.cos(a), center[1] + 1500 * math.sin(a)))
    loop = [(center[0] + 6000 * math.cos(2 * math.pi * k / 6),
             center[1] + 6000 * math.sin(2 * math.pi * k / 6)) for k in range(6)]
    plan.patrol_loops.append(loop)
    for b in plan.buildings:
        if b.kind == "house":
            cx, cy = b.rect.center
            plan.civilian_anchors.append((cx, cy - b.rect.h))

    return plan
