"""
Give every building on L_Tutorial_Island an AGSBuildingObjective, so it can be set on fire.

    python gs_ue.py tools\\hamlet\\gs_buildings.py --timeout 900

Destroys and rebuilds the whole set on each run - buildings are DERIVED DATA, reproducible from
the level, and leaving stale ones behind means two overlapping definitions of the same house each
adopting a share of its pieces.

WHAT A BUILDING IS - AND WHY IT IS NOT A CLUSTER RADIUS
------------------------------------------------------
Four attempts clustered kit pieces by distance: all shell pieces at XY 600 (7 buildings, the whole
village core fused into one), then tighter (multi-storey houses split per storey), then roof pieces
at XY 250 (71 "buildings", which was Michael's tavern cut into ~30 objectives plus one placed on
top of the windmill). Every value traded one failure for the other, because distance was never the
right question.

Michael settled it by annotating a top-down of the village, one stroke per house. 47 downtown. The
level itself already carries that identity two different ways, and neither is a distance:

  1. SM_MERGED_House_*  -  ONE ACTOR IS ONE WHOLE HOUSE.
     61 of them. The village's ordinary houses were merged to single meshes, so they have no
     separate roof, wall or window piece at all. This is why roof clustering could not find them:
     there was nothing to cluster. It found 245 roof TILES, which belong almost entirely to a
     handful of detailed kitbashed buildings - House_2x1_T9 alone is ONE building with 46 tiles.

  2. An ATTACHED HIERARCHY  -  the detailed kitbashed buildings.
     Innbase (498 pieces, 48 roof tiles) is Michael's tavern, which he confirmed is one objective.
     Also House_2x1_L7_Detailed, House_2x1_T9, House_1x3_10, WaterMill_Closed, SM_House_Window_A.
     Their pieces are attached into one hierarchy, so the subtree IS the building.

Both are identity carried by the level. Nothing here is tuned, and re-running on a changed map
cannot silently drift the way a radius does.

EXCLUDED, DELIBERATELY
----------------------
  Distant_*   21 backdrop houses at X 25k..38k and -36k..-58k, off the island. Scenery the player
              can never reach; a burnable objective there is an objective that can never be met.
  Wells       SM_WellRoof matches "Roof" but a stone well is not a building.
  Windmill    already AGSMillObjective, and the one structure Michael crossed out on his annotation.
              Its hierarchy is 2 pieces, so the >=8 piece floor drops it; the proximity check below
              is the belt to that braces.

THE RULE THIS SCRIPT STILL RESPECTS
-----------------------------------
    A cluster objective's adopt radius must not exceed what its spread distance can traverse.

The market spent a session unwinnable because it adopted 64 scattered stalls when fire could reach
~30. Adopting more pieces makes a building HARDER to burn, not richer, and nothing reports the
shortfall. So radii here come from the building's own geometry and are capped - see RADIUS_CAP.
"""
import math
from collections import defaultdict

import unreal

SAVE = True

# One actor, one whole house. The prefix the Dreamscape village kit uses for its merged meshes.
MERGED = "SM_MERGED_"
BACKDROP = "Distant"          # off-island scenery
NOT_A_BUILDING_MESH = ("Well", "Bridge", "Fence", "QuestBoard", "Terrain", "CloudCard", "SkySphere")

# An attached hierarchy is a building if it has a roof and enough pieces to be a structure. Eight
# drops SM_WIndmill_Base_Blueprint (2 pieces, 1 roof) without naming it.
MIN_HIERARCHY_PIECES = 8

# Radius cap. Innbase's subtree spans 10,722 uu because fences and paths are attached to the same
# root as the inn; sizing a radius from that would adopt half the village and set a burn threshold
# the building could never meet. Centres and spans come from ROOF pieces only, which mark where the
# structure actually is, and the cap is the backstop.
RADIUS_CAP = 3500.0

# Do not place a building on top of an objective that already exists (the windmill, the market).
OBJECTIVE_CLEARANCE = 1500.0

LABEL = "GS_Building_%02d"

OUT = []
def say(s): OUT.append(str(s))
def flush(): print("\n".join(OUT))

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
lib = unreal.GSRaidLibrary

if les.is_in_play_in_editor():
    say("REFUSING: PIE is running - editor-world edits are invisible during PIE.")
    flush(); raise SystemExit


def mesh_of(a):
    c = a.get_component_by_class(unreal.StaticMeshComponent)
    return c.static_mesh.get_name() if (c and c.static_mesh) else None


def bounds_of(a):
    """World-space centre and half-extent.

    NOT get_actor_location(): this kit offsets its meshes from their actor pivot by a median of
    287 uu (max 671), so pivots group and size things wrongly - that bug once left 76 of 113
    windows owned by no building at all.
    """
    o, e = a.get_actor_bounds(False)
    return (o.x, o.y, o.z), (e.x, e.y, e.z)


def root_of(a):
    n = 0
    while a.get_attach_parent_actor() and n < 50:
        a = a.get_attach_parent_actor()
        n += 1
    return a


def is_roof(m):
    return m and "Roof" in m and "Beam" not in m


actors = eas.get_all_level_actors()

