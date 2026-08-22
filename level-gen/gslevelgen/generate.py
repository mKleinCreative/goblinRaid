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

import json
import math
import random
from dataclasses import dataclass, field, asdict
from pathlib import Path

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
    # World-space XY bounds of this piece once placed and rotated. Carried on the plan so
    # the evaluator can measure actual coverage instead of counting pieces — counting is
    # what let a roof with a 192 cm hole in it pass every check.
    bb: tuple[float, float, float, float] = (0.0, 0.0, 0.0, 0.0)
    # Full transform, not just yaw. 69 of House_2x1_T9's 220 pieces carry a non-unit scale
    # and most are MIRRORS (-1) — stamping those unmirrored is why stairs and railings did
    # not meet. The chimney is 10 Fireplace_Tiling segments at scale 1.375, spaced 137.5
    # apart; drop the scale and every segment renders 37.5 short of its neighbour.
    pitch: float = 0.0
    roll: float = 0.0
    scale: tuple[float, float, float] = (1.0, 1.0, 1.0)


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
    kind: str                      # market | statue | windmill | field | house
    rect: Rect
    # 2.8: the raid assigns three REQUIRED objectives (one market, one statue, one windmill)
    # and gates extraction on those. Wheat fields and houses are optional — worth points,
    # never gating. Only the required three are counted by check_objective_mix.
    required: bool = True
    # 2.8: "The statue is the one target that doesn't burn: it has to be brought down, stone
    # on stone." Every other objective is a burn. The evaluator enforces this.
    destruction: str = "fire"      # fire | topple


# GDD 2.8, revised 2026-08-14 (queue #156). The raid assigns three required objectives; the
# hand-authored tutorial hamlet carries one of each, and a generated hamlet may roll two of
# a kind but never three (1: "a randomized mix across many hamlets, never more than two of
# a kind, is the post-slice generator's job"). This generator IS that post-slice job.
REQUIRED_KINDS = ("market", "statue", "windmill")
OPTIONAL_KINDS = ("field", "house")

# Aisle between market stalls, and the border between the outermost stall and the edge of
# the market objective's footprint. One goblin-width of walking room.
STALL_GAP = 200.0

# Walking room the generator leaves between a placed anchor and the next thing it sites.
# Matches the evaluator's BUILDING_MARGIN with room to spare, so a plan does not generate
# overlaps the refiner then has to spend its three passes undoing.
BUILDING_CLEARANCE = 600.0

# 2.8: "The statue is the one target that doesn't burn: it has to be brought down, stone on
# stone." Everything else on the roster is a burn.
DESTRUCTION = {
    "market": "fire",
    "statue": "topple",
    "windmill": "fire",
    "field": "fire",
    "house": "fire",
}


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


REFERENCE_JSON = Path(__file__).resolve().parent.parent / "reference" / "TutorialIsland.json"
# Hand-authored houses, in ascending size. Taverns (Innbase) and working structures
# (WaterMill_Closed) are deliberately excluded — different building types.
# House_1x3_10 is excluded: it is L-shaped, and check_roof_coverage cannot tell its
# unroofed wing from a hole (it reports ~55% uncovered on a building a human built
# correctly). Shipping only what the evaluator can verify, rather than loosening the
# evaluator to admit a building it cannot check. Re-add it with a footprint-polygon
# coverage test instead of an AABB one.
TEMPLATE_HOUSES = ("House_2x1_L7_Detailed", "House_2x1_T9")
_TEMPLATES: dict | None = None
# How much of the below-foundation undercroft to bury. 1.0 puts the foundation
# course exactly on the ground plane; less leaves the rock facade showing, which
# is the wiggle room the facade exists to provide on sloped terrain.
BURY_FRACTION = 0.85


def load_templates() -> dict:
    """
    The hand-authored buildings, as placement lists.

    THREE ATTEMPTS AT SYNTHESIS FAILED, so this stopped synthesising.

      1. tile one roof piece per floor cell -> a 192 cm hole down every house
      2. two slopes at a ridge -> closed, but read as a shed
      3. a perimeter ring of Base+Top pairs per STYLE_GUIDE R3/R5 -> a mass of overlapping
         geometry radiating outward; the capture looked like an exploded starfish

    The style guide's rules are statistical — piece ratios, pairing, storey heights, yaw
    discipline. They are all true and none of them told me where a roof piece actually goes.
    That spatial grammar is in the reference buildings, so this reads it out and stamps it
    rather than re-deriving it. `gs_buildings.py` reached the same conclusion about building
    identity: the level already carries the answer.
    """
    global _TEMPLATES
    if _TEMPLATES is None:
        if not REFERENCE_JSON.exists():
            raise FileNotFoundError(
                f"{REFERENCE_JSON} not found. Extract the reference buildings first:\n"
                f"    python gs_ue.py level-gen\\extract_buildings.py --timeout 900"
            )
        data = json.loads(REFERENCE_JSON.read_text(encoding="utf-8"))
        _TEMPLATES = {b["id"]: b for b in data["buildings"] if b["id"] in TEMPLATE_HOUSES}
    return _TEMPLATES


