"""Treeline generator - regenerates the forest edge along the GS_ForestWall_* blockers.

The boundary of the playable area is a chain of GS_ForestWall_* boxes. The treeline is the
band of trees hugging their OUTER face; it is what tells the player "the world stops here"
before they walk into an invisible wall. Move the blockers and the treeline has to be
regenerated, which is what this module is for.

Access it from the editor:

    Tools -> Goblin Siege -> Treeline

or from the Python console (Output Log, Cmd dropdown set to Python):

    import gs_treeline
    gs_treeline.survey()        # report only - changes nothing
    gs_treeline.preview()       # drop markers where trees WOULD go
    gs_treeline.regenerate()    # clear the band and re-scatter
    gs_treeline.restore()       # undo the last regenerate

Every regenerate writes a full backup of the affected instances to Saved/GSTreeline first,
so restore() is always available.

THE ONE TRAP, AND IT COST A SESSION (#322): a downward line trace is NOT a ground test.
Foliage has collision, so a trace from a tree's base hits that tree's own trunk and reports
"grounded, gap 0" - 566 of 572 birches resolved that way and were never tested at all. The
map also carries fog cards, a water plane and the blockers themselves, and all of them read
as ground. _ground_ignore() exists to exclude the lot; do not trace without it.
"""
import json
import os
import time

import unreal

# --- what the treeline is made of -------------------------------------------------------
# Apple is deliberately absent: it is an orchard tree that belongs to the village, not to
# the forest edge, and regenerating the band must not touch it.
SPECIES = {
    "SM_VillageBirch_01": 0.27,
    "SM_VillageBirch_02": 0.27,
    "SM_VillageBirch_03": 0.26,
    "SM_VillageOak": 0.20,
}
KEEP_UNTOUCHED = ("SM_Tree_Apple_01",)

# --- band shape, in unreal units --------------------------------------------------------
BAND_START = 200.0      # gap between the blocker face and the first trunk
BAND_DEPTH = 3500.0     # how deep the forest edge runs outward
CLEAR_INSIDE = 600.0    # also clear this far INSIDE the line, so nothing climbs the wall
SPACING = 500.0         # minimum trunk-to-trunk distance
MIN_SLOPE_Z = 0.55      # reject ground steeper than ~57 degrees
SEED = 1

BACKUP_DIR = os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "GSTreeline")


# ============================================================ world helpers

def _actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def _foliage_actor(actors=None):
    for a in (actors or _actors()):
        if a.get_class().get_name() == "InstancedFoliageActor":
            return a
    return None


def _ground_ignore(actors=None):
    """Everything a downward trace must NOT accept as ground. See the module docstring."""
    actors = actors or _actors()
    out = []
    for a in actors:
        lab = a.get_actor_label()
        cls = a.get_class().get_name()
        if (cls == "InstancedFoliageActor"        # a tree's own trunk
                or "ForestWall" in lab             # the blockers themselves
                or lab.startswith("Plane")         # fog cards AND the water plane
                or cls == "BP_RiverSpline_C"):     # river surfaces
            out.append(a)
    return out


NO_TREES_PREFIX = "GS_NoTrees"


def _no_tree_zones(actors=None):
    """Boxes the level artist has placed to say 'never plant here'. See mark()."""
    out = []
    for a in (actors or _actors()):
        if a.get_actor_label().startswith(NO_TREES_PREFIX):
            o, e = a.get_actor_bounds(False)
            out.append((o, e))
    return out


def _road_footprints(actors=None, margin=250.0):
    """The roads are DECALS, and a decal has no collision.

    191 dirt decals are projected onto the landscape here, so a downward trace passes straight
    through one and reports clean landscape - which is how trees ended up standing in a road.
    Their bounds are the only thing that can keep planting off them.
    """
    out = []
    for a in (actors or _actors()):
        if a.get_class().get_name() != "DecalActor":
            continue
        dc = a.get_component_by_class(unreal.DecalComponent)
        mat = dc.get_decal_material() if dc else None
        nm = (mat.get_name() if mat else "").lower()
        if "dirt" not in nm and "road" not in nm and "path" not in nm:
            continue
        o, e = a.get_actor_bounds(False)
        out.append((o, unreal.Vector(e.x + margin, e.y + margin, e.z)))
    return out


