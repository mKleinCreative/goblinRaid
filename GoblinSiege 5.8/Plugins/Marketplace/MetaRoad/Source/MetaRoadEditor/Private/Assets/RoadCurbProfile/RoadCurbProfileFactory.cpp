/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */


#include "RoadCurbProfileFactory.h"
#include "MetaRoadEditorModule.h"
#include "Modules/ModuleManager.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Assets/RoadCurbProfile.h"
#include "Assets/RoadMarkProfile.h"

#define LOCTEXT_NAMESPACE "RoadCurbProfileFactory"

URoadCurbProfileFactory::URoadCurbProfileFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = URoadCurbProfile::StaticClass();
}

UObject* URoadCurbProfileFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<URoadCurbProfile>(InParent, Class, Name, Flags | RF_Transactional);;
}

FText URoadCurbProfileFactory::GetDisplayName() const
{
	return LOCTEXT("RoadCurbProfileFactoryName", "Curb Profile");
}

uint32 URoadCurbProfileFactory::GetMenuCategories() const
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	return AssetTools.FindAdvancedAssetCategory(FMetaRoadEditorModule::AssetCategoryName);
}


#undef LOCTEXT_NAMESPACE