def compose_house(kit: Kit, rng: random.Random, ox: float, oy: float,
                  mods_w: int, mods_h: int, hid: str, yaw: float | None = None) -> Building:
    """
    Stamp a hand-authored house at (ox, oy), rotated to its own base yaw.

    Variation comes from choosing among the reference houses and rotating them, not from
    re-deriving the assembly. STYLE_GUIDE R1 (own base yaw), R2 (storeys), R3 (roof is the
    biggest role), R4/R5 (roof grammar), R7 (beams), R8/R10 (piece variety) are all satisfied
    by construction, because a human satisfied them.

    `mods_w`/`mods_h` now select a template by size rather than dictating a footprint.
    """
    templates = load_templates()
    want = max(1, mods_w) * max(1, mods_h)
    names = sorted(templates)
    tpl = templates[names[min(len(names) - 1, max(0, want - 1))]]

    # `if yaw else` treated an explicit yaw=0.0 as "unspecified" and rolled a random one,
    # so every test that thought it was pinning the rotation was not. None means unset.
    base_yaw = rng.uniform(0.0, 360.0) if yaw is None else yaw   # R1
    rad = math.radians(base_yaw)
    ca, sa = math.cos(rad), math.sin(rad)

    # The FOUNDATION COURSE is the ground line. Below it the reference carries an
    # undercroft — stone pillar bases, lower stairs, the rock facade — which is authored to
    # be buried so a house can sit in uneven terrain instead of perching on it. Stamping the
    # lowest piece at z=0 stood that whole basement proud of the ground.
    found_z = min((p["rel"][2] for p in tpl["pieces"] if "Foundation" in p["mesh"]),
                  default=0.0)
    sink = -found_z * BURY_FRACTION

    pl: list[Placement] = []
    fxs, fys = [], []
    for piece_rec in tpl["pieces"]:
        piece = kit.pieces.get(piece_rec["mesh"])
        if piece is None:
            continue                                            # not in the measured kit
        lx, ly, lz = piece_rec["rel"]
        wx = ox + lx * ca - ly * sa
        wy = oy + lx * sa + ly * ca
        wyaw = (piece_rec["yaw"] + base_yaw) % 360.0
        sc = tuple(piece_rec.get("scale", (1.0, 1.0, 1.0)))
        bb = world_bb(piece, wx, wy, wyaw)
        pl.append(Placement(piece.name, wx, wy, lz + sink, wyaw, bb,
                            piece_rec.get("pitch", 0.0), piece_rec.get("roll", 0.0), sc))
        if "Floor" in piece.name or "Foundation" in piece.name:
            fxs += [bb[0], bb[2]]
            fys += [bb[1], bb[3]]

    if not fxs:                                                 # fall back to every piece
        fxs = [c for p in pl for c in (p.bb[0], p.bb[2])]
        fys = [c for p in pl for c in (p.bb[1], p.bb[3])]
    rect = Rect(min(fxs), min(fys), max(fxs) - min(fxs), max(fys) - min(fys))
    roofs = sum(1 for p in pl if "Roof" in p.mesh)
    return Building(id=hid, kind="house", rect=rect, placements=pl,
                    notes=f"template {tpl['id']}, {len(pl)} pieces ({roofs} roof), "
                          f"yaw {base_yaw:.0f}")


