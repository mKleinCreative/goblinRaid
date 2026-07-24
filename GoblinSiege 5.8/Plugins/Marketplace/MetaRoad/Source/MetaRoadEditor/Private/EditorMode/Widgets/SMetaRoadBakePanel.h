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
 * SMetaRoadBakePanel
 *
 * The Bake sub-mode panel (Bake palette): the UMetaRoadBakeSettings details view (Generated Assets) above
 * four buttons — Bake Selected/All, Clear Selected/All — wired to the editor mode. The "Selected" buttons
 * require a selected road; all buttons are disabled while an async bake runs (Mode->IsBaking()).
 */
class SMetaRoadBakePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaRoadBakePanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UMetaRoadEditorMode>, EditorMode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TWeakObjectPtr<UMetaRoadEditorMode> EditorMode;
	TSharedPtr<IDetailsView> BakeSettingsView;
};
