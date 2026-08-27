"""Road splines - the road network as data, because nothing else on this map is.

WHY THIS EXISTS. The roads on L_Tutorial_Island are painted into the landscape material, so they
have no geometry, no collision and no position any code can query. Two things were tried first and
both failed, which is worth recording so nobody tries them again:

  * The 339 MI_Decal_Dirt decals are NOT a road. Median nearest-neighbour spacing is 67uu (they are
    stacked clumps of mud dressing), they form 23 disconnected components even when anything within
    1500uu is linked, and the nearest one to the village gate road is 9,734uu away.
  * The landscape paint layers do not separate road from field. This was tested properly, against
    ground truth, once Michael had drawn five roads by hand: sampling ON his road versus 1500uu to
    the side of the SAME segment gave, for GS_Road_4, AutoLandscape=1.00 in both places -
    IDENTICAL. GS_Road_2 gave Layer_04 0.824 on-road against 1.000 off-road, a difference far too
    small and inconsistent to threshold. There is no road layer to extract, so a road mask cannot
    be built and the network cannot be generated. Someone has to draw every road.
    (An earlier note here claimed get_layer_weights_at_location costs ~5 seconds per call and that
    this made sampling impossible. That was FIRST-CALL WARMUP - four calls later took 202ms total.
    Sampling is cheap; the data is simply not there.)

So a human draws the road once, and everything else reads the spline.

THE ACTOR IS ACF's, DELIBERATELY. GS_Road_* are ACF_SplinePath_BP instances, because ACF already
ships AI spline following - ACFFollowSplineCommandBP and ACF_NPCFollowSpline_BP - so a patrol or a
reinforcement column can follow a road we drew without any new C++. Do not replace this with a
custom road actor; that was the whole point of picking it.

    import gs_roads
    gs_roads.add_road()        # drop a new 4-point spline at the camera, then drag it onto the road
    gs_roads.snap()            # pull every road point down onto the ground (do this after drawing)
    gs_roads.report()          # what roads exist, how long, how many points
    gs_roads.sample(400.0)     # -> [(x, y, z), ...] points along every road
"""
import unreal

ROAD_PREFIX = "GS_Road_"
ROAD_BP = "/AscentCombatFramework/Integrations/Actors/ACF_SplinePath_BP"


def _actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def _ignore(actors=None):
    """Surfaces a ground trace must refuse - see gs_treeline for the full story."""
    out = []
    for a in (actors or _actors()):
        lab = a.get_actor_label()
        cls = a.get_class().get_name()
        if (cls == "InstancedFoliageActor" or "ForestWall" in lab
                or lab.startswith("Plane") or cls == "BP_RiverSpline_C"
                or lab.startswith(ROAD_PREFIX)):
            out.append(a)
    return out


def roads(actors=None):
    """Every GS_Road_* actor with a spline, sorted by label."""
    out = []
    for a in (actors or _actors()):
        if not a.get_actor_label().startswith(ROAD_PREFIX):
            continue
        sc = a.get_component_by_class(unreal.SplineComponent)
        if sc:
            out.append((a, sc))
    return sorted(out, key=lambda t: t[0].get_actor_label())


def _ground(x, y, z0, ignore):
    hit = unreal.SystemLibrary.line_trace_single(
        _world(), unreal.Vector(x, y, z0 + 6000), unreal.Vector(x, y, z0 - 20000),
        unreal.TraceTypeQuery.ECC_VISIBILITY, True, ignore, unreal.DrawDebugTrace.NONE, True)
    info = hit.to_dict() if hit else None
    if info and info.get("blocking_hit"):
        return info["impact_point"].z
    return None


