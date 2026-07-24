/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadCurbProfileAction.h"
#include "Assets/RoadCurbProfile.h"
#include "AssetTypeCategories.h"
#include "Modules/ModuleManager.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "MetaRoadEditorModule.h"
#include "SourceCodeNavigation.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "Styling/SlateIconFinder.h"

#define LOCTEXT_NAMESPACE "RoadCurbProfileAction"


FText UAssetDefinition_RoadCurbProfile::GetAssetDisplayName() const
{
	return LOCTEXT("RoadCurbProfileActionName", "Curb Profile");
}

FText UAssetDefinition_RoadCurbProfile::GetAssetDisplayName(const FAssetData& AssetData) const
{
	return Super::GetAssetDisplayName(AssetData);
}

FText UAssetDefinition_RoadCurbProfile::GetAssetDescription(const FAssetData& AssetData) const
{
	return Super::GetAssetDescription(AssetData);
}

FLinearColor UAssetDefinition_RoadCurbProfile::GetAssetColor() const
{
	return FColor(201, 29, 85);
}

TSoftClassPtr<UObject> UAssetDefinition_RoadCurbProfile::GetAssetClass() const
{
	return URoadCurbProfile::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_RoadCurbProfile::GetAssetCategories() const
{
	static FAssetCategoryPath Category(FText::FromName(FMetaRoadEditorModule::AssetCategoryName));
	static const FAssetCategoryPath Categories[] = { Category };

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	return Categories;
}

UThumbnailInfo* UAssetDefinition_RoadCurbProfile::LoadThumbnailInfo(const FAssetData& InAssetData) const
{
	return UE::Editor::FindOrCreateThumbnailInfo(InAssetData.GetAsset(), USceneThumbnailInfo::StaticClass());
}

EAssetCommandResult UAssetDefinition_RoadCurbProfile::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	TArray<UObject*> Objects = OpenArgs.LoadObjects<UObject>();
	FSimpleAssetEditor::CreateEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Objects);

	return EAssetCommandResult::Handled;
}

const FSlateBrush* UAssetDefinition_RoadCurbProfile::GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const
{
	return FSlateIconFinder::FindIconForClass(URoadCurbProfile::StaticClass()).GetIcon();
}

const FSlateBrush* UAssetDefinition_RoadCurbProfile::GetIconBrush(const FAssetData& InAssetData, const FName InClassName) const
{
	return FSlateIconFinder::FindIconForClass(URoadCurbProfile::StaticClass()).GetIcon();
}

#undef LOCTEXT_NAMESPACE
