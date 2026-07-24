/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadBuildPresetAction.h"
#include "Assets/RoadBuildPreset.h"
#include "MetaRoadEditorModule.h"

#define LOCTEXT_NAMESPACE "RoadBuildPresetAction"

FText UAssetDefinition_RoadBuildPreset::GetAssetDisplayName() const
{
	return LOCTEXT("RoadBuildPresetActionName", "Road Build Preset");
}

FLinearColor UAssetDefinition_RoadBuildPreset::GetAssetColor() const
{
	return FColor(85, 170, 255);
}

TSoftClassPtr<UObject> UAssetDefinition_RoadBuildPreset::GetAssetClass() const
{
	return URoadBuildPreset::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_RoadBuildPreset::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { FAssetCategoryPath(FText::FromName(FMetaRoadEditorModule::AssetCategoryName)) };
	return Categories;
}

#undef LOCTEXT_NAMESPACE
