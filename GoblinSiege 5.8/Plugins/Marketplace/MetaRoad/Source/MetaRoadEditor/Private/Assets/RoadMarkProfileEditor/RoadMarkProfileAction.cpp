/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadMarkProfileAction.h"
#include "Assets/RoadMarkProfile.h"
#include "RoadMarkProfileEditor.h"
#include "AssetTypeCategories.h"
#include "Modules/ModuleManager.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "MetaRoadEditorModule.h"
#include "SourceCodeNavigation.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "Styling/SlateIconFinder.h"

#define LOCTEXT_NAMESPACE "RoadMarkProfileAction"

FText UAssetDefinition_RoadMarkProfile::GetAssetDisplayName() const
{
	return LOCTEXT("RoadMarkProfileActionName", "Mark Profile");
}

FLinearColor UAssetDefinition_RoadMarkProfile::GetAssetColor() const
{
	return FColor(201, 29, 85);
}

TSoftClassPtr<UObject> UAssetDefinition_RoadMarkProfile::GetAssetClass() const
{
	return URoadMarkProfile::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_RoadMarkProfile::GetAssetCategories() const
{
	static FAssetCategoryPath Category(FText::FromName(FMetaRoadEditorModule::AssetCategoryName));
	static const FAssetCategoryPath Categories[] = { Category };

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	return Categories;
}

UThumbnailInfo* UAssetDefinition_RoadMarkProfile::LoadThumbnailInfo(const FAssetData& InAssetData) const
{
	return UE::Editor::FindOrCreateThumbnailInfo(InAssetData.GetAsset(), USceneThumbnailInfo::StaticClass());
}

EAssetCommandResult UAssetDefinition_RoadMarkProfile::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	TArray<UObject*> Objects = OpenArgs.LoadObjects<UObject>();
	for (UObject* Object : Objects)
	{
		URoadMarkProfile* RoadMarkProfile = Cast<URoadMarkProfile>(Object);
		if (RoadMarkProfile != nullptr)
		{
			TSharedRef<FRoadMarkProfileEditor> Editor(new FRoadMarkProfileEditor());
			Editor->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, RoadMarkProfile);
		}
	}

	return EAssetCommandResult::Handled;

}

const FSlateBrush* UAssetDefinition_RoadMarkProfile::GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const
{
	return FSlateIconFinder::FindIconForClass(URoadMarkProfile::StaticClass()).GetIcon();
}

const FSlateBrush* UAssetDefinition_RoadMarkProfile::GetIconBrush(const FAssetData& InAssetData, const FName InClassName) const
{
	return FSlateIconFinder::FindIconForClass(URoadMarkProfile::StaticClass()).GetIcon();
}

#undef LOCTEXT_NAMESPACE