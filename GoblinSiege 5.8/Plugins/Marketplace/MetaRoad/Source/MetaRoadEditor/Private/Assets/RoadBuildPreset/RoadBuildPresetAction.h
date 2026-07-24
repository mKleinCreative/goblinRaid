/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "AssetDefinitionDefault.h"
#include "RoadBuildPresetAction.generated.h"

UCLASS(MinimalAPI)
class UAssetDefinition_RoadBuildPreset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	// OpenAssets inherits the default (generic property editor) — presets are mostly created/applied from
	// the mode's Preset panel, not edited directly.
};