ROAD_CLEARANCE = 700.0


def _road_spline_keepout(step=350.0, clearance=ROAD_CLEARANCE):
    """Keep trees off the drawn roads (GS_Road_* splines - see gs_roads.py).

    A painted road is invisible to every trace, so the spline a human drew is the ONLY thing that
    knows where the road is. Returns box keep-outs strung along each road.
    """
    try:
        import gs_roads
    except Exception:
        return []
    half = unreal.Vector(clearance, clearance, 100000.0)
    return [(unreal.Vector(x, y, z), half) for (x, y, z) in gs_roads.sample(step)]


def clear_roads(clearance=ROAD_CLEARANCE, save=True):
    """Delete forest trees standing on a drawn road."""
    zones = _road_spline_keepout(clearance=clearance)
    if not zones:
        unreal.log_warning("[treeline] no GS_Road_* splines - draw one with gs_roads.add_road()")
        return 0
    ifa = _foliage_actor()
    removed = 0
    for c in ifa.get_components_by_class(unreal.InstancedStaticMeshComponent):
        m = c.static_mesh
        if not m or m.get_name() not in SPECIES:
            continue
        kill = [i for i in range(c.get_instance_count())
                if _blocked(c.get_instance_transform(i, True).translation.x,
                            c.get_instance_transform(i, True).translation.y, zones)]
        if kill:
            c.remove_instances(kill)
            removed += len(kill)
    unreal.log("[treeline] removed %d trees within %.0fuu of a road" % (removed, clearance))
    if save:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return removed


def _blocked(x, y, zones):
    for (o, e) in zones:
        if abs(x - o.x) <= e.x and abs(y - o.y) <= e.y:
            return True
    return False


def mark(size=2500.0):
    """Drop a 'no trees here' box in front of the viewport camera.

    Move and scale it like any actor, then run apply_marks(). The box is hidden in game and
    has no collision; it exists only to tell the generator to keep out. Because it stays in
    the level, later regenerate() runs keep honouring it - a mark is permanent, not one-shot.
    """
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    loc, rot = ues.get_level_viewport_camera_info()
    fwd = unreal.MathLibrary.get_forward_vector(rot)
    target = unreal.Vector(loc.x + fwd.x * 4000.0, loc.y + fwd.y * 4000.0, loc.z + fwd.z * 4000.0)
    hit = unreal.SystemLibrary.line_trace_single(
        _world(), target, unreal.Vector(target.x, target.y, target.z - 60000.0),
        unreal.TraceTypeQuery.ECC_VISIBILITY, True, _ground_ignore(),
        unreal.DrawDebugTrace.NONE, True)
    info = hit.to_dict() if hit else None
    if info and info.get("blocking_hit"):
        target = info["impact_point"]

    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    n = len(_no_tree_zones())
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, target, unreal.Rotator(0, 0, 0))
    a.set_actor_label("%s_%d" % (NO_TREES_PREFIX, n))
    smc = a.static_mesh_component
    smc.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    smc.set_world_scale3d(unreal.Vector(size / 100.0, size / 100.0, 6.0))
    smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    smc.set_hidden_in_game(True)
    unreal.log("[treeline] placed %s - move/scale it, then run apply_marks()" % a.get_actor_label())
    eas.set_selected_level_actors([a])
    return a