def add_road(length=16000.0, points=4):
    """Drop a new road spline in front of the camera, ground-snapped, ready to drag."""
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    loc, rot = ues.get_level_viewport_camera_info()
    fwd = unreal.MathLibrary.get_forward_vector(rot)
    cx, cy = loc.x + fwd.x * 5000.0, loc.y + fwd.y * 5000.0
    ignore = _ignore()
    base = _ground(cx, cy, loc.z, ignore)
    if base is None:
        base = 0.0

    bp = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).load_asset(ROAD_BP)
    a = eas.spawn_actor_from_object(bp, unreal.Vector(cx, cy, base), unreal.Rotator(0, 0, 0))
    a.set_actor_label("%s%d" % (ROAD_PREFIX, len(roads())))
    sc = a.get_component_by_class(unreal.SplineComponent)
    sc.clear_spline_points(False)
    # lay the points along the camera's facing, so the spline starts pointing where you are looking
    for i in range(points):
        t = (i / float(points - 1) - 0.5) * length
        x, y = cx + fwd.x * t, cy + fwd.y * t
        z = _ground(x, y, base, ignore)
        sc.add_spline_point(unreal.Vector(x, y, (base if z is None else z) + 20.0),
                            unreal.SplineCoordinateSpace.WORLD, False)
    sc.update_spline()
    eas.set_selected_level_actors([a])
    unreal.log("[roads] placed %s - drag its points onto the road, then run snap()"
               % a.get_actor_label())
    return a


def snap(offset=20.0, save=True):
    """Pull every road point down onto the ground.

    Drawing a spline in a top-down view leaves every point at the camera's height. This is the
    step that makes a drawn road usable; run it after any editing session.
    """
    ignore = _ignore()
    moved = 0
    for (a, sc) in roads():
        for i in range(sc.get_number_of_spline_points()):
            p = sc.get_location_at_spline_point(i, unreal.SplineCoordinateSpace.WORLD)
            z = _ground(p.x, p.y, p.z, ignore)
            if z is None:
                unreal.log_warning("[roads] %s point %d has no ground under it" % (a.get_actor_label(), i))
                continue
            if abs((z + offset) - p.z) > 1.0:
                sc.set_location_at_spline_point(i, unreal.Vector(p.x, p.y, z + offset),
                                                unreal.SplineCoordinateSpace.WORLD, False)
                moved += 1
        sc.update_spline()

    # The junction MARKERS have to come too. roads_at() measures from the marker, so a marker left
    # at an old buried Z reads as far from roads that are in fact right under it - GS_Junction_9
    # silently dropped GS_Road_20 that way.
    jmoved = 0
    for j in junctions():
        p = j.get_actor_location()
        z = _ground(p.x, p.y, p.z, ignore)
        if z is None:
            unreal.log_warning("[roads] %s has no ground under it" % j.get_actor_label())
            continue
        if abs((z + offset) - p.z) > 1.0:
            j.set_actor_location(unreal.Vector(p.x, p.y, z + offset), False, False)
            jmoved += 1
    unreal.log("[roads] snapped %d road point(s) and %d junction marker(s) to the ground"
               % (moved, jmoved))
    if save:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return moved


def _target_road():
    """The road to edit: the selected GS_Road_*, else the most recently added one."""
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in eas.get_selected_level_actors():
        if a.get_actor_label().startswith(ROAD_PREFIX):
            sc = a.get_component_by_class(unreal.SplineComponent)
            if sc:
                return a, sc
    rs = roads()
    return rs[-1] if rs else (None, None)


def _camera_ground():
    """Where the viewport camera is looking, on the ground."""
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    loc, rot = ues.get_level_viewport_camera_info()
    fwd = unreal.MathLibrary.get_forward_vector(rot)
    ignore = _ignore()
    # trace along the camera's aim first, so looking down at a spot puts the point THERE
    hit = unreal.SystemLibrary.line_trace_single(
        _world(), loc,
        unreal.Vector(loc.x + fwd.x * 80000.0, loc.y + fwd.y * 80000.0, loc.z + fwd.z * 80000.0),
        unreal.TraceTypeQuery.ECC_VISIBILITY, True, ignore, unreal.DrawDebugTrace.NONE, True)
    info = hit.to_dict() if hit else None
    if info and info.get("blocking_hit"):
        p = info["impact_point"]
        return unreal.Vector(p.x, p.y, p.z)
    # aiming at the sky - fall back to straight down from a point ahead
    x, y = loc.x + fwd.x * 5000.0, loc.y + fwd.y * 5000.0
    z = _ground(x, y, loc.z, ignore)
    return unreal.Vector(x, y, loc.z if z is None else z)


