"""Tagging GSRaidMarkers - giving the placed markers a job.

WHY THIS EXISTS. 18 AGSRaidMarker actors are placed on L_Tutorial_Island and **every one of them
has an empty MarkerType**, so nothing reads any of them. `AGSRaidMarker::GatherByType` filters by
tag, which means an untagged marker is invisible to every consumer: GSHordeSubsystem.cpp:307 and
:1022 log "No Marker.HordeArrival markers in this level - the horde has nowhere to arrive from" and
bail. The markers were placed long before anything asked for them.

A marker carries FOUR things, not one (AGSRaidMarker.h:82-101):

    MarkerType   the tag - what job this marker does
    GroupId      which route/set it belongs to
    OrderIndex   position within that group - LOAD-BEARING for a patrol, meaningless outside one
    Radius       how much ground it speaks for

Tagging alone is not enough for a patrol. A patrol route is an ORDERED GROUP, so a set of
PatrolNode markers with no GroupId is 18 unrelated points, not a route. set_patrol_route() exists
because that is the part which is easy to forget and silent when wrong.

Everything writes through UGSRaidLibrary::ConfigureMarker, which is the same edit a designer makes
in the Details panel and which REFUSES an unresolvable tag rather than quietly writing an empty one
(GSRaidLibrary.cpp:70-76).

    import gs_markers
    gs_markers.report()                       # what every marker currently says
    gs_markers.set_type("Marker.HordeArrival", radius=800)     # tag the SELECTED markers
    gs_markers.set_patrol_route("VillageLoop")                 # selected -> an ordered route
"""
import unreal

# The five declared marker tags (Combat/GSGameplayTags.h:214-218). Only Marker.HordeArrival has a
# consumer today; PatrolNode and GuardPost are read by nothing yet, which is a systems gap and not
# a reason to leave the markers blank.
TAGS = (
    "Marker.HordeArrival",
    "Marker.PatrolNode",
    "Marker.GuardPost",
    "Marker.CivilianAnchor",
    "Marker.CoverProp",
    # Added 2026-08-26 via GameplayTagService (config-sourced, no build). Marker.HordeArrival is
    # the PLAYER's goblins walking in on the horn; the DEFENDERS' reinforcements needed their own
    # tag or the boundary ring would have spawned the player's own horde on the perimeter.
    "Marker.DefenderArrival",
)
DEFAULT_RADIUS = 600.0


def _eas():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def markers(actors=None):
    out = [a for a in (actors or _eas().get_all_level_actors())
           if a.get_class().get_name() == "GSRaidMarker"]
    return sorted(out, key=lambda a: a.get_actor_label())


def selected_markers():
    return [a for a in _eas().get_selected_level_actors()
            if a.get_class().get_name() == "GSRaidMarker"]


def _read(a):
    # An EMPTY FGameplayTag stringifies as "None", not "" - so `name or "<none>"` reads a blank tag
    # as tagged and reports 0 untagged on a level where nothing is tagged at all. Test the string
    # against the empty forms, never truthiness.
    tag = a.get_editor_property("marker_type")
    name = str(tag.get_editor_property("tag_name")) if tag else ""
    if name in ("", "None"):
        name = "<none>"
    return {"label": a.get_actor_label(),
            "type": name,
            "group": str(a.get_editor_property("group_id")),
            "order": a.get_editor_property("order_index"),
            "radius": a.get_editor_property("radius")}


def report():
    rows = [_read(a) for a in markers()]
    untagged = [r for r in rows if r["type"] == "<none>"]
    unreal.log("[markers] %d marker(s), %d untagged" % (len(rows), len(untagged)))
    for r in rows:
        unreal.log("[markers]   %-16s %-22s group=%-12s order=%-3d radius=%.0f"
                   % (r["label"], r["type"], r["group"], r["order"], r["radius"]))
    if untagged:
        unreal.log_warning("[markers] %d marker(s) have NO type and are invisible to every "
                           "consumer - GatherByType filters by tag" % len(untagged))
    return rows


def set_type(tag_name, group=None, radius=DEFAULT_RADIUS, start_order=0, save=True):
    """Tag the SELECTED markers. Select them in the viewport or outliner first."""
    sel = selected_markers()
    if not sel:
        unreal.log_warning("[markers] nothing selected - select GSRaidMarker actors first")
        return 0
    if tag_name not in TAGS:
        unreal.log_warning("[markers] '%s' is not a declared marker tag. Declared: %s"
                           % (tag_name, ", ".join(TAGS)))
        return 0
    gid = unreal.Name(group) if group else unreal.Name("None")
    done = 0
    for i, a in enumerate(sel):
        ok = unreal.GSRaidLibrary.configure_marker(
            a, unreal.Name(tag_name), gid, start_order + i, float(radius))
        if ok:
            done += 1
        else:
            unreal.log_warning("[markers] %s REFUSED - tag did not resolve" % a.get_actor_label())
    unreal.log("[markers] %d of %d marker(s) set to %s%s"
               % (done, len(sel), tag_name, " group=%s" % group if group else ""))
    if save:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return done