def apply_marks(save=True):
    """Delete forest trees inside every GS_NoTrees_* box. The boxes stay."""
    actors = _actors()
    zones = _no_tree_zones(actors)
    if not zones:
        unreal.log_warning("[treeline] no %s_* boxes in the level - place one with mark()"
                           % NO_TREES_PREFIX)
        return 0
    ifa = _foliage_actor(actors)
    removed = 0
    for c in ifa.get_components_by_class(unreal.InstancedStaticMeshComponent):
        m = c.static_mesh
        if not m or m.get_name() not in SPECIES:
            continue
        kill = []
        for i in range(c.get_instance_count()):
            p = c.get_instance_transform(i, True).translation
            if _blocked(p.x, p.y, zones):
                kill.append(i)
        if kill:
            c.remove_instances(kill)
            removed += len(kill)
    unreal.log("[treeline] apply_marks: removed %d trees inside %d zone(s)" % (removed, len(zones)))
    if save:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return removed


def _mesh_paths():
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    out = {}
    for a in ar.get_assets_by_class(
            unreal.TopLevelAssetPath("/Script/Engine", "StaticMesh"), True):
        p = str(a.package_name)
        out[p.split("/")[-1]] = p
    return out


def _blockers(actors=None, selected_only=False):
    """The GS_ForestWall_* chain in each wall's OWN frame.

    Returns (label, centre, forward, right, half_long, half_out) with forward along the wall
    run and right along its thickness, both unit vectors in world space.

    THE TRAP THIS REPLACES: the first version used get_actor_bounds(), which is an AXIS-ALIGNED
    box. GS_ForestWall_6 and _7 are yawed -64.6 and -122.7 degrees, so their AABBs are huge
    diagonal squares whose "long axis" is meaningless - the scatter ran along world X/Y instead
    of along the wall, and no trees appeared where the wall actually is.
    """
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    pool = eas.get_selected_level_actors() if selected_only else (actors or _actors())
    out = []
    for a in pool:
        if "ForestWall" not in a.get_actor_label():
            continue
        smc = a.get_component_by_class(unreal.StaticMeshComponent)
        if not smc or not smc.static_mesh:
            continue
        ext = smc.static_mesh.get_bounds().box_extent      # unscaled local half-size
        sc = a.get_actor_scale3d()
        hx, hy = ext.x * sc.x, ext.y * sc.y
        rot = a.get_actor_rotation()
        fwd = unreal.MathLibrary.get_forward_vector(rot)
        rgt = unreal.MathLibrary.get_right_vector(rot)
        if hx >= hy:
            out.append((a.get_actor_label(), a.get_actor_location(), fwd, rgt, hx, hy))
        else:
            # thickness runs along local X instead - swap so 'forward' is always the long run
            out.append((a.get_actor_label(), a.get_actor_location(), rgt, fwd, hy, hx))
    return sorted(out, key=lambda t: t[0])


def _play_centre(actors=None):
    """Centroid of the building objectives - i.e. the village, i.e. the inside."""
    actors = actors or _actors()
    pts = [a.get_actor_location() for a in actors
           if a.get_class().get_name() == "GSBuildingObjective"]
    if not pts:
        pts = [a.get_actor_location() for a in actors if "ForestWall" not in a.get_actor_label()]
    return (sum(p.x for p in pts) / len(pts), sum(p.y for p in pts) / len(pts))


def _out_sign(centre, right, play):
    """Which way along the wall's 'right' points AWAY from the village."""
    dx, dy = centre.x - play[0], centre.y - play[1]
    return 1.0 if (right.x * dx + right.y * dy) >= 0 else -1.0


def _signed_band_offset(px, py, blocker, play):
    """How far (px,py) sits outside a blocker's face, or None if past either end of it.

    Negative means inside the play area. This is the test that decides what gets cleared.
    """
    _lab, centre, fwd, rgt, half_long, half_out = blocker
    dx, dy = px - centre.x, py - centre.y
    along = fwd.x * dx + fwd.y * dy
    if abs(along) > half_long:
        return None
    sgn = _out_sign(centre, rgt, play)
    return (rgt.x * dx + rgt.y * dy) * sgn - half_out


