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
class FMetaRoadToolPresetManager;

/**
 * SMetaRoadPresetPanel
 *
 * The "Preset" Edit sub-mode panel: a preset picker combo (FMetaRoadToolPresetManager) + a details view of
 * the preview manager's transient working-copy build settings (flattened by FMetaRoadBuildSettingsDetails)
 * + an "Apply To Selected" button. Owns its preset manager (bound to FEdModePresetContext). Applying a
 * preset rebuilds the live preview. RefreshWorkingObjects re-points the details view when the working
 * copies change (called by the mode).
 */
class SMetaRoadPresetPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaRoadPresetPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UMetaRoadEditorMode>, EditorMode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Re-point the working-copy details view at the mode's current preset working objects. */
	void RefreshWorkingObjects();

private:
	// A preset's values were applied to the working copies — rebuild the live preview.
	void OnPresetApplied();

	TWeakObjectPtr<UMetaRoadEditorMode> EditorMode;
	TSharedPtr<FMetaRoadToolPresetManager> PresetManager;
	TSharedPtr<IDetailsView> PresetDetailsView;
};
