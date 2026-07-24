/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "DetailCustomizations/MetaRoadActorDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailBuilderTypes.h" // FAddPropertyParams
#include "MetaRoadActor.h"
#include "Utils/MetaRoadBlueprintLibrary.h"
#include "RoadMeshBuild/MetaRoadBuildSettings.h"
#include "RoadMeshBuild/MetaRoadBuildSettingsBase.h"

#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MetaRoadActorDetails"

void MetaRoadBuildSettingsDetails::FlattenHoldersIntoCategories(IDetailLayoutBuilder& DetailBuilder, const TArray<UMetaRoadBuildSettings*>& Holders)
{
	if (Holders.Num() == 0)
	{
		return;
	}

	// HideRootObjectNode hides the expandable object/array node; CreateCategoryNodes(false) suppresses the
	// per-property-set inner category sub-node (e.g. "Mesh" / "Drive Surface") so each set's properties land
	// flat, directly under our single section category instead of nesting one level deeper.
	const FAddPropertyParams FlattenParams = FAddPropertyParams().HideRootObjectNode(true).CreateCategoryNodes(false);

	// One category per property set, in canonical pipeline order (triangulation first), driven by the single
	// schema in UMetaRoadBuildSettings. Multiple holders group same-class instances for multi-edit.
	int32 SortOrder = 0;
	for (const FMetaRoadBuildPropertySetInfo& Info : UMetaRoadBuildSettings::GetBuildPropertySetInfos())
	{
		if (!Info.PropsClass)
		{
			continue;
		}

		TArray<UObject*> SectionObjects;
		for (UMetaRoadBuildSettings* Holder : Holders)
		{
			if (Holder)
			{
				if (UMetaRoadBuildSettingsBase* PropertySet = Holder->FindOrCreatePropertySet(Info.PropsClass))
				{
					SectionObjects.Add(PropertySet);
				}
			}
		}
		if (SectionObjects.Num() == 0)
		{
			continue;
		}

		const FName CategoryName(*Info.PropsClass->GetName());
		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(CategoryName, Info.SectionName);
		Category.SetSortOrder(SortOrder++);
		Category.AddExternalObjects(SectionObjects, EPropertyLocation::Default, FlattenParams);
	}
}

// --- FMetaRoadActorDetails (build settings as a default object, edited in a separate window) ------------

TSharedRef<IDetailCustomization> FMetaRoadActorDetails::MakeInstance()
{
	return MakeShared<FMetaRoadActorDetails>();
}

void FMetaRoadActorDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Hide the raw instanced holder property: removes the class picker / "None" combo so the user can't
	// change or clear the build-settings class. It's replaced by an "Edit..." button below.
	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(AMetaRoad, BuildSettings));

	// Ensure each selected actor has a default build-settings holder (created on demand).
	TArray<TWeakObjectPtr<UObject>> CustomizedObjects;
	DetailBuilder.GetObjectsBeingCustomized(CustomizedObjects);

	TArray<TWeakObjectPtr<UMetaRoadBuildSettings>> Holders;
	TArray<TWeakObjectPtr<AMetaRoad>> RoadActors;
	for (const TWeakObjectPtr<UObject>& Object : CustomizedObjects)
	{
		if (AMetaRoad* Road = Cast<AMetaRoad>(Object.Get()))
		{
			RoadActors.Add(Road);
			if (UMetaRoadBuildSettings* Settings = UMetaRoadBuildSettings::GetForActor(Road, /*bCreateIfMissing=*/true))
			{
				Settings->EnsureDefaultPropertySets();
				Holders.Add(Settings);
			}
		}
	}

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Meta Road");
	Category.AddCustomRow(LOCTEXT("BuildSettingsRow", "Build Settings"))
	.NameContent()
	[
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(LOCTEXT("BuildSettingsLabel", "Build Settings"))
	]
	.ValueContent()
	[
		SNew(SButton)
		.HAlign(HAlign_Center)
		.ToolTipText(LOCTEXT("EditBuildSettingsTip", "Open the road build settings in a separate window."))
		.IsEnabled(Holders.Num() > 0)
		.OnClicked(FOnClicked::CreateSP(this, &FMetaRoadActorDetails::OpenBuildSettingsWindow, Holders))
		.Content()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("EditBuildSettings", "Edit..."))
		]
	];

	// "Save as Template" — creates a Blueprint from a single selected road actor (see
	// UMetaRoadBlueprintLibrary::SaveAsTemplate). Disabled for a multi-actor selection.
	const TWeakObjectPtr<AMetaRoad> SingleRoad = (RoadActors.Num() == 1) ? RoadActors[0] : nullptr;
	IDetailCategoryBuilder& UtilsCategory = DetailBuilder.EditCategory("Utils");
	UtilsCategory.AddCustomRow(LOCTEXT("SaveAsTemplateRow", "Save as Template"))
	.NameContent()
	[
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(LOCTEXT("SaveAsTemplateLabel", "Save as Template"))
	]
	.ValueContent()
	[
		SNew(SButton)
		.HAlign(HAlign_Center)
		.ToolTipText(LOCTEXT("SaveAsTemplateTip", "Save this road actor as a Blueprint template (connections preserved). Select a single actor."))
		.IsEnabled(SingleRoad.IsValid())
		.OnClicked(FOnClicked::CreateSP(this, &FMetaRoadActorDetails::OnSaveAsTemplateClicked, SingleRoad))
		.Content()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("SaveAsTemplate", "Save..."))
		]
	];
}

