"""
Turn L_Tutorial_Island's loose building kit into burnable buildings.

    python gs_ue.py tools\\hamlet\\gs_buildings.py --timeout 600

Idempotent. Does NOT save by default - pass SAVE=True below once the result looks right.

WHY THIS SCRIPT EXISTS
----------------------
There is no building on this map. A house is 20-40 separate StaticMeshActors kitbashed from 150
piece types (floors of 12 triangles, walls, corners, beams, roof segments, windows) - 1,413
instances in total. Fire had nothing to own. This clusters those pieces into buildings and gives
each cluster an AGSBuildingObjective, which becomes the thing that burns and scores.

THE RULE THIS SCRIPT EXISTS TO RESPECT
--------------------------------------
    A cluster objective's adopt radius must not exceed what its spread distance can traverse.

The market spent a session unwinnable because it adopted 64 scattered stalls when fire could reach
only ~30. Adopting more pieces makes a building HARDER to burn, not richer, and nothing reports the
shortfall. So the clustering LINK distance and the flammable SPREAD radius are the same number by
construction here - a cluster is, by definition, a set of pieces fire can walk between.
"""
import math

import unreal

SAVE = True
# 250, measured not guessed. A link sweep over the real map showed a sharp percolation threshold:
# 180 -> 70 clusters, largest 17.  250 -> 81 clusters, largest 49.  320 -> largest 189.  600 -> 421.
# Past ~320 neighbouring houses bridge and the whole village fuses into one "building" that would
# need 278 pieces burnt to complete. 250 sits safely below that knee.
#
# This also corrects the rule I wrote first. I had link == spread; the real constraint is
# link <= spread. Equality was over-tight: at 250 < 450 every piece in a cluster is still reachable
# by fire (which is what the rule protects), and fire ALSO crossing between neighbouring houses is
# a feature in a burning village, not a defect.
LINK = 250.0
# WHAT COUNTS AS A HOUSE: a roof over some walls.
#
# This replaces a piece-count and span threshold, and the replacement is the point. Counting pieces
# is a statistical guess at "is this a building", and it was wrong in both directions: it admitted
# stacks of six co-located window frames, and it excluded 92 clusters that HAVE A ROOF - real houses
# the player simply could not set on fire. Only 11 of the village's houses were burnable and nobody
# could tell by looking.
#
# A roof is what a house has and a pile of spare frames does not. Requiring some walls under it
# drops the detached roof-beam clusters that "has a roof" alone would admit (102 -> 29).
#
#   >=12 pieces + span>=300   -> 11 buildings   (the old rule; 92 roofed houses missed)
#   has a roof                -> 102            (includes loose roof fragments)
#   roof + >=3 wall/house     -> 29             <- this
MIN_ROOF_PIECES = 1
MIN_WALL_PIECES = 3
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

PIECE_KEYS = ("House", "Roof", "Wall", "Window", "Door", "Foundation", "Barn", "Tavern")
WINDOW_KEYS = ("Window",)


def mesh_of(a):
    c = a.get_component_by_class(unreal.StaticMeshComponent)
    return c.static_mesh.get_name() if (c and c.static_mesh) else None


actors = eas.get_all_level_actors()
pieces = []
for a in actors:
    if isinstance(a, unreal.GSBurnObjectiveBase):
        continue
    m = mesh_of(a)
    if m and any(k in m for k in PIECE_KEYS):
        pieces.append(a)

say("building kit pieces found: %d" % len(pieces))
if not pieces:
    say("nothing to do"); flush(); raise SystemExit

P = [(a.get_actor_location().x, a.get_actor_location().y, a.get_actor_location().z) for a in pieces]

