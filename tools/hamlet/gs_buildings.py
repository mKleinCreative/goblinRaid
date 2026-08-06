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

SAVE = False
LINK = 600.0          # cluster link distance AND flammable spread radius - deliberately identical
MIN_PIECES = 6        # below this it is a fence or a lone prop, not a building
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

comps = [c for c in comps if len(c) >= MIN_PIECES]
comps.sort(key=len, reverse=True)
say("clusters at link=%.0f with >=%d pieces: %d" % (LINK, MIN_PIECES, len(comps)))
say("  sizes: %s%s" % ([len(c) for c in comps[:12]], " ..." if len(comps) > 12 else ""))

existing = [a for a in actors if isinstance(a, unreal.GSBuildingObjective)]
say("existing building objectives: %d" % len(existing))

made = 0
for idx, comp in enumerate(comps):
    label = LABEL % idx
    if any(a.get_actor_label() == label for a in actors):
        say("  SKIP %s exists" % label)
        continue

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
    say("  CREATED %-16s %3d pieces  span=%6.0f  at (%.0f, %.0f, %.0f)"
        % (label, len(comp), span, cx, cy, cz))
    made += 1

say("\ncreated %d building objective(s)" % made)

# ---- windows become breakable, which is what makes them a way in
wins = [a for a in pieces if any(k in (mesh_of(a) or "") for k in WINDOW_KEYS)]
say("\nwindow actors: %d" % len(wins))
added = 0
for a in wins:
    if a.get_component_by_class(unreal.GSBreakableComponent):
        continue
    try:
        # AddInstanceComponent under the hood, or it evaporates on reload - the same trap the
        # market's flammable components fell into.
        c = a.add_component_by_class(unreal.GSBreakableComponent, False, unreal.Transform(), False)
        if c:
            added += 1
    except Exception as exc:
        say("  add failed on %s: %s" % (a.get_actor_label(), str(exc)[:70]))
        break
say("breakable components added: %d" % added)
say("NOTE: BrokenCollection is left empty - windows hide + puff until fracture assets exist.")

if SAVE:
    les.save_current_level()
    say("\nSAVED")
else:
    say("\nNOT SAVED - set SAVE=True once the cluster sizes above look like buildings.")
flush()
