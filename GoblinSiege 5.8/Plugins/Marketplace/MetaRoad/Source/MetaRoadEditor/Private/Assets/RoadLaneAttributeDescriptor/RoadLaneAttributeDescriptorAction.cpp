/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadLaneAttributeDescriptorAction.h"
#include "Assets/RoadLaneAttributeDescriptor.h"
#include "AssetTypeCategories.h"
#include "Modules/ModuleManager.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "MetaRoadEditorModule.h"

#define LOCTEXT_NAMESPACE "RoadLaneAttributeDescriptorAction"


FText UAssetDefinition_RoadLaneAttributeDescriptor::GetAssetDisplayName() const
{
	return LOCTEXT("RoadLaneAttributeDescriptorActionName", "Attribute Profile");
}

FText UAssetDefinition_RoadLaneAttributeDescriptor::GetAssetDisplayName(const FAssetData& AssetData) const
{
	FString NativeParentClassPath;
	AssetData.GetTagValue(FBlueprintTags::NativeParentClassPath, NativeParentClassPath);

	UClass* ParentClass = nullptr;
	if (!NativeParentClassPath.IsEmpty())
	{
		UObject* Outer = nullptr;
		ResolveName(Outer, NativeParentClassPath, false, false);
		ParentClass = FindObject<UClass>(Outer, *NativeParentClassPath);
	}

	URoadLaneAttributeDescriptor* CDO = nullptr;
	if (ParentClass)
	{
		CDO = Cast<URoadLaneAttributeDescriptor>(ParentClass->GetDefaultObject());
	}

	if (CDO)
	{
		return CDO->GetAssetDisplayName();
	}

	return Super::GetAssetDisplayName(AssetData);
}

FText UAssetDefinition_RoadLaneAttributeDescriptor::GetAssetDescription(const FAssetData& AssetData) const
{
	return Super::GetAssetDescription(AssetData);
}

FLinearColor UAssetDefinition_RoadLaneAttributeDescriptor::GetAssetColor() const
{
	return FColor(201, 29, 85);
}

TSoftClassPtr<UObject> UAssetDefinition_RoadLaneAttributeDescriptor::GetAssetClass() const
{
	return URoadLaneAttributeDescriptorBlueprint::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_RoadLaneAttributeDescriptor::GetAssetCategories() const
{
	static FAssetCategoryPath Category(FText::FromName(FMetaRoadEditorModule::AssetCategoryName));
	static const FAssetCategoryPath Categories[] = { Category };

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	return Categories;
}


EAssetCommandResult UAssetDefinition_RoadLaneAttributeDescriptor::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	bool bIsLoftBP = false;
	TArray<UBlueprint*> Objects = OpenArgs.LoadObjects<UBlueprint>();
	for (UBlueprint* Object : Objects)
	{
		URoadLaneAttributeDescriptor* Descriptor = Cast<URoadLaneAttributeDescriptor>(Object->GeneratedClass->GetDefaultObject());

		if (Descriptor != nullptr)
		{
			if(Descriptor->OpenCustomAssetEditor(OpenArgs))
			{
				bIsLoftBP = true;
			}
		}
	}

	if (bIsLoftBP)
	{
		return EAssetCommandResult::Handled;
	}
	else
	{
		return Super::OpenAssets(OpenArgs);
	}

}

#undef LOCTEXT_NAMESPACE
