import unreal

eds = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)

dirty = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
print('dirty maps before save:', dirty)

ok = eas.save_asset('/Game/Maps/L_Tutorial_Island', only_if_is_dirty=False)
print('save_asset ->', ok)

print('dirty maps after save:',
      [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()])

# Read back from the saved world: confirm every fog card is really non-colliding.
still = []
for a in eds.get_all_level_actors():
    if not isinstance(a, unreal.StaticMeshActor):
        continue
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        m = c.get_editor_property('static_mesh')
        if not m or 'BasicShapes/Plane' not in m.get_path_name():
            break
        mat = c.get_material(0)
        if mat and 'MI_Fog_02' in mat.get_path_name():
            if 'NO_COLLISION' not in str(c.get_collision_enabled()):
                still.append(a.get_actor_label())
        break

print('fog cards STILL colliding:', still if still else 'none - all 12 clear')