# ============================================================ measurement

def survey(verbose=True):
    """Report the treeline's state. Changes nothing."""
    actors = _actors()
    world = _world()
    ifa = _foliage_actor(actors)
    ignore = _ground_ignore(actors)
    boxes = _blockers(actors)
    play = _play_centre(actors)

    total = 0
    in_band = 0
    floating = 0
    worst = 0.0
    per_species = {}
    for c in ifa.get_components_by_class(unreal.InstancedStaticMeshComponent):
        m = c.static_mesh
        if not m or m.get_name() not in SPECIES:
            continue
        per_species[m.get_name()] = c.get_instance_count()
        for i in range(c.get_instance_count()):
            p = c.get_instance_transform(i, True).translation
            total += 1
            for blocker in boxes:
                d = _signed_band_offset(p.x, p.y, blocker, play)
                if d is not None and -CLEAR_INSIDE <= d <= BAND_START + BAND_DEPTH:
                    in_band += 1
                    break
            hit = unreal.SystemLibrary.line_trace_single(
                world, unreal.Vector(p.x, p.y, p.z + 200), unreal.Vector(p.x, p.y, p.z - 40000),
                unreal.TraceTypeQuery.ECC_VISIBILITY, True, ignore,
                unreal.DrawDebugTrace.NONE, True)
            info = hit.to_dict() if hit else None
            if info and info.get("blocking_hit"):
                gap = p.z - info["impact_point"].z
                if gap > 150.0:
                    floating += 1
                    worst = max(worst, gap)

    if verbose:
        unreal.log("[treeline] blockers %d | forest trees %d | in the band %d"
                   % (len(boxes), total, in_band))
        for k in sorted(per_species):
            unreal.log("[treeline]    %-20s %d" % (k, per_species[k]))
        unreal.log("[treeline] floating above real ground: %d (worst %.0f uu)" % (floating, worst))
    return {"blockers": len(boxes), "trees": total, "in_band": in_band,
            "floating": floating, "worst": worst, "per_species": per_species}


# ============================================================ generation