def add_point():
    """Append a point where the camera is looking. Fly the road and click along it.

    Far easier than dragging points in a top-down view: point the viewport at the road and add
    a point, move along, add another. The spline builds itself along the route you walked.
    """
    a, sc = _target_road()
    if not sc:
        unreal.log_warning("[roads] no road selected or present - run add_road() first")
        return None
    p = _camera_ground()
    sc.add_spline_point(unreal.Vector(p.x, p.y, p.z + 20.0),
                        unreal.SplineCoordinateSpace.WORLD, True)
    unreal.log("[roads] %s: point %d at (%.0f, %.0f, %.0f)"
               % (a.get_actor_label(), sc.get_number_of_spline_points() - 1, p.x, p.y, p.z))
    return a


def remove_last_point():
    """Undo the last add_point()."""
    a, sc = _target_road()
    if not sc or sc.get_number_of_spline_points() == 0:
        unreal.log_warning("[roads] nothing to remove")
        return None
    sc.remove_spline_point(sc.get_number_of_spline_points() - 1, True)
    unreal.log("[roads] %s: removed last point, %d left"
               % (a.get_actor_label(), sc.get_number_of_spline_points()))
    return a


def start_road():
    """Begin a fresh road with a single point at the camera, ready for add_point()."""
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    bp = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).load_asset(ROAD_BP)
    p = _camera_ground()
    a = eas.spawn_actor_from_object(bp, p, unreal.Rotator(0, 0, 0))
    a.set_actor_label("%s%d" % (ROAD_PREFIX, len(roads())))
    sc = a.get_component_by_class(unreal.SplineComponent)
    sc.clear_spline_points(False)
    sc.add_spline_point(unreal.Vector(p.x, p.y, p.z + 20.0),
                        unreal.SplineCoordinateSpace.WORLD, True)
    eas.set_selected_level_actors([a])
    unreal.log("[roads] started %s - now fly the road and use 'Add point at camera'"
               % a.get_actor_label())
    return a


def sample(step=400.0):
    """Points along every road, for anything that needs the network as data."""
    out = []
    for (_a, sc) in roads():
        length = sc.get_spline_length()
        n = max(2, int(length / step) + 1)
        for i in range(n):
            d = length * i / float(n - 1)
            p = sc.get_location_at_distance_along_spline(d, unreal.SplineCoordinateSpace.WORLD)
            out.append((p.x, p.y, p.z))
    return out


JUNCTION_PREFIX = "GS_Junction_"
JUNCTION_RADIUS = 700.0


def junctions(actors=None):
    """Every placed junction marker, sorted by label."""
    out = [a for a in (actors or _actors()) if a.get_actor_label().startswith(JUNCTION_PREFIX)]
    return sorted(out, key=lambda a: a.get_actor_label())


def roads_at(location, radius=JUNCTION_RADIUS):
    """Which roads pass within `radius` of a point, and how close each comes.

    This is what makes a junction queryable: a patrol standing on one can ask which ways lead out.
    """
    out = []
    for (a, sc) in roads():
        closest = sc.find_location_closest_to_world_location(
            location, unreal.SplineCoordinateSpace.WORLD)
        d = ((location.x - closest.x) ** 2 + (location.y - closest.y) ** 2) ** 0.5
        if d <= radius:
            key = sc.find_input_key_closest_to_world_location(location)
            n = sc.get_number_of_spline_points()
            # an endpoint join is a dead-end into the junction; mid-spline is a through road
            end = "start" if key < 0.5 else ("end" if key > n - 1.5 else "through")
            out.append((a.get_actor_label(), d, end))
    return sorted(out, key=lambda t: t[1])


