"""
Create the scratch map that generated hamlets get placed on. Run in the editor:

    python gs_ue.py level-gen\\make_scratch_map.py --timeout 300

Creates /Game/Maps/L_LevelGen_Scratch if it does not exist and opens it: a ground plane big
enough for a hamlet, a directional light, a sky, and a PlayerStart — enough to walk the layout
on foot, which is the only review that has ever caught anything here. Week 1's on-foot pass
found a road cutting through a house and a windmill on an unbuildable rock face; the
fly-through had missed both.

Refuses to run if anything is unsaved, and never touches L_Tutorial_Island.
"""
import unreal

OUT = []
MAP_PATH = "/Game/Maps/L_LevelGen_Scratch"
HALF_EXTENT_CM = 14000.0     # the generator's site is +/-12000; give it margin


def say(s):
    OUT.append(str(s))


def main():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    esub = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)

    # EditorLoadingAndSavingUtils, NOT EditorAssetSubsystem — the first version called this
    # on the wrong class behind a hasattr(), so it reported "clean" when it had not looked.
    # That is the exact failure this pipeline's evaluator refuses to make: a check that
    # cannot run must not read as a check that passed. So no hasattr guard here — if the
    # API moves, this raises and the run stops.
    dirty = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or [])
             + list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() or []))
    if dirty:
        names = [str(p.get_name()) for p in dirty][:8]
        say(f"REFUSED: {len(dirty)} unsaved package(s) — switching maps would lose them: {names}")
        return
    say("unsaved packages: 0 (verified, not assumed)")

    if esub.does_asset_exist(MAP_PATH):
        say(f"{MAP_PATH} exists — opening it")
        les.load_level(MAP_PATH)
    else:
        say(f"creating {MAP_PATH}")
        if not les.new_level(MAP_PATH):
            say("FAILED: new_level returned False")
            return

    # Idempotence. The first version spawned the ground and a full lighting rig on every
    # run, so re-opening the map stacked a second sun on the first and the editor said so:
    # "Multiple directional lights are competing to be the single one used for forward
    # shading." Placement scripts in this repo rebuild derived data rather than adding to
    # it — the same rule tools/hamlet/gs_buildings.py established.
    existing = {str(a.get_actor_label()) for a in eas.get_all_level_actors()}

    def already(label: str) -> bool:
        if label in existing:
            say(f"  {label}: already present, left alone")
            return True
        return False

    # Ground: a scaled cube, because a hamlet spans ~180 m and the default template floor
    # is a few metres across.
    cube = esub.load_asset("/Engine/BasicShapes/Cube.Cube")
    if cube and not already("ScratchGround"):
        ground = eas.spawn_actor_from_object(cube, unreal.Vector(0, 0, -50.0))
        if ground:
            ground.set_actor_label("ScratchGround")
            ground.set_actor_scale3d(
                unreal.Vector(HALF_EXTENT_CM * 2 / 100.0, HALF_EXTENT_CM * 2 / 100.0, 1.0))
            say(f"ground plane: {HALF_EXTENT_CM * 2 / 100.0:.0f} m square")

    for cls, label, loc, rot in (
        (unreal.DirectionalLight, "Sun", unreal.Vector(0, 0, 5000), unreal.Rotator(0, -46, 30)),
        (unreal.SkyLight, "SkyLight", unreal.Vector(0, 0, 4000), unreal.Rotator(0, 0, 0)),
        (unreal.SkyAtmosphere, "SkyAtmosphere", unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0)),
        (unreal.ExponentialHeightFog, "Fog", unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0)),
    ):
        if already(label):
            continue
        try:
            a = eas.spawn_actor_from_class(cls, loc, rot)
            if a:
                a.set_actor_label(label)
        except Exception as exc:  # noqa: BLE001 - a missing lighting class must not stop the map
            say(f"  (skipped {label}: {type(exc).__name__})")

    # PlayerStart at the runic site, so an on-foot walk begins where a raid does.
    try:
        import json
        with open(r"D:\goblinRaid\level-gen\out\plan.json", encoding="utf-8") as fh:
            plan = json.load(fh)
        rs = plan["runic_site"]
        # The runic site moves with the seed, so this one IS rebuilt rather than skipped.
        for a in eas.get_all_level_actors():
            if str(a.get_actor_label()).startswith("PlayerStart_RunicSite"):
                eas.destroy_actor(a)
        ps = eas.spawn_actor_from_class(unreal.PlayerStart,
                                        unreal.Vector(rs[0], rs[1], 200.0))
        if ps:
            ps.set_actor_label("PlayerStart_RunicSite")
            say(f"PlayerStart at the runic site ({rs[0]:.0f}, {rs[1]:.0f}) — an on-foot walk "
                f"starts where the raid does")
    except Exception as exc:  # noqa: BLE001
        say(f"  (no PlayerStart: {type(exc).__name__} {exc})")

    les.save_current_level()
    say(f"saved {MAP_PATH}")
    say("now: python gs_ue.py level-gen\\apply_in_editor.py --timeout 600")


try:
    main()
except Exception:  # noqa: BLE001
    import traceback
    say("FAILED: " + traceback.format_exc())

print("\n".join(OUT))