def set_patrol_route(group, radius=DEFAULT_RADIUS, chain=True, save=True):
    """Turn the SELECTED markers into one ordered patrol route.

    OrderIndex is what makes a route a route. Selection order is not reliable, so by default the
    markers are chained nearest-to-nearest from the first one - which produces a walkable loop
    rather than a set of points that teleports a patrol across the village between waypoints.
    """
    sel = selected_markers()
    if len(sel) < 2:
        unreal.log_warning("[markers] select at least two markers for a route")
        return 0
    ordered = sel
    if chain:
        remaining = list(sel[1:])
        ordered = [sel[0]]
        while remaining:
            here = ordered[-1].get_actor_location()
            nxt = min(remaining, key=lambda m: (m.get_actor_location() - here).length())
            remaining.remove(nxt)
            ordered.append(nxt)
    done = 0
    for i, a in enumerate(ordered):
        if unreal.GSRaidLibrary.configure_marker(
                a, unreal.Name("Marker.PatrolNode"), unreal.Name(group), i, float(radius)):
            done += 1
    unreal.log("[markers] patrol route '%s': %d node(s) in walking order" % (group, done))
    for i, a in enumerate(ordered):
        unreal.log("[markers]    %d. %s" % (i, a.get_actor_label()))
    if save:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return done


def tag_group(group, tag_name, radius=DEFAULT_RADIUS, save=True):
    """Tag every marker already in `group`, KEEPING each one's existing OrderIndex.

    set_type() renumbers from the selection, which would destroy an ordering somebody authored by
    hand. The boundary ring arrived already numbered 0-8 and only lacked a type.
    """
    hit = [a for a in markers() if str(a.get_editor_property("group_id")) == group]
    if not hit:
        unreal.log_warning("[markers] no markers in group '%s'" % group)
        return 0
    done = 0
    for a in hit:
        order = a.get_editor_property("order_index")
        if unreal.GSRaidLibrary.configure_marker(
                a, unreal.Name(tag_name), unreal.Name(group), order, float(radius)):
            done += 1
    unreal.log("[markers] group '%s': %d marker(s) -> %s (order preserved)"
               % (group, done, tag_name))
    if save:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return done


def set_defender_arrival():
    """Selected markers become defender-reinforcement arrival points."""
    return set_type("Marker.DefenderArrival", radius=1000.0)


def clear_type(save=True):
    """Blank the SELECTED markers back to untagged."""
    sel = selected_markers()
    for a in sel:
        a.set_editor_property("marker_type", unreal.GameplayTag())
        a.set_editor_property("group_id", unreal.Name("None"))
        a.set_editor_property("order_index", 0)
    unreal.log("[markers] cleared %d marker(s)" % len(sel))
    if save:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return len(sel)


def select_untagged():
    """Select every marker that still has no type, so they can be tagged in one go."""
    un = [a for a in markers() if _read(a)["type"] == "<none>"]
    _eas().set_selected_level_actors(un)
    unreal.log("[markers] selected %d untagged marker(s)" % len(un))
    return len(un)


def select_by_type(tag_name):
    hit = [a for a in markers() if _read(a)["type"] == tag_name]
    _eas().set_selected_level_actors(hit)
    unreal.log("[markers] selected %d marker(s) of type %s" % (len(hit), tag_name))
    return len(hit)


# --- one-click wrappers, so a panel button needs no arguments -----------------------------------
def set_horde_arrival():
    return set_type("Marker.HordeArrival", radius=800.0)


def set_guard_post():
    return set_type("Marker.GuardPost")


def set_civilian_anchor():
    return set_type("Marker.CivilianAnchor")


def set_cover_prop():
    return set_type("Marker.CoverProp")


def set_patrol_here():
    """Patrol route named after the lowest-numbered selected marker, so each route is distinct."""
    sel = selected_markers()
    if not sel:
        unreal.log_warning("[markers] nothing selected")
        return 0
    return set_patrol_route("Patrol_%s" % sel[0].get_actor_label().replace("GSRaidMarker", ""))


PANEL = "/Game/EditorTools/EUW_GSMarkerTools"


def open_panel():
    """Open the floating Marker Tools panel."""
    aes = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    bp = aes.load_asset(PANEL)
    if not bp:
        unreal.log_warning("[markers] %s is missing" % PANEL)
        return None
    eus = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
    res = eus.spawn_and_register_tab_and_get_id(bp)
    return res[1] if isinstance(res, tuple) else res


def register_menu():
    """Tools -> Goblin Siege -> Markers."""
    menus = unreal.ToolMenus.get()
    tools = menus.find_menu("LevelEditor.MainMenu.Tools")
    if not tools:
        return False
    sub = tools.add_sub_menu("LevelEditor.MainMenu.Tools", "GoblinSiege", "GSMarkers", "Markers")
    for label, fn, tip in (
            ("Open Marker Tools panel", "open_panel", "Floating button panel."),
            ("Report all markers", "report", "Type, group, order and radius for every marker."),
            ("Select untagged", "select_untagged", "Select every marker with no type.")):
        e = unreal.ToolMenuEntry(name="GSMarkers_%s" % fn, type=unreal.MultiBlockType.MENU_ENTRY)
        e.set_label(label)
        e.set_tool_tip(tip)
        e.set_string_command(unreal.ToolMenuStringCommandType.PYTHON, "",
                             string="import gs_markers; gs_markers.%s()" % fn)
        sub.add_menu_entry("Markers", e)
    menus.refresh_all_widgets()
    return True