def _spawn_junction(loc, label):
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, loc, unreal.Rotator(0, 0, 0))
    a.set_actor_label(label)
    smc = a.static_mesh_component
    smc.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Sphere"))
    smc.set_world_scale3d(unreal.Vector(4.0, 4.0, 4.0))
    smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    smc.set_hidden_in_game(True)
    a.tags = [unreal.Name("GS.RoadJunction")]
    return a


def _next_junction_label():
    used = set()
    for a in junctions():
        try:
            used.add(int(a.get_actor_label()[len(JUNCTION_PREFIX):]))
        except ValueError:
            pass
    i = 0
    while i in used:
        i += 1
    return "%s%d" % (JUNCTION_PREFIX, i)


def mark_junction(radius=JUNCTION_RADIUS, weld=True, save=True):
    """Mark a junction where the camera is looking, and pull the roads there onto it.

    A junction that is only 'two splines that happen to be near each other' is invisible to code
    and breaks the moment someone nudges a point. Marking makes it an object with a position.
    """
    p = _camera_ground()
    loc = unreal.Vector(p.x, p.y, p.z + 20.0)
    near = roads_at(loc, radius)
    if not near:
        unreal.log_warning("[roads] no road within %.0fuu - junction not placed" % radius)
        return None
    a = _spawn_junction(loc, _next_junction_label())
    welded = _weld_to(loc, radius) if weld else 0
    unreal.log("[roads] %s at (%.0f, %.0f) - %d road(s): %s%s"
               % (a.get_actor_label(), loc.x, loc.y, len(near),
                  ", ".join("%s(%s, %.0fuu)" % (n[0], n[2], n[1]) for n in near),
                  "; welded %d endpoint(s)" % welded if welded else ""))
    unreal.get_editor_subsystem(unreal.EditorActorSubsystem).set_selected_level_actors([a])
    if save:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return a


def _weld_to(loc, radius):
    """Snap any road ENDPOINT within radius exactly onto a junction position."""
    welded = 0
    for (a, sc) in roads():
        n = sc.get_number_of_spline_points()
        if n < 2:
            continue
        for idx in (0, n - 1):
            p = sc.get_location_at_spline_point(idx, unreal.SplineCoordinateSpace.WORLD)
            d = ((p.x - loc.x) ** 2 + (p.y - loc.y) ** 2) ** 0.5
            if 1.0 < d <= radius:
                sc.set_location_at_spline_point(idx, loc, unreal.SplineCoordinateSpace.WORLD, False)
                welded += 1
        sc.update_spline()
    return welded


def auto_junctions(tolerance=JUNCTION_RADIUS, save=True):
    """Place a marker at every place roads already meet, so the network is explicit.

    Meeting points are clustered first - three roads at one crossroads is ONE junction, not three.
    """
    rs = roads()
    hits = []
    for (a, sc) in rs:
        n = sc.get_number_of_spline_points()
        if n < 2:
            continue
        for idx in (0, n - 1):
            p = sc.get_location_at_spline_point(idx, unreal.SplineCoordinateSpace.WORLD)
            for (b, sb) in rs:
                if b.get_actor_label() == a.get_actor_label():
                    continue
                c = sb.find_location_closest_to_world_location(p, unreal.SplineCoordinateSpace.WORLD)
                if ((p.x - c.x) ** 2 + (p.y - c.y) ** 2) ** 0.5 <= tolerance:
                    hits.append(unreal.Vector((p.x + c.x) / 2, (p.y + c.y) / 2, (p.z + c.z) / 2))

    clusters = []
    for h in hits:
        for cl in clusters:
            if ((cl[0].x - h.x) ** 2 + (cl[0].y - h.y) ** 2) ** 0.5 <= tolerance:
                cl.append(h)
                break
        else:
            clusters.append([h])

    existing = [a.get_actor_location() for a in junctions()]
    made = []
    for cl in clusters:
        cx = sum(v.x for v in cl) / len(cl)
        cy = sum(v.y for v in cl) / len(cl)
        cz = sum(v.z for v in cl) / len(cl)
        loc = unreal.Vector(cx, cy, cz)
        if any(((e.x - cx) ** 2 + (e.y - cy) ** 2) ** 0.5 <= tolerance for e in existing):
            continue                              # already marked
        a = _spawn_junction(loc, _next_junction_label())
        _weld_to(loc, tolerance)
        made.append(a.get_actor_label())
        existing.append(loc)
        unreal.log("[roads] %s at (%.0f, %.0f) - %s"
                   % (a.get_actor_label(), cx, cy,
                      ", ".join(n[0] for n in roads_at(loc, tolerance))))
    unreal.log("[roads] auto_junctions: %d new marker(s)" % len(made))
    if save:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return made


