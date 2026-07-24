/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"

namespace MetaRoadWidgetUtils
{
	/** Creates the compact IDetailsView used by the Meta Road sub-mode panels (no name area, no selection
	 *  tip; search off by default). Centralizes the FDetailsViewArgs + PropertyEditor lookup boilerplate. */
	inline TSharedRef<IDetailsView> MakeCompactDetailsView(bool bAllowSearch = false)
	{
		FPropertyEditorModule& PropertyEditorModule =
			FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FDetailsViewArgs Args;
		Args.bAllowSearch = bAllowSearch;
		Args.bHideSelectionTip = true;
		Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		return PropertyEditorModule.CreateDetailView(Args);
	}
}
