/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "EditorMode/Widgets/SMetaRoadVisibilityPanel.h"
#include "EditorMode/Widgets/MetaRoadWidgetUtils.h"
#include "EditorMode/MetaRoadVisibilitySettings.h"

#include "IDetailsView.h"

void SMetaRoadVisibilityPanel::Construct(const FArguments& InArgs)
{
	// Edit the config-backed CDO so the global debug toggles (bDrawBoundaries / bShowWireframe) the pipeline
	// + mode read are the same instance shown here. SyncFromModule mirrors the module-driven tile visibility.
	UMetaRoadVisibilitySettings* Settings = UMetaRoadVisibilitySettings::Get();
	Settings->SyncFromModule();

	DetailsView = MetaRoadWidgetUtils::MakeCompactDetailsView();
	DetailsView->SetObject(Settings);

	ChildSlot
	[
		DetailsView.ToSharedRef()
	];
}