FReply FMetaRoadActorDetails::OpenBuildSettingsWindow(TArray<TWeakObjectPtr<UMetaRoadBuildSettings>> Holders)
{
	TArray<UObject*> Objects;
	for (const TWeakObjectPtr<UMetaRoadBuildSettings>& Holder : Holders)
	{
		if (UMetaRoadBuildSettings* Settings = Holder.Get())
		{
			Objects.Add(Settings);
		}
	}
	if (Objects.Num() == 0)
	{
		return FReply::Handled();
	}

	// Reuse an already-open window instead of stacking duplicates.
	if (TSharedPtr<SWindow> Existing = SettingsWindow.Pin())
	{
		Existing->BringToFront();
		return FReply::Handled();
	}

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs Args;
	Args.bAllowSearch = true;
	Args.bShowOptions = true;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	// The view shows the UMetaRoadBuildSettings object(s) — the registered FMetaRoadBuildSettingsDetails
	// flattens the property sets into categories automatically.
	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(Args);
	DetailsView->SetObjects(Objects);

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("BuildSettingsWindowTitle", "Road Build Settings"))
		.ClientSize(FVector2D(420.f, 640.f))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			DetailsView
		];
	SettingsWindow = Window;
	FSlateApplication::Get().AddWindow(Window);

	return FReply::Handled();
}

FReply FMetaRoadActorDetails::OnSaveAsTemplateClicked(TWeakObjectPtr<AMetaRoad> Road)
{
	if (AMetaRoad* RoadPtr = Road.Get())
	{
		UMetaRoadBlueprintLibrary::SaveAsTemplate(RoadPtr);
	}
	return FReply::Handled();
}

// --- FMetaRoadBuildSettingsDetails (holders shown directly — the Preset panel's working copies) ---------

TSharedRef<IDetailCustomization> FMetaRoadBuildSettingsDetails::MakeInstance()
{
	return MakeShared<FMetaRoadBuildSettingsDetails>();
}

void FMetaRoadBuildSettingsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Hide the raw instanced PropertySets array — re-injected flat as categories below.
	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UMetaRoadBuildSettings, PropertySets));

	TArray<TWeakObjectPtr<UObject>> CustomizedObjects;
	DetailBuilder.GetObjectsBeingCustomized(CustomizedObjects);

	TArray<UMetaRoadBuildSettings*> Holders;
	for (const TWeakObjectPtr<UObject>& Object : CustomizedObjects)
	{
		if (UMetaRoadBuildSettings* Settings = Cast<UMetaRoadBuildSettings>(Object.Get()))
		{
			Settings->EnsureDefaultPropertySets();
			Holders.Add(Settings);
		}
	}

	MetaRoadBuildSettingsDetails::FlattenHoldersIntoCategories(DetailBuilder, Holders);
}

#undef LOCTEXT_NAMESPACE
