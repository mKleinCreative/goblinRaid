/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "EditorMode/MetaRoadVisibilitySettings.h"
#include "MetaRoadEditorModule.h"
#include "RoadEditorCommands.h"
#include "Editor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaRoadVisibilitySettings)

void UMetaRoadVisibilitySettings::SyncFromModule()
{
	bTilesVisibility = FMetaRoadEditorModule::IsTileRendersVisibleInEditor();
}

void UMetaRoadVisibilitySettings::UnhideAllSplines()
{
	// Reuse the bound command so the actor-iteration logic lives in one place.
	FMetaRoadEditorModule::Get().GetCommandList()->TryExecuteAction(
		FRoadEditorCommands::Get().UnhideAllSpline.ToSharedRef());
}

#if WITH_EDITOR
void UMetaRoadVisibilitySettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaRoadVisibilitySettings, bTilesVisibility))
	{
		// The TileMapWindowVisibility command is a pure toggle (+ render refresh); fire it only
		// when the module flag disagrees with the new property value so they end up in sync.
		if (FMetaRoadEditorModule::IsTileRendersVisibleInEditor() != bTilesVisibility)
		{
			FMetaRoadEditorModule::Get().GetCommandList()->TryExecuteAction(
				FRoadEditorCommands::Get().TileMapWindowVisibility.ToSharedRef());
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaRoadVisibilitySettings, bDrawBoundaries)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UMetaRoadVisibilitySettings, bShowWireframe))
	{
		// Debug-draw toggles are read live by UMetaRoadEditorMode::Render / Tick — persist + repaint.
		SaveConfig();
		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports();
		}
	}
}
#endif