def branch(length=12000.0, points=3):
    """Start a new road AT the selected junction, so it is connected by construction."""
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    sel = [a for a in eas.get_selected_level_actors()
           if a.get_actor_label().startswith(JUNCTION_PREFIX)]
    if not sel:
        js = junctions()
        if not js:
            unreal.log_warning("[roads] no junction selected or present - run mark_junction()")
            return None
        sel = [js[-1]]
    j = sel[0]
    start = j.get_actor_location()
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    _loc, rot = ues.get_level_viewport_camera_info()
    fwd = unreal.MathLibrary.get_forward_vector(rot)
    ignore = _ignore()

    bp = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).load_asset(ROAD_BP)
    a = eas.spawn_actor_from_object(bp, start, unreal.Rotator(0, 0, 0))
    a.set_actor_label("%s%d" % (ROAD_PREFIX, len(roads())))
    sc = a.get_component_by_class(unreal.SplineComponent)
    sc.clear_spline_points(False)
    sc.add_spline_point(start, unreal.SplineCoordinateSpace.WORLD, False)   # exactly on the junction
    for i in range(1, points):
        t = length * i / float(points - 1)
        x, y = start.x + fwd.x * t, start.y + fwd.y * t
        z = _ground(x, y, start.z, ignore)
        sc.add_spline_point(unreal.Vector(x, y, (start.z if z is None else z) + 20.0),
                            unreal.SplineCoordinateSpace.WORLD, False)
    sc.update_spline()
    eas.set_selected_level_actors([a])
    unreal.log("[roads] %s branches from %s - drag it, or use 'Add point at camera'"
               % (a.get_actor_label(), j.get_actor_label()))
    return a


