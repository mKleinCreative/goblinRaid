"""
Read how a human assembled the kit. Run in the editor:

    python gs_ue.py level-gen\\extract_buildings.py --timeout 600

Opens a reference map, groups its static meshes into buildings, and dumps every piece's
position RELATIVE to its building origin into level-gen/reference/<map>.json.

WHY
---
The composer's assembly grammar was invented from measured bounds — which produced houses
that were geometrically valid and did not look like the pack's. But the Dreamscape Showcase
and the tutorial hamlet already contain buildings a human made from these exact pieces. The
grammar is sitting in the level; nobody had read it.

This is the same move `tools/hamlet/gs_buildings.py` made for building *identity* — "the
level itself already carries that identity two different ways, and neither is a distance" —
applied to building *composition*.

GROUPING
--------
Two kinds of building, per gs_buildings.py:
  1. An attached hierarchy — the subtree IS the building. Used where present.
  2. Otherwise, single-actor merged meshes, which teach nothing about assembly and are
     recorded but flagged.
Loose kit pieces that belong to no hierarchy are clustered by a link distance derived from
the measured module (not a tuned constant): pieces within one module of each other are the
same structure.
"""
import json
import math

import unreal

OUT = []
MAPS = {
    # The Dreamscape "Showcase" map is a CATALOGUE, not a village: 141 pieces, every mesh
    # appearing exactly once, at off-grid positions across 29 Z levels. It displays one of
    # everything for inspection and teaches nothing about assembly. Kept here as a record of
    # a checked-and-rejected source.
    # "Showcase": "/Game/DreamscapeSeries/DreamscapeFarmlands/Maps/Showcase",

    # The real teachers: the detailed kitbashed buildings on the hamlet, which gs_buildings.py
    # already identified as attached hierarchies where "the subtree IS the building" —
    # Innbase (498 pieces, Michael's tavern), House_2x1_T9, House_2x1_L7_Detailed,
    # House_1x3_10, WaterMill_Closed.
    "TutorialIsland": "/Game/Maps/L_Tutorial_Island",
}
DEST_DIR = r"D:\goblinRaid\level-gen\reference"
KIT_JSON = r"D:\goblinRaid\level-gen\kit.json"


def say(s):
    OUT.append(str(s))


def mesh_name(comp):
    m = comp.static_mesh
    return str(m.get_name()) if m else None


def main():
    import os
    os.makedirs(DEST_DIR, exist_ok=True)

    with open(KIT_JSON, encoding="utf-8") as fh:
        kit = {m["name"]: m for m in json.load(fh)["meshes"]}
    module = kit["SM_House_Floor_5x4_01"]["size_cm"][0]
    say(f"module = {module:.1f} cm (measured)")

    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    dirty = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or [])
             + list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() or []))
    if dirty:
        say(f"REFUSED: {len(dirty)} unsaved package(s); opening a map would lose them")
        return

    for label, path in MAPS.items():
        say(f"\n=== {label} ===")
        les.load_level(path)
        actors = eas.get_all_level_actors()
        say(f"actors: {len(actors)}")

        # Every static mesh instance in the level that belongs to the kit.
        pieces = []
        for a in actors:
            root_label = str(a.get_actor_label())
            for comp in a.get_components_by_class(unreal.StaticMeshComponent):
                n = mesh_name(comp)
                if not n or n not in kit:
                    continue
                t = comp.get_world_transform()
                loc, rot, scale = t.translation, t.rotation.rotator(), t.scale3d
                pieces.append({
                    "mesh": n,
                    "actor": root_label,
                    "attach_root": str(a.get_attach_parent_actor().get_actor_label())
                                   if a.get_attach_parent_actor() else root_label,
                    "loc": [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)],
                    "yaw": round(rot.yaw, 1), "pitch": round(rot.pitch, 1),
                    "roll": round(rot.roll, 1),
                    "scale": [round(scale.x, 3), round(scale.y, 3), round(scale.z, 3)],
                })
        say(f"kit piece instances: {len(pieces)}")

        # Group: attachment root first; loose pieces cluster by one module of link distance.
        groups = {}
        loose = []
        for p in pieces:
            if p["attach_root"] != p["actor"]:
                groups.setdefault(p["attach_root"], []).append(p)
            else:
                loose.append(p)

        clusters = []
        remaining = list(loose)
        while remaining:
            seed = remaining.pop()
            cluster = [seed]
            changed = True
            while changed:
                changed = False
                for q in list(remaining):
                    for c in cluster:
                        if (abs(q["loc"][0] - c["loc"][0]) <= module * 1.5 and
                                abs(q["loc"][1] - c["loc"][1]) <= module * 1.5 and
                                abs(q["loc"][2] - c["loc"][2]) <= module * 2.5):
                            cluster.append(q)
                            remaining.remove(q)
                            changed = True
                            break
            clusters.append(cluster)

        for i, c in enumerate(clusters):
            if len(c) >= 4:                       # a building, not a stray prop
                groups[f"cluster_{i}"] = c

        # Re-express each building relative to its own min corner, so two buildings in
        # different places compare directly.
        buildings = []
        for name, ps in groups.items():
            if len(ps) < 4:
                continue
            ox = min(p["loc"][0] for p in ps)
            oy = min(p["loc"][1] for p in ps)
            oz = min(p["loc"][2] for p in ps)
            buildings.append({
                "id": name,
                "piece_count": len(ps),
                "origin": [ox, oy, oz],
                "pieces": sorted(
                    [{"mesh": p["mesh"],
                      "rel": [round(p["loc"][0] - ox, 1), round(p["loc"][1] - oy, 1),
                              round(p["loc"][2] - oz, 1)],
                      "yaw": p["yaw"], "pitch": p["pitch"], "roll": p["roll"],
                      "scale": p["scale"]}
                     for p in ps],
                    key=lambda d: (d["rel"][2], d["rel"][0], d["rel"][1])),
            })
        buildings.sort(key=lambda b: -b["piece_count"])

        dest = DEST_DIR + "\\" + label + ".json"
        with open(dest, "w", encoding="utf-8") as fh:
            json.dump({"map": path, "module_cm": module, "buildings": buildings}, fh, indent=1)
        say(f"wrote {dest}: {len(buildings)} building(s)")
        for b in buildings[:8]:
            say(f"   {b['id']:<28} {b['piece_count']:>4} pieces")


try:
    main()
except Exception:
    import traceback
    say("FAILED: " + traceback.format_exc())

print("\n".join(OUT))
