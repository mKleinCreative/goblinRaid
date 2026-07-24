/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;

/**
 * SMetaRoadVisibilityPanel
 *
 * The "Visibility" sub-mode panel (Misk palette): a details view over the config-backed
 * UMetaRoadVisibilitySettings CDO (Tiles Visibility + debug toggles + Unhide All). Edits the same CDO the
 * pipeline/mode read globally; SyncFromModule() refreshes the module-mirrored fields on construction.
 */
class SMetaRoadVisibilityPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaRoadVisibilityPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<IDetailsView> DetailsView;
};