def check(weld_tolerance=400.0, weld=False):
    """Validate the drawn network: junctions, near-misses, and points off the ground.

    Roads drawn separately almost touch rather than meeting. A 14uu gap looks joined and is not,
    which matters the moment anything paths along them.
    """
    rs = roads()
    ends = []
    for (a, sc) in rs:
        n = sc.get_number_of_spline_points()
        if n < 2:
            continue
        for idx in (0, n - 1):
            p = sc.get_location_at_spline_point(idx, unreal.SplineCoordinateSpace.WORLD)
            ends.append([a.get_actor_label(), sc, idx, p])

    # Endpoint against the nearest point on every OTHER spline, not endpoint-to-endpoint: a road
    # that ends partway along another is a T-junction, and that is most of a real network.
    joins, welded = [], 0
    for (la, sa, ia, pa) in ends:
        for (b, sb) in rs:
            lb = b.get_actor_label()
            if lb == la:
                continue
            closest = sb.find_location_closest_to_world_location(
                pa, unreal.SplineCoordinateSpace.WORLD)
            d = ((pa.x - closest.x) ** 2 + (pa.y - closest.y) ** 2) ** 0.5
            if d <= weld_tolerance:
                joins.append((la, ia, lb, d))
                if weld and d > 1.0:
                    sa.set_location_at_spline_point(
                        ia, unreal.Vector(closest.x, closest.y, closest.z),
                        unreal.SplineCoordinateSpace.WORLD, False)
                    welded += 1
    if weld:
        for (_a, sc) in rs:
            sc.update_spline()

    ignore = _ignore()
    floating = []
    for (a, sc) in rs:
        for i in range(sc.get_number_of_spline_points()):
            p = sc.get_location_at_spline_point(i, unreal.SplineCoordinateSpace.WORLD)
            z = _ground(p.x, p.y, p.z, ignore)
            if z is None or abs(p.z - (z + 20.0)) > 150.0:
                floating.append((a.get_actor_label(), i, None if z is None else p.z - z))

    unreal.log("[roads] %d road(s); %d endpoint junction(s) within %.0fuu"
               % (len(rs), len(joins), weld_tolerance))
    for (la, ia, lb, d) in joins:
        unreal.log("[roads]    %s pt%d meets %s at %.0fuu%s"
                   % (la, ia, lb, d, " (welded)" if weld and d > 1.0 else ""))
    if floating:
        unreal.log_warning("[roads] %d point(s) not sitting on the ground:" % len(floating))
        for (lab, i, gap) in floating[:10]:
            unreal.log_warning("[roads]    %s pt%d %s"
                               % (lab, i, "NO GROUND" if gap is None else "%.0fuu off" % gap))
    # marked junctions, and meeting points nobody has marked yet
    js = junctions()
    unreal.log("[roads] %d marked junction(s)" % len(js))
    for j in js:
        near = roads_at(j.get_actor_location())
        unreal.log("[roads]    %-16s %s" % (j.get_actor_label(),
                   ", ".join("%s(%s)" % (n[0], n[2]) for n in near) or "NO ROADS - stale marker"))
    unmarked = []
    for (la, ia, lb, d) in joins:
        for (a2, sc2) in rs:
            if a2.get_actor_label() != la:
                continue
            p = sc2.get_location_at_spline_point(ia, unreal.SplineCoordinateSpace.WORLD)
            if not any(((j.get_actor_location().x - p.x) ** 2
                        + (j.get_actor_location().y - p.y) ** 2) ** 0.5 <= JUNCTION_RADIUS
                       for j in js):
                unmarked.append((la, ia, lb, d))
    if unmarked:
        unreal.log_warning("[roads] %d meeting point(s) with NO junction marker:" % len(unmarked))
        for (la, ia, lb, d) in unmarked:
            unreal.log_warning("[roads]    %s pt%d / %s (%.0fuu) - run auto_junctions()"
                               % (la, ia, lb, d))
    if weld:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return {"roads": len(rs), "junctions": joins, "welded": welded, "floating": floating,
            "marked": [j.get_actor_label() for j in js], "unmarked": unmarked}


def report():
    rs = roads()
    if not rs:
        unreal.log_warning("[roads] no %s* actors - run add_road()" % ROAD_PREFIX)
        return []
    rows = []
    for (a, sc) in rs:
        n = sc.get_number_of_spline_points()
        L = sc.get_spline_length()
        unreal.log("[roads] %-14s %2d points, %.0fuu long" % (a.get_actor_label(), n, L))
        rows.append((a.get_actor_label(), n, L))
    unreal.log("[roads] %d road(s), %.0fuu total" % (len(rows), sum(r[2] for r in rows)))
    return rows


PANEL = "/Game/EditorTools/EUW_GSRoadTools"


def open_panel():
    """Open the floating Road Tools panel - buttons instead of menu trips."""
    aes = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    bp = aes.load_asset(PANEL)
    if not bp:
        unreal.log_warning("[roads] %s is missing" % PANEL)
        return None
    eus = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
    res = eus.spawn_and_register_tab_and_get_id(bp)
    tab = res[1] if isinstance(res, tuple) else res
    unreal.log("[roads] Road Tools panel open (%s)" % tab)
    return tab


