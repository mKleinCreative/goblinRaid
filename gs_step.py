import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les.is_in_play_in_editor():
    les.editor_request_end_play()
    print('PIE ended (level actors were only moved in PIE - nothing persists)')
dirty = list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) + \
        list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
print('dirty:', [p.get_name() for p in dirty])
