import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
dirty = list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) + \
        list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
print('in PIE:', les.is_in_play_in_editor())
print('dirty :', [p.get_name() for p in dirty])
print('SAFE TO CLOSE' if not dirty and not les.is_in_play_in_editor() else 'NOT SAFE')
