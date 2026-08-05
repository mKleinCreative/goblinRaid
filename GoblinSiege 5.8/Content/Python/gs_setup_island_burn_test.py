# gs_setup_island_burn_test.py
#
# Prepares L_Tutorial_Island for a live burn test:
#   1. Points the wheat foliage at MI_GS_VillageWheat (parented to M_GS_Crop_Master,
#      which contains MF_GS_BurnChar and samples the world burn mask).
#   2. Reports where the wheat actually is, so a field objective can be placed on it.
#
# Idempotent. Read-only until it reports; every write is verified by read-back.
#
# Run from the editor with L_Tutorial_Island already open:
#   exec(open(r"D:\goblinRaid\GoblinSiege 5.8\Content\Python\gs_setup_island_burn_test.py").read())

import unreal

WHEAT_MI   = "/Game/VFX/Burn/MI_GS_VillageWheat"
WHEAT_KEY  = "Wheat"
EXPECT_MAP = "L_Tutorial_Island"


def log(tag, msg):
    print("[GS-ISLAND] %-7s | %s" % (tag, msg))


def main():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les.is_in_play_in_editor():
        log("ABORT", "PIE is running - stop it first; PIE-active writes silently no-op.")
        return

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log("MAP", world.get_name())
    if EXPECT_MAP not in world.get_name():
        log("ABORT", "expected %s to be open." % EXPECT_MAP)
        return

    mi = unreal.EditorAssetLibrary.load_asset(WHEAT_MI)
    if not mi:
        log("ABORT", "missing " + WHEAT_MI)
        return
    parent = mi.get_editor_property("parent")
    log("CHECK", "MI parent = %s" % (parent.get_path_name() if parent else None))

    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    swapped, already, comps, bounds = 0, 0, 0, []

    for a in eas.get_all_level_actors():
        if "InstancedFoliageActor" not in a.get_class().get_name():
            continue
        for c in a.get_components_by_class(unreal.FoliageInstancedStaticMeshComponent):
            sm = c.get_editor_property("static_mesh")
            if not sm or WHEAT_KEY not in sm.get_name():
                continue
            comps += 1
            n = c.get_instance_count()
            b = c.get_editor_property("bounds") if False else None
            try:
                o = c.k2_get_component_location()
                bounds.append((sm.get_name(), n, o.x, o.y))
            except Exception:
                bounds.append((sm.get_name(), n, 0.0, 0.0))

            # Foliage draws through the component's override_materials; setting it
            # here affects every instance on that component, which is exactly what
            # a world-space mask wants - one material, position decides the char.
            try:
                cur = c.get_editor_property("override_materials")
                if cur and len(cur) > 0 and cur[0] and cur[0].get_path_name() == mi.get_path_name():
                    already += 1
                    continue
                c.set_editor_property("override_materials", [mi])
                c.modify()
                back = c.get_editor_property("override_materials")
                if back and back[0] and back[0].get_path_name() == mi.get_path_name():
                    swapped += 1
                else:
                    log("WARN", "override did not stick on %s" % sm.get_name())
            except Exception as e:
                log("WARN", "override failed on %s: %s" % (sm.get_name(), e))

    log("INFO", "wheat foliage components: %d | swapped: %d | already: %d" % (comps, swapped, already))
    for nm, n, x, y in bounds:
        log("WHEAT", "%-26s instances=%-7d component at (%.0f, %.0f)" % (nm, n, x, y))

    # Existing burn objectives on this map, if any.
    objs = []
    for a in eas.get_all_level_actors():
        cn = a.get_class().get_name()
        if any(k in cn for k in ("GSFieldFire", "GSMill", "GSMarket", "GSDestructible")):
            loc = a.get_actor_location()
            objs.append("%s '%s' at (%.0f, %.0f, %.0f)" % (cn, a.get_actor_label(), loc.x, loc.y, loc.z))
    if objs:
        log("INFO", "burn objectives already placed:")
        for o in objs:
            log("OBJ", o)
    else:
        log("TODO", "no burn objectives on this map yet - a GSFieldFireObjective must be "
                    "placed over a wheat cluster before there is anything to ignite.")

    if swapped:
        log("SAVE", "level is dirty - save it (Ctrl+S) to keep the material swap.")
    log("NEXT", "PIE, throw a torch into the wheat, and watch for char behind the flame front.")


main()