def compose_house_synthesised(kit: Kit, rng: random.Random, ox: float, oy: float,
                              mods_w: int, mods_h: int, hid: str, yaw: float = 0.0) -> Building:
    """
    Compose a house by the rules in STYLE_GUIDE.md, which were read out of the hand-authored
    kitbashes on L_Tutorial_Island rather than invented from bounds.

      R1  the building has its own base yaw; every piece sits at base + {0,90,180,270}
      R2  storeys stack at the measured wall height (400 cm); 1-3 of them
      R3  the roof is the largest role in the piece list
      R4  roof pieces go down as Base+Top PAIRS at identical coordinates
      R5  the roof is a perimeter ring with corners, not a field of tiles
      R6  Extended variants are the default
      R7  a beam per roof segment
      R8  wall runs mix blank / window / door variants
      R9  every storey is floored
      R10 every storey is cornered, with varied corner pieces
    """
    mw, mh = kit.module
    w, h = mods_w * mw, mods_h * mh
    base_yaw = yaw if yaw else rng.uniform(0.0, 360.0)      # R1
    local: list[tuple] = []                                  # (piece, lx, ly, z, lyaw)

    def at(piece, lx, ly, z, lyaw):
        """Place by pivot, in the building's own frame."""
        local.append((piece, lx, ly, z, lyaw))

    def by_min(piece, min_x, min_y, z, lyaw):
        """Place so the piece's rotated footprint starts at (min_x, min_y)."""
        x0, y0, _, _ = rotated_extent(piece, lyaw)
        local.append((piece, min_x - x0, min_y - y0, z, lyaw))

    def pick(*roles):
        opts = [p for r in roles for p in kit.role(r)]
        return rng.choice(opts) if opts else None

    found = next((p for p in kit.role("foundation") if p.name.endswith("_5x4")),
                 kit.role("foundation")[0] if kit.role("foundation") else None)
    floor = next((p for p in kit.role("floor") if p.name.endswith("_5x4_01")),
                 kit.role("floor")[0])
    wall_any = kit.role("wall_window") + kit.role("wall_blank") + kit.role("wall_door")
    if not wall_any:
        wall_any = kit.role("wall_window") or kit.role("corner")
    storey_h = wall_any[0].size[2]                            # R2: the wall IS the storey
    storeys = rng.randint(1, 3)

    # --- foundation ring --------------------------------------------------------------
    z = 0.0
    if found:
        for i in range(mods_w):
            by_min(found, i * mw, 0.0, z, 0.0)
            by_min(found, i * mw, h - found.size[1], z, 0.0)
        for j in range(mods_h):
            by_min(found, 0.0, j * mh, z, 90.0)
            by_min(found, w - found.size[1], j * mh, z, 90.0)
        z += found.size[2]

    # --- storeys: floor + wall ring + corners (R9, R8, R10) ---------------------------
    door_module = rng.randrange(mods_w)
    for s in range(storeys):
        for i in range(mods_w):                               # R9
            for j in range(mods_h):
                by_min(floor, i * mw, j * mh, z, 0.0)
        wz = z + floor.size[2]
        for i in range(mods_w):                               # R8: vary the run
            a = pick("wall_window", "wall_blank") or wall_any[0]
            b = pick("wall_window", "wall_blank") or wall_any[0]
            if s == 0 and i == door_module and kit.role("wall_door"):
                a = rng.choice(kit.role("wall_door"))
            by_min(a, i * mw, 0.0, wz, 0.0)
            by_min(b, i * mw, h - b.size[1], wz, 0.0)
        for j in range(mods_h):
            a = pick("wall_window", "wall_blank") or wall_any[0]
            b = pick("wall_window", "wall_blank") or wall_any[0]
            by_min(a, 0.0, j * mh, wz, 90.0)
            by_min(b, w - b.size[1], j * mh, wz, 90.0)
        for (cx, cy, cyaw) in ((0.0, 0.0, 0.0), (w, 0.0, 90.0),
                               (w, h, 180.0), (0.0, h, 270.0)):   # R10
            c = pick("corner")
            if c:
                at(c, cx, cy, wz, cyaw)
        z = wz + storey_h

    # --- roof: a perimeter ring of Base+Top pairs (R4, R5, R6, R7) --------------------
    roof_z = z

    def pair(base_role, top_role, lx, ly, lyaw):
        """R4: a _Base and its _Top go down at the same point, same yaw."""
        b = next((p for p in kit.role(base_role) if "Extended" in p.name), None) \
            or (kit.role(base_role)[0] if kit.role(base_role) else None)
        t = next((p for p in kit.role(top_role) if "Extended" in p.name), None) \
            or (kit.role(top_role)[0] if kit.role(top_role) else None)
        for p in (b, t):
            if p:
                at(p, lx, ly, roof_z, lyaw)
        return b is not None

    tiles = kit.role("roof_tile")
    tile_base = next((p for p in tiles if p.name.endswith("_Tiling_Base")), None)
    tile_top = next((p for p in tiles if p.name.endswith("_Tiling_TopExtended")), None) \
        or next((p for p in tiles if p.name.endswith("_Tiling_Top")), None)
    beam = pick("roof_beam")
    seg = tile_base.size[1] if tile_base else mw              # run length per segment

    if tile_base and tile_top:
        edges = ((0.0, 0.0, 0.0, 1.0, h, 0.0), (w, 0.0, 0.0, 1.0, h, 90.0),
                 (w, h, -1.0, 0.0, w, 180.0), (0.0, h, 0.0, -1.0, h, 270.0))
        for (sx, sy, dx, dy, length, eyaw) in edges:
            n = max(1, int(round(length / seg)))
            for k in range(n):
                t = (k + 0.5) * (length / n)
                at(tile_base, sx + dx * t, sy + dy * t, roof_z, eyaw)
                at(tile_top, sx + dx * t, sy + dy * t, roof_z, eyaw)   # R4
                if beam:                                                # R7
                    at(beam, sx + dx * t, sy + dy * t, roof_z, eyaw)
        for (cx, cy, cyaw) in ((0.0, 0.0, 0.0), (w, 0.0, 90.0),
                               (w, h, 180.0), (0.0, h, 270.0)):         # R5 turns
            pair("roof_corner_outer", "roof_corner_inner", cx, cy, cyaw)
        for (ex, ey, eyaw) in ((w / 2.0, 0.0, 0.0), (w / 2.0, h, 180.0)):
            pair("roof_end", "roof_end", ex, ey, eyaw)

    # --- bake the local frame into world (R1) -----------------------------------------
    rad = math.radians(base_yaw)
    ca, sa = math.cos(rad), math.sin(rad)
    pl: list[Placement] = []
    xs, ys = [], []
    for (piece, lx, ly, lz, lyaw) in local:
        wx = ox + lx * ca - ly * sa
        wy = oy + lx * sa + ly * ca
        wyaw = (lyaw + base_yaw) % 360.0
        bb = world_bb(piece, wx, wy, wyaw)
        pl.append(Placement(piece.name, wx, wy, lz + piece.pivot_from_min[2], wyaw, bb))
        xs += [bb[0], bb[2]]
        ys += [bb[1], bb[3]]

    # The building's footprint is where its FLOOR is, not where its roof reaches. Extended
    # roof corners are 691 cm square and oversail the walls by design; including them made a
    # 500 x 1500 cottage claim a 2681 x 1823 plot and no seed could be packed. Real villages
    # put houses close enough for the eaves to overlap — that is what eaves are.
    fxs, fys = [], []
    for p in pl:
        if "Floor" in p.mesh or "Foundation" in p.mesh:
            fxs += [p.bb[0], p.bb[2]]
            fys += [p.bb[1], p.bb[3]]
    if not fxs:
        fxs, fys = xs, ys
    rect = Rect(min(fxs), min(fys), max(fxs) - min(fxs), max(fys) - min(fys)) if fxs else \
        Rect(ox, oy, w, h)
    roofs = sum(1 for p in pl if "Roof" in p.mesh)
    return Building(id=hid, kind="house", rect=rect, placements=pl,
                    notes=f"{mods_w}x{mods_h} modules, {storeys} storey(s), "
                          f"yaw {base_yaw:.0f}, {roofs} roof pieces")


