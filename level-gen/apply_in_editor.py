"""
Place a generated plan into the open level. Run inside the editor:

    python gs_ue.py level-gen\\apply_in_editor.py --timeout 600

Reads level-gen/out/plan.json. This is the ONLY step that needs the editor; generation,
evaluation and refinement all run headless (CLAUDE.md constraint 2 — the editor is a scarce,
human-gated resource, so the loop must not depend on it).

THREE REFUSALS, ON PURPOSE
--------------------------
  1. A plan built from the synthetic kit is refused. Its dimensions are invented, and the
     whole discipline of this pipeline is that no dimension is invented.
  2. A plan that did not PASS evaluation is refused. Shipping an unevaluated layout is the
     No Man's Sky failure the pipeline exists to avoid.
  3. L_Tutorial_Island is refused outright. It is hand-authored content (GDD 2.8) and 184 MB;
     generated actors go on a scratch map.

IDEMPOTENT: every actor placed here is tagged, and a re-run destroys the previous set first —
the same "derived data, rebuilt every run" rule tools/hamlet/gs_buildings.py established, and
for the same reason: two overlapping definitions of the same thing is worse than none.
"""
import json

import unreal

OUT = []
TAG = "GSLevelGen"
PLAN = r"D:\goblinRaid\level-gen\out\plan.json"
EVAL = r"D:\goblinRaid\level-gen\out\evaluation.json"
FORBIDDEN_MAPS = ("L_Tutorial_Island",)


def say(s):
    OUT.append(str(s))


def main():
    with open(PLAN, encoding="utf-8") as fh:
        plan = json.load(fh)

    if plan.get("synthetic_kit"):
        say("REFUSED: this plan was built from the synthetic kit — its dimensions are "
            "invented. Run kit_manifest.py, then regenerate.")
        return

    try:
        with open(EVAL, encoding="utf-8") as fh:
            summary = json.load(fh)["summary"]
        if plan["seed"] not in summary.get("passed", []):
            say(f"REFUSED: seed {plan['seed']} is not in the passed list {summary.get('passed')}. "
                "An unevaluated layout does not get placed.")
            return
    except FileNotFoundError:
        say("REFUSED: no evaluation.json beside the plan — cannot confirm it passed.")
        return

    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    asset_sub = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)

    world = les.get_current_level()
    map_name = str(unreal.SystemLibrary.get_object_name(world.get_outer())) if world else "?"
    for bad in FORBIDDEN_MAPS:
        if bad.lower() in map_name.lower():
            say(f"REFUSED: current map is {map_name}. Generated actors do not go on the "
                f"hand-authored tutorial hamlet. Open a scratch map first.")
            return
    say(f"map: {map_name}")

    # Idempotence: destroy the previous generated set before placing a new one.
    killed = 0
    for a in eas.get_all_level_actors():
        if TAG in [str(t) for t in a.tags]:
            eas.destroy_actor(a)
            killed += 1
    say(f"removed {killed} actor(s) from a previous run")

    placed, missing = 0, set()
    for b in plan["buildings"]:
        for p in b["placements"]:
            mesh_path = p.get("path") or _lookup(plan, p["mesh"])
            mesh = asset_sub.load_asset(mesh_path) if mesh_path else None
            if not isinstance(mesh, unreal.StaticMesh):
                missing.add(p["mesh"])
                continue
            loc = unreal.Vector(p["x"], p["y"], p["z"])
            rot = unreal.Rotator(0.0, 0.0, p.get("yaw", 0.0))
            actor = eas.spawn_actor_from_object(mesh, loc, rot)
            if actor:
                actor.set_actor_label(f"GEN_{b['id']}_{placed}")
                actor.tags = [unreal.Name(TAG), unreal.Name(f"GEN_{b['kind']}")]
                placed += 1

    say(f"placed {placed} static mesh actor(s)")
    if missing:
        say(f"{len(missing)} mesh(es) could not be resolved: {sorted(missing)[:8]}")

    # The 3.3 contract: markers the Town agent reads. Placed as tagged empty actors so the
    # perception system can find guard posts and patrol loops without re-deriving them.
    marks = 0
    for name, pts in (("GuardPost", plan["guard_posts"]),
                      ("CivilianAnchor", plan["civilian_anchors"]),
                      ("Signpost", plan["signposts"])):
        for i, (mx, my) in enumerate(pts):
            a = eas.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(mx, my, 0.0))
            if a:
                a.set_actor_label(f"GEN_{name}_{i}")
                a.tags = [unreal.Name(TAG), unreal.Name(f"GEN_{name}")]
                marks += 1
    for li, loop in enumerate(plan["patrol_loops"]):
        for i, (mx, my) in enumerate(loop):
            a = eas.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(mx, my, 0.0))
            if a:
                a.set_actor_label(f"GEN_PatrolLoop{li}_{i}")
                a.tags = [unreal.Name(TAG), unreal.Name("GEN_PatrolPoint")]
                marks += 1
    say(f"placed {marks} marker(s) (guard posts, civilian anchors, signposts, patrol points)")

    say("NOT saving the level — this is a scratch map and the placement is cheap to redo. "
        "Save from the editor if you want to keep it.")


def _lookup(plan, mesh_name):
    """Plans carry mesh names; kit.json carries the paths."""
    global _KIT
    try:
        _KIT
    except NameError:
        with open(r"D:\goblinRaid\level-gen\kit.json", encoding="utf-8") as fh:
            _KIT = {m["name"]: m["path"] for m in json.load(fh)["meshes"]}
    return _KIT.get(mesh_name)


try:
    main()
except Exception:  # noqa: BLE001
    import traceback
    say("FAILED: " + traceback.format_exc())

print("\n".join(OUT))