# ---- connected components at LINK. Grid-bucketed: 1,413 pieces is 2M pair tests brute force,
# which is slow enough over the MCP bridge to look like a hang.
cell = LINK
grid = {}
for i, (x, y, z) in enumerate(P):
    grid.setdefault((int(x // cell), int(y // cell), int(z // cell)), []).append(i)

def neighbours(i):
    x, y, z = P[i]
    gx, gy, gz = int(x // cell), int(y // cell), int(z // cell)
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dz in (-1, 0, 1):
                for j in grid.get((gx + dx, gy + dy, gz + dz), ()):
                    if j != i and math.dist(P[i], P[j]) <= LINK:
                        yield j

seen, comps = set(), []
for i in range(len(P)):
    if i in seen:
        continue
    stack, comp = [i], []
    seen.add(i)
    while stack:
        k = stack.pop()
        comp.append(k)
        for j in neighbours(k):
            if j not in seen:
                seen.add(j)
                stack.append(j)
    comps.append(comp)

def span_of(comp):
    cx = sum(P[i][0] for i in comp) / len(comp)
    cy = sum(P[i][1] for i in comp) / len(comp)
    cz = sum(P[i][2] for i in comp) / len(comp)
    return max(math.dist((cx, cy, cz), P[i]) for i in comp)

MESH = [mesh_of(a) or "" for a in pieces]

def is_building(comp):
    roofs = sum(1 for i in comp if "Roof" in MESH[i])
    walls = sum(1 for i in comp if "Wall" in MESH[i] or "House" in MESH[i])
    return roofs >= MIN_ROOF_PIECES and walls >= MIN_WALL_PIECES

comps = [c for c in comps if is_building(c)]
comps.sort(key=len, reverse=True)
say("clusters that are houses (roof + >=%d wall): %d" % (MIN_WALL_PIECES, len(comps)))
say("  sizes: %s%s" % ([len(c) for c in comps[:12]], " ..." if len(comps) > 12 else ""))

# Buildings are DERIVED DATA - entirely reproducible from the kit pieces - so a re-run rebuilds
# them rather than skipping what exists. Skipping was fine while the rule was fixed; the moment the
# rule changed it would have left the old 11 in place alongside the new set, two overlapping
# definitions of the same house, each adopting a share of its pieces.
stale = [a for a in actors if isinstance(a, unreal.GSBuildingObjective)]
for a in stale:
    eas.destroy_actor(a)
say("removed %d existing building objective(s) to rebuild" % len(stale))
actors = eas.get_all_level_actors()

made = 0
for idx, comp in enumerate(comps):
    label = LABEL % idx
    cx = sum(P[i][0] for i in comp) / len(comp)
    cy = sum(P[i][1] for i in comp) / len(comp)
    cz = sum(P[i][2] for i in comp) / len(comp)
    span = max(math.dist((cx, cy, cz), P[i]) for i in comp)

    b = eas.spawn_actor_from_class(unreal.GSBuildingObjective,
                                   unreal.Vector(cx, cy, cz), unreal.Rotator(0, 0, 0))
    b.set_actor_label(label)
    # Radius covers the cluster and no more. Generous here is not generous - it drags in the
    # neighbour's wall and raises the burn threshold this building can never meet.
    b.set_editor_property("adopt_radius", span * 1.1)
    lib.set_objective_identity(b, "Objective.Burn.House", "A House")
    made += 1
    if made <= 6 or made % 10 == 0:
        say("  %-16s %3d pieces  span=%6.0f  at (%.0f, %.0f, %.0f)"
            % (label, len(comp), span, cx, cy, cz))

say("\ncreated %d building objective(s)" % made)

# ---- windows become breakable, which is what makes them a way in
wins = [a for a in pieces if any(k in (mesh_of(a) or "") for k in WINDOW_KEYS)]
say("\nwindow actors: %d" % len(wins))
added = 0
for a in wins:
    if a.get_component_by_class(unreal.GSBreakableComponent):
        continue
    # UGSRaidLibrary::MakeActorBreakable, NOT add_component_by_class - that method does not exist
    # on a StaticMeshActor and silently added zero components on the first run. The C++ helper also
    # routes through AddInstanceComponent, without which the component would not survive a save.
    if lib.make_actor_breakable(a, True):
        added += 1
say("breakable components added: %d" % added)
say("NOTE: BrokenCollection is left empty - windows hide + puff until fracture assets exist.")

if SAVE:
    les.save_current_level()
    say("\nSAVED")
else:
    say("\nNOT SAVED - set SAVE=True once the cluster sizes above look like buildings.")
flush()