def register_menu():
    """Tools -> Goblin Siege -> Roads."""
    menus = unreal.ToolMenus.get()
    tools = menus.find_menu("LevelEditor.MainMenu.Tools")
    if not tools:
        return False
    sub = tools.add_sub_menu("LevelEditor.MainMenu.Tools", "GoblinSiege", "GSRoads", "Roads")
    for label, fn, tip in (
            ("Open Road Tools panel", "open_panel", "Floating button panel - no menu trips."),
            ("Start a road here", "start_road", "Begin a one-point road at the camera."),
            ("Add point at camera", "add_point", "Append a point where you are looking."),
            ("Remove last point", "remove_last_point", "Undo the last added point."),
            ("Mark junction here", "mark_junction", "Mark a junction and weld roads onto it."),
            ("Auto-mark all junctions", "auto_junctions", "Mark every place roads already meet."),
            ("Branch a road from junction", "branch", "Start a new road on the selected junction."),
            ("Check network", "check", "Junctions, gaps and points off the ground."),
            ("Add a 4-point road here", "add_road", "Drop a draggable spline at the camera."),
            ("Snap roads to ground", "snap", "Pull every road point down onto the terrain."),
            ("Report roads", "report", "List the road splines and their lengths.")):
        e = unreal.ToolMenuEntry(name="GSRoads_%s" % fn, type=unreal.MultiBlockType.MENU_ENTRY)
        e.set_label(label)
        e.set_tool_tip(tip)
        e.set_string_command(unreal.ToolMenuStringCommandType.PYTHON, "",
                             string="import gs_roads; gs_roads.%s()" % fn)
        sub.add_menu_entry("Roads", e)
    menus.refresh_all_widgets()
    return True


# ============================================================ patrol testing (PIE)

def start_patrols():
    """Start every ACF patrol loop in the RUNNING game. Use during PIE.

    Nothing calls StartPatrolLoop in normal play - ACF expects a BT service or a spawner to do it,
    and this project has neither. Without it the blackboard's TargetLocation stays at FLT_MAX and
    the AI is failing for a reason that has nothing to do with patrolling. Click this first, then
    open the Behavior Tree debugger.
    """
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    w = ues.get_game_world()
    if not w:
        unreal.log_warning("[roads] not in PIE - press Play first")
        return 0
    started = 0
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
        for pc in a.get_components_by_class(unreal.ACFAIPatrolComponent):
            path = pc.get_editor_property("path_to_follow")
            if not path:
                continue
            pc.start_patrol_loop(True)
            ctl = a.get_instigator_controller() if hasattr(a, "get_instigator_controller") else None
            bb = ctl.get_components_by_class(unreal.BlackboardComponent) if ctl else []
            tl = ""
            if bb:
                try:
                    v = bb[0].get_value_as_vector("TargetLocation")
                    tl = "  TargetLocation=(%.0f, %.0f)" % (v.x, v.y)
                except Exception:
                    pass
            unreal.log("[roads] patrol started: %-24s path=%s%s"
                       % (a.get_name(), path.get_actor_label(), tl))
            started += 1
    if started == 0:
        unreal.log_warning("[roads] no actors with a patrol component AND a path assigned")
    else:
        unreal.log("[roads] started %d patrol loop(s)" % started)
    return started


def patrol_status():
    """Report every patroller's live state - the numbers to read next to the BT debugger."""
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:
        unreal.log_warning("[roads] not in PIE")
        return []
    rows = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
        for pc in a.get_components_by_class(unreal.ACFAIPatrolComponent):
            ctl = a.get_instigator_controller() if hasattr(a, "get_instigator_controller") else None
            p = a.get_actor_location()
            v = a.get_velocity()
            tl = None
            if ctl:
                bb = ctl.get_components_by_class(unreal.BlackboardComponent)
                if bb:
                    try:
                        tl = bb[0].get_value_as_vector("TargetLocation")
                    except Exception:
                        pass
            # FLT_MAX means the key was never written - i.e. patrol never started
            unwritten = tl is not None and tl.x > 1e30
            unreal.log("[roads] %-24s speed %6.1f  active=%-5s  target=%s"
                       % (a.get_name(), (v.x ** 2 + v.y ** 2) ** 0.5,
                          pc.is_patrol_loop_active(),
                          "UNSET (patrol never started)" if unwritten
                          else ("(%.0f, %.0f)" % (tl.x, tl.y) if tl else "?")))
            rows.append((a.get_name(), (v.x ** 2 + v.y ** 2) ** 0.5))
    return rows