# ---------------------------------------------------------------- existing objectives to avoid
avoid = []
for a in actors:
    if isinstance(a, unreal.GSBurnObjectiveBase) and not isinstance(a, unreal.GSBuildingObjective):
        c, _e = bounds_of(a)
        avoid.append(c)
say("existing non-building objectives to keep clear of: %d" % len(avoid))


def too_close_to_objective(c):
    return any(math.hypot(c[0] - o[0], c[1] - o[1]) < OBJECTIVE_CLEARANCE for o in avoid)


# ---------------------------------------------------------------- 1. merged houses: 1 actor = 1 house
candidates = []   # (kind, centre, radius, detail)
merged_skipped_backdrop = 0
for a in actors:
    if isinstance(a, unreal.GSBurnObjectiveBase):
        continue
    m = mesh_of(a)
    if not m or not m.startswith(MERGED) or "House" not in m:
        continue
    if BACKDROP in m:
        merged_skipped_backdrop += 1
        continue
    c, e = bounds_of(a)
    # Its own footprint plus a little, so adoption picks up the porch and any loose prop leaning
    # on it, and no further.
    r = max(e[0], e[1]) * 1.15 + 150.0
    candidates.append(("merged", c, min(r, RADIUS_CAP), m))

say("merged house actors (1 actor = 1 house): %d   [%d Distant_* backdrop skipped]"
    % (len(candidates), merged_skipped_backdrop))

# ---------------------------------------------------------------- 2. attached kit hierarchies
sub = defaultdict(list)
for a in actors:
    if isinstance(a, unreal.GSBurnObjectiveBase):
        continue
    m = mesh_of(a)
    if not m or not a.get_attach_parent_actor():
        continue
    sub[root_of(a).get_actor_label()].append((a, m))

hier = 0
for label, members in sorted(sub.items()):
    if len(members) < MIN_HIERARCHY_PIECES:
        continue
    roofs = [(a, m) for a, m in members
             if is_roof(m) and not any(k in m for k in NOT_A_BUILDING_MESH)]
    if not roofs:
        continue                      # bridges, fences, quest boards - no roof, not a building
    # Centre and span from the ROOFS. The subtree also holds attached fences and paths, which would
    # drag the centre off the building and inflate the radius.
    pts = [bounds_of(a) for a, _m in roofs]
    cx = sum(p[0][0] for p in pts) / len(pts)
    cy = sum(p[0][1] for p in pts) / len(pts)
    cz = sum(p[0][2] for p in pts) / len(pts)
    span = max(math.dist((cx, cy, cz), p[0]) + max(p[1]) for p in pts) + 300.0
    candidates.append(("hierarchy", (cx, cy, cz), min(span, RADIUS_CAP),
                       "%s (%d pieces, %d roof tiles)" % (label, len(members), len(roofs))))
    hier += 1
say("attached kit hierarchies with a roof: %d" % hier)

# ---------------------------------------------------------------- drop anything on an objective
kept = []
for cand in candidates:
    if too_close_to_objective(cand[1]):
        say("  SKIPPED %s - sits on an existing objective (windmill/market)" % cand[3][:52])
        continue
    kept.append(cand)
candidates = kept

say("")
say("BUILDINGS TO CREATE: %d" % len(candidates))

# ---------------------------------------------------------------- rebuild
stale = [a for a in actors if isinstance(a, unreal.GSBuildingObjective)]
for a in stale:
    eas.destroy_actor(a)
say("removed %d existing building objective(s) to rebuild" % len(stale))

made = 0
for idx, (kind, c, r, detail) in enumerate(candidates):
    label = LABEL % idx
    b = eas.spawn_actor_from_class(unreal.GSBuildingObjective,
                                   unreal.Vector(c[0], c[1], c[2]), unreal.Rotator(0, 0, 0))
    b.set_actor_label(label)
    b.set_editor_property("adopt_radius", r)
    lib.set_objective_identity(b, "Objective.Burn.House", "A House")
    made += 1
    if kind == "hierarchy" or made <= 4:
        say("  %-16s %-10s r=%5.0f  at (%.0f, %.0f, %.0f)  %s"
            % (label, kind, r, c[0], c[1], c[2], detail[:46]))
say("\ncreated %d building objective(s)" % made)

# ---------------------------------------------------------------- windows become a way in
#
# Only the kitbashed buildings have separate window actors; a merged house has its windows baked
# into the single mesh, so there is nothing to break. Those houses are ROOF-ONLY entries, which is
# reported rather than papered over.
actors = eas.get_all_level_actors()
wins = [a for a in actors
        if "Window" in (mesh_of(a) or "") and not isinstance(a, unreal.GSBurnObjectiveBase)]
added = 0
for a in wins:
    if a.get_component_by_class(unreal.GSBreakableComponent):
        continue
    # UGSRaidLibrary::MakeActorBreakable, NOT add_component_by_class - that does not exist on a
    # StaticMeshActor and silently added zero components on the first run. The C++ helper also
    # routes through AddInstanceComponent, without which the component would not survive a save.
    if lib.make_actor_breakable(a, True):
        added += 1
say("\nwindow actors: %d   breakable components added: %d" % (len(wins), added))
say("NOTE: BrokenCollection is left empty - windows hide + puff until fracture assets exist.")

if SAVE:
    les.save_current_level()
    say("\nSAVED")
else:
    say("\nNOT SAVED")
flush()
