/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadZoneTypeDetails.h"
#include "MetaRoadSettings.h"
#include "MetaRoadTypes.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Images/SImage.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "IPropertyUtilities.h"
#include "IDetailPropertyRow.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "Modules/ModuleManager.h"
#include "ISettingsModule.h"


#define LOCTEXT_NAMESPACE "RoadZoneTypeDetails"

TSharedRef<IPropertyTypeCustomization> FRoadZoneTypeCustomization::MakeInstance()
{
	return MakeShareable(new FRoadZoneTypeCustomization);
}

void FRoadZoneTypeCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructProperty = StructPropertyHandle;
	PropUtils = StructCustomizationUtils.GetPropertyUtilities().Get();
	TypeNameProperty = StructProperty->GetChildHandle(TEXT("TypeName"));
	check(TypeNameProperty);

	//UE::ZoneGraphDelegates::OnRoadZoneTypesChanged.AddSP(this, &FRoadZoneTypeCustomization::CacheZoneTypes);
	CacheZoneTypes();

	HeaderRow
	.NameContent()
	[
		StructProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(200.0f)
	[
		SNew(SComboButton)
		.OnGetMenuContent(this, &FRoadZoneTypeCustomization::OnGetComboContent)
		.ContentPadding(FMargin(2.0f, 0.0f))
		.ButtonContent()
		[
			SNew(SHorizontalBox)
			// Color
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.MaxWidth(25)
			.VAlign(VAlign_Center)
			[
				SNew(SColorBlock)
				.Color(this, &FRoadZoneTypeCustomization::GetColor)
				.Size(FVector2D(16.0f, 16.0f))
			]
			// Description
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(FMargin(6.0f, 2.0f))
			[
				SNew(STextBlock)
				.Text(this, &FRoadZoneTypeCustomization::GetDescription)
				.Font(IDetailLayoutBuilder::GetDetailFontBold())
				.ToolTipText(this, &FRoadZoneTypeCustomization::GetToolTip)
			]
		]
	];
}

void FRoadZoneTypeCustomization::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

void FRoadZoneTypeCustomization::CacheZoneTypes()
{
	ZoneTypes.Reset();
	if (const UMetaRoadSettings* MetaRoadSettings = GetDefault<UMetaRoadSettings>())
	{
		ZoneTypes = MetaRoadSettings->RoadZoneTypes;
	}
}

FLinearColor FRoadZoneTypeCustomization::GetColor() const
{
	FName TypeName = NAME_None;
	if (TypeNameProperty->GetValue(TypeName) == FPropertyAccess::Success)
	{
		FLinearColor Color = FLinearColor::Black;
		if(auto* Found = ZoneTypes.Find(TypeName))
		{
			Color = FLinearColor(Found->EditorColor);
		}
		return Color;
	}
	return FLinearColor::Transparent;
}

FText FRoadZoneTypeCustomization::GetDescription() const
{
	FName TypeName = NAME_None;
	if (TypeNameProperty->GetValue(TypeName) == FPropertyAccess::Success)
	{
		return FText::FromName(TypeName);
	}
	return FText::GetEmpty();
}

FText FRoadZoneTypeCustomization::GetToolTip() const
{
	FName TypeName = NAME_None;
	if (TypeNameProperty->GetValue(TypeName) == FPropertyAccess::Success)
	{
		if (auto* Found = ZoneTypes.Find(TypeName))
		{
			return FText::FromString(Found->Description);
		}
	}
	return FText();
}

TSharedRef<SWidget> FRoadZoneTypeCustomization::OnGetComboContent() const
{
	FMenuBuilder MenuBuilder(true, nullptr);

	FUIAction EditTagsItemAction(FExecuteAction::CreateSP(const_cast<FRoadZoneTypeCustomization*>(this), &FRoadZoneTypeCustomization::OnEditZoneTypes));
	MenuBuilder.AddMenuEntry(LOCTEXT("EditEditZoneTypes", "Edit Lanes Types..."), TAttribute<FText>(), FSlateIcon(), EditTagsItemAction);
	MenuBuilder.AddMenuSeparator();

	for (const auto& [Key, Value] : ZoneTypes)
	{

		FUIAction BitItemAction(FExecuteAction::CreateSP(const_cast<FRoadZoneTypeCustomization*>(this), &FRoadZoneTypeCustomization::OnSetZoneType, Key));

		TSharedRef<SWidget> Widgets = SNew(SHorizontalBox)
			// Color
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.MaxWidth(25)
			.VAlign(VAlign_Center)
			[
				SNew(SColorBlock)
				.Color(Value.EditorColor)
				.Size(FVector2D(16.0f, 16.0f))
			]
			// Description
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(FMargin(6.0f, 2.0f))
			[
				SNew(STextBlock)
				.Text(FText::FromName(Key))
				.ToolTipText(FText::FromString(Value.Description))
			];


		MenuBuilder.AddMenuEntry(BitItemAction, Widgets);
	}

	return MenuBuilder.MakeWidget();
}

void FRoadZoneTypeCustomization::OnEditZoneTypes()
{
	if (const UMetaRoadSettings* MetaRoadSettings = GetDefault<UMetaRoadSettings>())
	// Goto settings to edit tags
	FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer(MetaRoadSettings->GetContainerName(), MetaRoadSettings->GetCategoryName(), MetaRoadSettings->GetSectionName());
}

void FRoadZoneTypeCustomization::OnSetZoneType(FName ZoneType)
{
	TypeNameProperty->SetValue(ZoneType);
}

#undef LOCTEXT_NAMESPACE