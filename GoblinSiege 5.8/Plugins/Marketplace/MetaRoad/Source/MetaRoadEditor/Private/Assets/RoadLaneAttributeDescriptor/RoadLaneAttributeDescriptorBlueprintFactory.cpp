/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadLaneAttributeDescriptorBlueprintFactory.h"
#include "Assets/RoadLaneAttributeDescriptor.h"
#include "AssetActionUtility.h"
#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMisc.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/SClassPickerDialog.h"
#include "Misc/AssertionMacros.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"
#include "Widgets/SWindow.h"
#include "MetaRoadEditorModule.h"
#include "Modules/ModuleManager.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "RoadLaneAttributeDescriptorBlueprintFactory"

class FFeedbackContext;
class UObject;

class FBlutilityBlueprintFactoryFilter : public IClassViewerFilter
{
public:
	bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) override
	{
		return InClass->IsChildOf<URoadLaneAttributeDescriptor>();
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return InUnloadedClassData->IsChildOf(URoadLaneAttributeDescriptor::StaticClass());
	}
};

/////////////////////////////////////////////////////
// URoadLaneAttributeDescriptorBlueprintFactory

URoadLaneAttributeDescriptorBlueprintFactory::URoadLaneAttributeDescriptorBlueprintFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = URoadLaneAttributeDescriptorBlueprint::StaticClass();
}

bool URoadLaneAttributeDescriptorBlueprintFactory::ConfigureProperties()
{
	// Null the parent class so we can check for selection later
	ParentClass = NULL;

	// Load the class viewer module to display a class picker
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.DisplayMode = EClassViewerDisplayMode::TreeView;
	// Only want blueprint actor base classes.
	Options.bIsBlueprintBaseOnly = true;
	// This will allow unloaded blueprints to be shown.
	Options.bShowUnloadedBlueprints = true;
	Options.bEditorClassesOnly = true;

	TSharedPtr< FBlutilityBlueprintFactoryFilter > Filter = MakeShareable(new FBlutilityBlueprintFactoryFilter);
	Options.ClassFilters.Add(Filter.ToSharedRef());

	const FText TitleText = NSLOCTEXT("EditorFactories", "CreateBlueprintOptions", "Pick Parent Class");
	UClass* ChosenClass = NULL;
	const bool bPressedOk = SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, URoadLaneAttributeDescriptorBlueprint::StaticClass());
	if (bPressedOk)
	{
		ParentClass = ChosenClass;
	}

	return bPressedOk;
}

UObject* URoadLaneAttributeDescriptorBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	// Make sure we are trying to factory a blueprint, then create and init one
	check(Class->IsChildOf(UBlueprint::StaticClass()));

	EBlueprintType BPType = BPTYPE_Normal;
	if ((ParentClass == NULL) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass))
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ClassName"), (ParentClass != NULL) ? FText::FromString( ParentClass->GetName() ) : NSLOCTEXT("UnrealEd", "Null", "(null)") );
		FMessageDialog::Open( EAppMsgType::Ok, FText::Format( NSLOCTEXT("UnrealEd", "CannotCreateBlueprintFromClass", "Cannot create a blueprint based on the class '{ClassName}'."), Args ) );
		return NULL;
	}

	return FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPType, URoadLaneAttributeDescriptorBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
}

bool URoadLaneAttributeDescriptorBlueprintFactory::CanCreateNew() const
{
	return true;
}

uint32 URoadLaneAttributeDescriptorBlueprintFactory::GetMenuCategories() const
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	return AssetTools.FindAdvancedAssetCategory(FMetaRoadEditorModule::AssetCategoryName);
}

FText URoadLaneAttributeDescriptorBlueprintFactory::GetDisplayName() const
{
	return LOCTEXT("URoadLaneAttributeDescriptorBlueprintFactoryName", "Attribute Profile");
}

void URoadLaneAttributeDescriptorBlueprintFactory::OnClassPicked(UClass* InChosenClass)
{
	ParentClass = InChosenClass;
	PickerWindow.Pin()->RequestDestroyWindow();
}

FName URoadLaneAttributeDescriptorBlueprintFactory::GetNewAssetIconOverride() const
{
	return "RoadEditor.RoadLaneBuildMode";
}

#undef LOCTEXT_NAMESPACE
