/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadProfileAction.h"
#include "Assets/RoadProfile.h"
#include "RoadProfileEditor.h"
#include "AssetTypeCategories.h"
#include "Modules/ModuleManager.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "MetaRoadEditorModule.h"
#include "SourceCodeNavigation.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "Styling/SlateIconFinder.h"

#define LOCTEXT_NAMESPACE "RoadProfileAction"

FText UAssetDefinition_RoadProfile::GetAssetDisplayName() const
{
	return LOCTEXT("RoadProfileActionName", "Road Profile");
}

FLinearColor UAssetDefinition_RoadProfile::GetAssetColor() const
{
	return FColor(201, 29, 85);
}

TSoftClassPtr<UObject> UAssetDefinition_RoadProfile::GetAssetClass() const
{
	return URoadProfile::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_RoadProfile::GetAssetCategories() const
{
	static FAssetCategoryPath Category(FText::FromName(FMetaRoadEditorModule::AssetCategoryName));
	static const FAssetCategoryPath Categories[] = { Category };

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	return Categories;
}

UThumbnailInfo* UAssetDefinition_RoadProfile::LoadThumbnailInfo(const FAssetData& InAssetData) const
{
	return UE::Editor::FindOrCreateThumbnailInfo(InAssetData.GetAsset(), USceneThumbnailInfo::StaticClass());
}

EAssetCommandResult UAssetDefinition_RoadProfile::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	TArray<UObject*> Objects = OpenArgs.LoadObjects<UObject>();
	for (UObject* Object : Objects)
	{
		URoadProfile* RoadProfile = Cast<URoadProfile>(Object);
		if (RoadProfile != nullptr)
		{
			TSharedRef<FRoadProfileEditor> Editor(new FRoadProfileEditor());
			Editor->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, RoadProfile);
		}
	}

	return EAssetCommandResult::Handled;
}

const FSlateBrush* UAssetDefinition_RoadProfile::GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const
{
	return FSlateIconFinder::FindIconForClass(URoadProfile::StaticClass()).GetIcon();
}

const FSlateBrush* UAssetDefinition_RoadProfile::GetIconBrush(const FAssetData& InAssetData, const FName InClassName) const
{
	return FSlateIconFinder::FindIconForClass(URoadProfile::StaticClass()).GetIcon();
}

#undef LOCTEXT_NAMESPACE