def rotated_extent(piece, yaw: float) -> tuple[float, float, float, float]:
    """Local XY bounds of a piece after yaw, relative to its pivot."""
    px, py, _ = piece.pivot_from_min
    lx0, ly0 = -px, -py
    lx1, ly1 = lx0 + piece.size[0], ly0 + piece.size[1]
    rad = math.radians(yaw)
    c, s = math.cos(rad), math.sin(rad)
    xs, ys = [], []
    for (lx, ly) in ((lx0, ly0), (lx1, ly0), (lx1, ly1), (lx0, ly1)):
        xs.append(lx * c - ly * s)
        ys.append(lx * s + ly * c)
    return (min(xs), min(ys), max(xs), max(ys))


def world_bb(piece, wx: float, wy: float, yaw: float) -> tuple[float, float, float, float]:
    x0, y0, x1, y1 = rotated_extent(piece, yaw)
    return (wx + x0, wy + y0, wx + x1, wy + y1)


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
        # Rejects against objectives as well as buildings. Objectives are anchors — the
        # statue stands where the guards are thickest and cannot be shoved (2.8) — so a
        # house placed on top of one is a house the refiner has to move later, and with
        # three required objectives plus the optional fields there is no longer slack for
        # that. Generating the overlap and then repairing it is not cheaper than not
        # generating it: the same arithmetic, run once instead of three times.
        for _ in range(tries):
            a = rng.uniform(0, 2 * math.pi)
            r = rng.uniform(r_lo, r_hi)
            cx, cy = center[0] + r * math.cos(a), center[1] + r * math.sin(a)
            cand = Rect(cx - rect_w / 2, cy - rect_h / 2, rect_w, rect_h)
            if any(cand.overlaps(b.rect, 250.0) for b in plan.buildings):
                continue
            if any(cand.overlaps(o.rect, 250.0) for o in plan.objectives):
                continue
            return (cand.x, cand.y)
        return None

    # --- objectives: exactly three required, max two of a kind ------------------------
    # The roster is Market / Statue / Windmill (2.8, revised 2026-08-14). The granary this
    # generator used to roll was removed from the GDD entirely — it never had a mesh, a
    # Blueprint or a placed instance — and the wheat field was demoted to optional in the
    # same ruling. See PRE_BUILD_DECLARATION.md's addendum.
    kinds = ["market", "statue", "windmill"]
    if rng.random() < 0.35:                       # sometimes roll a duplicate, still legal
        kinds = rng.sample(REQUIRED_KINDS, 2) + [rng.choice(REQUIRED_KINDS)]
        rng.shuffle(kinds)

    def free_angle(used: list[float]) -> float:
        for _ in range(200):
            a = rng.uniform(0, 2 * math.pi)
            if all(abs(((a - u + math.pi) % (2 * math.pi)) - math.pi) > 0.9 for u in used):
                return a
        return rng.uniform(0, 2 * math.pi)   # crowded ring; take what we can get

    def anchor_spot(a: float, r_lo: float, r_hi: float,
                    size: tuple[float, float]) -> tuple[float, float]:
        """
        Objectives are anchors — placed first, never moved by the refiner (2.8). So they must
        not be generated on top of each other or on the well. Walk out along the bearing
        until the footprint is clear rather than trusting an angular gap: a 0.9 rad
        separation means nothing when one footprint is a 500cm plinth and the next is a
        windmill whose sails sweep 5354.
        """
        for _ in range(40):
            r = rng.uniform(r_lo, r_hi)
            for step in range(12):
                rr = r + step * 600.0
                cand = Rect(center[0] + rr * math.cos(a) - size[0] / 2,
                            center[1] + rr * math.sin(a) - size[1] / 2, size[0], size[1])
                clash = (any(cand.overlaps(b.rect, 250.0) for b in plan.buildings)
                         or any(cand.overlaps(o.rect, 250.0) for o in plan.objectives))
                if not clash:
                    return (cand.x, cand.y)
        rr = r_hi
        return (center[0] + rr * math.cos(a) - size[0] / 2,
                center[1] + rr * math.sin(a) - size[1] / 2)

    used_angles: list[float] = []
    for i, kind in enumerate(kinds):
        a = free_angle(used_angles)
        used_angles.append(a)
        if kind == "statue":
            # 2.8: the king's statue stands in the village square, "sitting where the guards
            # are thickest". Same central band the granary used to occupy, for the same
            # reason — it is an anchor the refiner may not shove.
            r_lo, r_hi = 900.0, 1800.0
            size = (mw, mh)                       # a plinth, not a building
        elif kind == "market":
            # 2.8: the market is the other objective in the village core — "two of your three
            # targets in the one place you least want to linger".
            #
            # The market IS its stalls ("burn the stalls and you burn the hamlet's
            # livelihood"), so the footprint is measured from the stall cluster rather than
            # chosen, the same way the windmill takes its footprint from the measured prefab.
            # This also mirrors the runtime class: GSMarketObjective carries a Stalls array
            # and auto-adopts the cluster around it.
            r_lo, r_hi = 1200.0, 2400.0
            n_stalls = rng.randint(2, 4)
            if kit.role("market"):
                sw, sh = kit.one("market").footprint
                span = n_stalls * sw + (n_stalls - 1) * STALL_GAP
                size = (span + STALL_GAP * 2, sh + STALL_GAP * 2)
            else:
                n_stalls = 0
                size = (mw * 2, mh * 2)
        else:
            r_lo, r_hi = 3400.0, 5200.0           # the windmill is the landmark
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
        ox, oy = anchor_spot(a, r_lo, r_hi, size)
        plan.objectives.append(Objective(id=f"obj_{i}_{kind}", kind=kind,
                                         rect=Rect(ox, oy, size[0], size[1]),
                                         required=True,
                                         destruction=DESTRUCTION[kind]))
        if kind == "windmill":
            wm = place_prefab(kit, "windmill", ox + size[0] / 2, oy + size[1] / 2,
                              f"windmill_{i}", "windmill")
            if wm:
                wm.rect = plan.objectives[-1].rect   # the objective owns the footprint
                wm.objective_id = plan.objectives[-1].id
                plan.buildings.append(wm)
        elif kind == "market" and n_stalls:
            # Stalls sit INSIDE the objective footprint and carry its objective_id, so the
            # overlap check does not report the market colliding with itself — the same
            # linkage the windmill needed.
            sw, sh = kit.one("market").footprint
            for s in range(n_stalls):
                sx = ox + STALL_GAP + s * (sw + STALL_GAP) + sw / 2
                sy = oy + size[1] / 2
                st = place_prefab(kit, "market", sx, sy, f"stall_{i}_{s}", "market")
                if st:
                    st.objective_id = plan.objectives[-1].id
                    plan.buildings.append(st)

    # --- optional objectives: wheat fields ---------------------------------------------
    # 2.8: fields and houses are "worth points but not gating extraction". They sprawl at
    # the hamlet's edges, and the cover guarantee applies to them too — 2.8 says broken
    # sightlines between the treeline and EVERY objective, not every required one.
    for i in range(rng.randint(1, 2)):
        a = free_angle(used_angles)
        used_angles.append(a)
        size = (mw * 6, mh * 6)
        ox, oy = anchor_spot(a, 4200.0, 6200.0, size)   # fields sprawl at the edges
        plan.objectives.append(Objective(id=f"opt_{i}_field", kind="field",
                                         rect=Rect(ox, oy, size[0], size[1]),
                                         required=False,
                                         destruction=DESTRUCTION["field"]))

    n_houses = rng.randint(5, 8)
    # The house ring starts OUTSIDE the core objectives, derived from where they actually
    # landed rather than typed. The statue and the market stand in the village square (2.8);
    # a ring that starts inside them wraps houses around the square and walls the statue in,
    # which the reachability check correctly calls an unwinnable raid. Measuring the ring off
    # the placed anchors is the same discipline kit.json applies to mesh footprints.
    core_reach = 0.0
    for o in plan.objectives:
        if o.kind in ("statue", "market"):
            for cx, cy in ((o.rect.x, o.rect.y), (o.rect.x2, o.rect.y),
                           (o.rect.x, o.rect.y2), (o.rect.x2, o.rect.y2)):
                core_reach = max(core_reach, math.dist((cx, cy), center))
    ring_lo = max(mw * 3.0, core_reach + BUILDING_CLEARANCE)
    house_ring = (ring_lo, ring_lo + mw * 4.0)
    for i in range(n_houses):
        # ONE module wide, deliberately. The measured roof set spans 766 cm from eave to
        # eave, which roofs a 500 cm module with proper overhang and cannot reach across
        # 1000. Houses vary along the ridge instead, which is also how the kit's Half and
        # End pieces are authored. A constraint the kit imposed, not a preference.
        mods_w, mods_h = 1, rng.randint(1, 3)
        # Compose at the origin FIRST to learn the template's real footprint, then find a
        # spot that size and translate. Reserving mods_w x mods_h modules booked 500x1500
        # for a house that is actually 1380x1818, and every seed collided.
        probe = compose_house(kit, rng, 0.0, 0.0, mods_w, mods_h, f"house_{i}")
        spot = free_spot(probe.rect.w, probe.rect.h, *house_ring)
        if spot is None:
            continue                     # the ring is full; fewer houses is not a failure
        dx = spot[0] - probe.rect.x
        dy = spot[1] - probe.rect.y
        probe.rect = Rect(probe.rect.x + dx, probe.rect.y + dy, probe.rect.w, probe.rect.h)
        for pc in probe.placements:
            pc.x += dx
            pc.y += dy
            pc.bb = (pc.bb[0] + dx, pc.bb[1] + dy, pc.bb[2] + dx, pc.bb[3] + dy)
        plan.buildings.append(probe)

    # Market stalls are no longer scattered here — they belong to the market objective and
    # are placed inside its footprint above (2.8: the market IS the stalls).

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