def _scatter_points(boxes, play, ignore, world, spacing=SPACING,
                    band_start=BAND_START, band_depth=BAND_DEPTH, seed=SEED):
    """Candidate trunk positions along the outside of the blocker chain, on real landscape."""
    import random
    rng = random.Random(seed)
    cell = spacing
    grid = {}

    def too_close(x, y):
        gx, gy = int(x // cell), int(y // cell)
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for (ox, oy) in grid.get((gx + dx, gy + dy), ()):
                    if (ox - x) ** 2 + (oy - y) ** 2 < spacing * spacing:
                        return True
        return False

    def remember(x, y):
        grid.setdefault((int(x // cell), int(y // cell)), []).append((x, y))

    _acts = _actors()
    keepout = _no_tree_zones(_acts) + _road_footprints(_acts) + _road_spline_keepout()
    placed = []
    rejected = {"spacing": 0, "not_landscape": 0, "steep": 0, "no_hit": 0, "keepout": 0}
    for blocker in boxes:
        _lab, centre, fwd, rgt, half_long, half_out = blocker
        sgn = _out_sign(centre, rgt, play)
        area = (2.0 * half_long) * band_depth
        target = int(area / (spacing * spacing) * 1.6)      # oversample; rejection thins it
        for _ in range(target):
            t = rng.uniform(-half_long, half_long)
            d = rng.uniform(band_start, band_start + band_depth)
            off = sgn * (half_out + d)
            x = centre.x + fwd.x * t + rgt.x * off
            y = centre.y + fwd.y * t + rgt.y * off
            if too_close(x, y):
                rejected["spacing"] += 1
                continue
            if _blocked(x, y, keepout):
                rejected["keepout"] += 1     # a road decal, or a GS_NoTrees_* box
                continue
            hit = unreal.SystemLibrary.line_trace_single(
                world, unreal.Vector(x, y, centre.z + 9000), unreal.Vector(x, y, centre.z - 25000),
                unreal.TraceTypeQuery.ECC_VISIBILITY, True, ignore,
                unreal.DrawDebugTrace.NONE, True)
            info = hit.to_dict() if hit else None
            if not info or not info.get("blocking_hit"):
                rejected["no_hit"] += 1
                continue
            ha = info.get("hit_actor")
            if not ha or ha.get_class().get_name() != "Landscape":
                rejected["not_landscape"] += 1     # road, bridge, roof, river, water
                continue
            if info["impact_normal"].z < MIN_SLOPE_Z:
                rejected["steep"] += 1
                continue
            ip = info["impact_point"]
            placed.append((ip.x, ip.y, ip.z))
            remember(x, y)
    return placed, rejected


def _assign_species(points, seed=SEED):
    import random
    rng = random.Random(seed + 7)
    names = sorted(SPECIES)
    weights = [SPECIES[n] for n in names]
    out = dict((n, []) for n in names)
    for (x, y, z) in points:
        out[rng.choices(names, weights=weights, k=1)[0]].append(unreal.Vector(x, y, z))
    return out


def preview(spacing=SPACING, band_start=BAND_START, band_depth=BAND_DEPTH, seed=SEED,
            max_markers=400):
    """Drop cheap markers where trees WOULD go, so the shape can be judged before committing."""
    actors = _actors()
    world = _world()
    boxes = _blockers(actors)
    play = _play_centre(actors)
    ignore = _ground_ignore(actors)
    pts, rej = _scatter_points(boxes, play, ignore, world, spacing, band_start, band_depth, seed)

    clear_preview()
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    mesh = unreal.load_asset("/Engine/BasicShapes/Cylinder")
    step = max(1, len(pts) // max_markers)
    n = 0
    for (x, y, z) in pts[::step]:
        a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z + 400),
                                       unreal.Rotator(0, 0, 0))
        a.set_actor_label("GS_TREELINE_PREVIEW_%d" % n)
        smc = a.static_mesh_component
        smc.set_static_mesh(mesh)
        smc.set_world_scale3d(unreal.Vector(1.2, 1.2, 8))
        smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        n += 1
    unreal.log("[treeline] preview: %d candidate trees, showing %d markers" % (len(pts), n))
    unreal.log("[treeline] rejected: %s" % rej)
    return {"candidates": len(pts), "markers": n, "rejected": rej}


def clear_preview():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    n = 0
    for a in _actors():
        if a.get_actor_label().startswith("GS_TREELINE_PREVIEW_"):
            eas.destroy_actor(a)
            n += 1
    if n:
        unreal.log("[treeline] cleared %d preview markers" % n)
    return n


def clear_path(points, radius=2200.0, save=False):
    """Delete forest trees within `radius` of a polyline - i.e. an ABANDONED treeline.

    When the blockers move, the trees that marked the old boundary are left stranded inside
    the map with nothing to explain them. There is no way to find them geometrically (they
    look like any other planting), so the line is supplied by hand.
    """
    ifa = _foliage_actor()
    segs = [(points[i], points[i + 1]) for i in range(len(points) - 1)]

    def near(x, y):
        for ((ax, ay), (bx, by)) in segs:
            vx, vy = bx - ax, by - ay
            L2 = vx * vx + vy * vy
            t = 0.0 if L2 == 0 else max(0.0, min(1.0, ((x - ax) * vx + (y - ay) * vy) / L2))
            cx, cy = ax + t * vx, ay + t * vy
            if (x - cx) ** 2 + (y - cy) ** 2 <= radius * radius:
                return True
        return False

    removed = 0
    for c in ifa.get_components_by_class(unreal.InstancedStaticMeshComponent):
        m = c.static_mesh
        if not m or m.get_name() not in SPECIES:
            continue
        kill = [i for i in range(c.get_instance_count())
                if near(c.get_instance_transform(i, True).translation.x,
                        c.get_instance_transform(i, True).translation.y)]
        if kill:
            c.remove_instances(kill)
            removed += len(kill)
    unreal.log("[treeline] old treeline: removed %d trees within %.0fuu of the path" % (removed, radius))
    if save:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return removed


def clear_water_trees(save=False):
    """Delete forest trees standing on the water plane. They are old-treeline strays."""
    world = _world()
    actors = _actors()
    ifa = _foliage_actor(actors)
    ignore = [a for a in actors
              if a.get_class().get_name() == "InstancedFoliageActor" or "ForestWall" in a.get_actor_label()]
    removed = 0
    for c in ifa.get_components_by_class(unreal.InstancedStaticMeshComponent):
        m = c.static_mesh
        if not m or m.get_name() not in SPECIES:
            continue
        kill = []
        for i in range(c.get_instance_count()):
            p = c.get_instance_transform(i, True).translation
            hit = unreal.SystemLibrary.line_trace_single(
                world, unreal.Vector(p.x, p.y, p.z + 200), unreal.Vector(p.x, p.y, p.z - 40000),
                unreal.TraceTypeQuery.ECC_VISIBILITY, True, ignore,
                unreal.DrawDebugTrace.NONE, True)
            info = hit.to_dict() if hit else None
            if not info or not info.get("blocking_hit"):
                continue
            ha = info.get("hit_actor")
            if ha and ha.get_actor_label().startswith("Plane"):
                kill.append(i)
        if kill:
            c.remove_instances(kill)
            removed += len(kill)
    unreal.log("[treeline] removed %d trees standing on the water plane" % removed)
    if save:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return removed


# ============================================================ backup / restore

def _backup_path(stamp=None):
    if not os.path.isdir(BACKUP_DIR):
        os.makedirs(BACKUP_DIR)
    stamp = stamp or time.strftime("%Y%m%d-%H%M%S")
    return os.path.join(BACKUP_DIR, "treeline-%s.json" % stamp)


def _dump_all_species():
    ifa = _foliage_actor()
    data = {}
    for c in ifa.get_components_by_class(unreal.InstancedStaticMeshComponent):
        m = c.static_mesh
        if not m or m.get_name() not in SPECIES:
            continue
        rows = []
        for i in range(c.get_instance_count()):
            t = c.get_instance_transform(i, True)
            tr = t.translation
            sc = t.scale3d
            rows.append([tr.x, tr.y, tr.z, sc.x, sc.y, sc.z])
        data[m.get_name()] = rows
    return data


def restore(path=None):
    """Put the forest species back exactly as the last regenerate found them."""
    if path is None:
        files = sorted(os.listdir(BACKUP_DIR)) if os.path.isdir(BACKUP_DIR) else []
        files = [f for f in files if f.endswith(".json")]
        if not files:
            unreal.log_warning("[treeline] no backup to restore from")
            return None
        path = os.path.join(BACKUP_DIR, files[-1])
    data = json.load(open(path))
    ifa = _foliage_actor()
    paths = _mesh_paths()
    for c in ifa.get_components_by_class(unreal.InstancedStaticMeshComponent):
        m = c.static_mesh
        if m and m.get_name() in data:
            c.clear_instances()
    for nm, rows in data.items():
        locs = [unreal.Vector(r[0], r[1], r[2]) for r in rows]
        if locs:
            unreal.FoliageService.add_foliage_instances(
                paths[nm], locs, min_scale=1.0, max_scale=1.0,
                align_to_normal=False, random_yaw=True, trace_to_surface=False)
    unreal.log("[treeline] restored from %s" % path)
    return path


# ============================================================ the command

def regenerate(spacing=SPACING, band_start=BAND_START, band_depth=BAND_DEPTH,
               clear_inside=CLEAR_INSIDE, seed=SEED, save=True):
    """Clear the treeline band and re-scatter it along the current blocker chain."""
    actors = _actors()
    world = _world()
    ifa = _foliage_actor(actors)
    boxes = _blockers(actors)
    play = _play_centre(actors)
    ignore = _ground_ignore(actors)
    paths = _mesh_paths()
    if not boxes:
        unreal.log_warning("[treeline] no GS_ForestWall_* blockers found - nothing to follow")
        return None

    backup = _backup_path()
    json.dump(_dump_all_species(), open(backup, "w"))
    unreal.log("[treeline] backup written: %s" % backup)

    # 1. clear the band, keeping every tree outside it
    removed = 0
    for c in ifa.get_components_by_class(unreal.InstancedStaticMeshComponent):
        m = c.static_mesh
        if not m or m.get_name() not in SPECIES:
            continue
        kill = []
        for i in range(c.get_instance_count()):
            p = c.get_instance_transform(i, True).translation
            for blocker in boxes:
                d = _signed_band_offset(p.x, p.y, blocker, play)
                if d is not None and -clear_inside <= d <= band_start + band_depth:
                    kill.append(i)
                    break
        if kill:
            c.remove_instances(kill)     # bulk; per-instance removal takes ~0.5s each
            removed += len(kill)

    # 2. re-scatter
    pts, rej = _scatter_points(boxes, play, ignore, world, spacing, band_start, band_depth, seed)
    by_species = _assign_species(pts, seed)
    added = 0
    for nm, locs in by_species.items():
        if not locs:
            continue
        # trace_to_surface is OFF on purpose: it re-snapped trees onto the water plane in
        # #322, because Plane2 still blocks queries. These points are already on landscape.
        unreal.FoliageService.add_foliage_instances(
            paths[nm], locs, min_scale=0.85, max_scale=1.25,
            align_to_normal=False, random_yaw=True, trace_to_surface=False)
        added += len(locs)

    unreal.log("[treeline] cleared %d, planted %d along %d blockers" % (removed, added, len(boxes)))
    unreal.log("[treeline] rejected: %s" % rej)
    if save:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        unreal.log("[treeline] level saved")
    return {"removed": removed, "added": added, "backup": backup, "rejected": rej}


# ============================================================ editor menu

def register_menu():
    """Tools -> Goblin Siege -> Treeline. Called from init_unreal.py at startup."""
    menus = unreal.ToolMenus.get()
    tools = menus.find_menu("LevelEditor.MainMenu.Tools")
    if not tools:
        return False
    sub = tools.add_sub_menu("LevelEditor.MainMenu.Tools", "GoblinSiege",
                             "GSTreeline", "Treeline")
    entries = (
        ("Survey (report only)", "survey", "Count trees, band members and floaters."),
        ("Preview band", "preview", "Drop markers where trees would go."),
        ("Clear preview markers", "clear_preview", "Remove the preview markers."),
        ("Mark a no-trees box here", "mark", "Drop a keep-out box at the camera. Move/scale it."),
        ("Apply marks (delete inside)", "apply_marks", "Delete trees inside every GS_NoTrees_* box."),
        ("REGENERATE treeline", "regenerate", "Clear the band and re-scatter. Backs up first."),
        ("Restore last backup", "restore", "Undo the last regenerate."),
    )
    for label, fn, tip in entries:
        e = unreal.ToolMenuEntry(name="GSTreeline_%s" % fn,
                                 type=unreal.MultiBlockType.MENU_ENTRY)
        e.set_label(label)
        e.set_tool_tip(tip)
        e.set_string_command(unreal.ToolMenuStringCommandType.PYTHON, "",
                             string="import gs_treeline; gs_treeline.%s()" % fn)
        sub.add_menu_entry("Treeline", e)
    menus.refresh_all_widgets()
    return True


if __name__ == "__main__":
    survey()
