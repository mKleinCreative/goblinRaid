/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/WeakObjectPtr.h"

class UMetaRoadEditorMode;
class IDetailsView;

/**
 * SMetaRoadFbxExportPanel
 *
 * The "FBX Export" sub-mode panel (Bake palette): the UMetaRoadFbxExportSettings details view above two
 * buttons — Export Selected / Export All — wired to the editor mode. "Export Selected" requires a selected
 * road; both buttons are disabled while an async bake runs (Mode->IsBaking()).
 */
class SMetaRoadFbxExportPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaRoadFbxExportPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UMetaRoadEditorMode>, EditorMode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TWeakObjectPtr<UMetaRoadEditorMode> EditorMode;
	TSharedPtr<IDetailsView> ExportSettingsView;
};
