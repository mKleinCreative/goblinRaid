"""
Measure the Dreamscape modular kit. Run ONCE, inside the editor:

    python gs_ue.py level-gen\\kit_manifest.py --timeout 300

Writes level-gen/kit.json: for every kit mesh, its real bounds, pivot offset and origin.

WHY THIS EXISTS AS A SEPARATE, MEASURED STEP
--------------------------------------------
The pieces are named `..._5x4_...`. It is tempting to decide that means 500x400 cm and hardcode
it. `tools/hamlet/gs_buildings.py` records what that instinct cost last time: four rounds of
clustering kit pieces by a *tuned distance*, each value trading one failure for another, "because
distance was never the right question". The level already carried the answer; nobody had asked it.

So the generator asks. Every dimension it uses is measured here and cached. If kit.json is
missing, the generator refuses to emit a placeable plan rather than guessing - see kit.py.

EDITOR GOTCHAS THIS SCRIPT IS WRITTEN AROUND (both from GoblinSiege 5.8/CLAUDE.md)
---------------------------------------------------------------------------------
  1. `unreal.EditorAssetLibrary` is a silent no-op in this build - does_asset_exist() returns
     False and load_asset() returns None for assets that demonstrably exist. Use
     `unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)`.
  2. execute_python_code DISCARDS buffered stdout. Accumulate output and print once at the end,
     the same way gs_wire_tutorial.py does.
"""
import json

import unreal

OUT = []


def say(s):
    OUT.append(str(s))


ROOTS = [
    "/Game/DreamscapeSeries/DreamscapeFarmlands/Meshes/Modular/House",
    "/Game/DreamscapeSeries/DreamscapeFarmlands/Meshes/Structures",
]
DEST = r"D:\goblinRaid\level-gen\kit.json"


def measure(asset_subsystem, registry, path):
    """Bounds of one static mesh, in cm, plus where its pivot sits inside them."""
    mesh = asset_subsystem.load_asset(path)
    if not isinstance(mesh, unreal.StaticMesh):
        return None
    b = mesh.get_bounding_box()          # FBox in local space
    mn, mx = b.min, b.max
    size = (mx.x - mn.x, mx.y - mn.y, mx.z - mn.z)
    return {
        "path": path,
        "name": path.rsplit("/", 1)[-1],
        "size_cm": [round(v, 2) for v in size],
        "min_cm": [round(mn.x, 2), round(mn.y, 2), round(mn.z, 2)],
        "max_cm": [round(mx.x, 2), round(mx.y, 2), round(mx.z, 2)],
        # Pivot offset: how far the origin sits from the box's min corner. Placement maths
        # needs this - a wall whose pivot is centred and one whose pivot is at a corner
        # snap to the same grid cell only if you account for it.
        "pivot_from_min_cm": [round(-mn.x, 2), round(-mn.y, 2), round(-mn.z, 2)],
    }


def main():
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    registry = unreal.AssetRegistryHelpers.get_asset_registry()

    entries, skipped = [], 0
    for root in ROOTS:
        assets = registry.get_assets_by_path(unreal.Name(root), recursive=True)
        say(f"{root}: {len(assets)} assets")
        for a in assets:
            path = str(a.package_name)
            try:
                m = measure(asset_subsystem, registry, path)
            except Exception as exc:  # noqa: BLE001 - one bad asset must not lose the pass
                say(f"  SKIP {path}: {type(exc).__name__} {exc}")
                skipped += 1
                continue
            if m is None:
                skipped += 1
                continue
            entries.append(m)

    entries.sort(key=lambda e: e["name"])
    manifest = {
        "measured_in_editor": True,
        "engine": "UE 5.8",
        "roots": ROOTS,
        "count": len(entries),
        "meshes": entries,
    }
    with open(DEST, "w", encoding="utf-8") as fh:
        json.dump(manifest, fh, indent=2)

    say(f"measured {len(entries)} meshes, skipped {skipped}")
    say(f"wrote {DEST}")

    # A quick sanity read-out: the foundation pieces define the module grid, so print them
    # rather than making anyone open the json to find out what 5x4 actually means.
    for e in entries:
        if "Foundation" in e["name"]:
            say(f"  {e['name']:<45} {e['size_cm']}")


try:
    main()
except Exception as exc:  # noqa: BLE001
    import traceback
    say("FAILED: " + traceback.format_exc())

print("\n".join(OUT))
