/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "SRoadLaneAttributeProfilePicker.h"
#include "DetailLayoutBuilder.h"
#include "Editor.h"
#include "IPropertyUtilities.h"
#include "PropertyCustomizationHelpers.h"
#include "Modules/ModuleManager.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "MetaRoadEditorModule.h"
#include "Assets/MetaRoadPreset_DEPRECATED.h"
#include "Assets/RoadLaneAttributeDescriptor.h"
#include "Utils/AssetUtils.h"

#define LOCTEXT_NAMESPACE "SRoadLaneAttributeProfilePicker"


void SRoadLaneAttributeProfilePicker::Construct(const FArguments& InArgs, TSharedPtr<IPropertyHandle> RoadLaneAttributeProfileProperty, TSharedPtr<IPropertyUtilities> InPropertyUtils)
{
	//OnStructPicked = InArgs._OnStructPicked;
	PropUtils = MoveTemp(InPropertyUtils);
	if (!RoadLaneAttributeProfileProperty.IsValid() || !PropUtils.IsValid())
	{
		return;
	}

	AttributeValueProperty = RoadLaneAttributeProfileProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneAttributeProfile, AttributeValueTemplate));
	AttributeDesctiptorProperty = RoadLaneAttributeProfileProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneAttributeProfile, AttributeDesctiptor));
	if (!AttributeValueProperty.IsValid() || !AttributeDesctiptorProperty.IsValid())
	{
		return;
	}

	FString AttributeClassPath;
	AttributeDesctiptorProperty->GetValueAsFormattedString(AttributeClassPath);
	TSoftClassPtr<URoadLaneAttributeDescriptor> AttributeDesctiptor(AttributeClassPath);

	SetComboBoxContent(AttributeDesctiptor.Get());

	ChildSlot
	[
		SAssignNew(ComboButton, SComboButton)
		.OnGetMenuContent(this, &SRoadLaneAttributeProfilePicker::GenerateStructPicker)
		.ContentPadding(0)
		.IsEnabled(AttributeValueProperty->IsEditable())
		.ButtonContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SImage)
				.Image_Lambda([this]()
				{ 
					return ComboBoxContent.Icon.GetIcon();
				})
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() 
				{ 
					return ComboBoxContent.Lable; 
				})
				.ToolTipText_Lambda([this]() { return ComboBoxContent.Tooltip; })
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		]
	];
}


TSharedRef<SWidget> SRoadLaneAttributeProfilePicker::GenerateStructPicker()
{
	FMenuBuilder MenuBuilder(true, nullptr, TSharedPtr<FExtender>(), false, &FAppStyle::Get());
	MenuBuilder.BeginSection(NAME_None);

	TSet<UClass*> Attributes = AssetUtils::GetAllClassesOfSubClass(URoadLaneAttributeDescriptor::StaticClass());
	for (auto& Attribute : Attributes)
	{
		auto* DefaultObject = Attribute->GetDefaultObject<URoadLaneAttributeDescriptor>();
		check(DefaultObject);

		MenuBuilder.AddMenuEntry(
			DefaultObject->GetDisplayName(),
			DefaultObject->GetToolTip(),
			DefaultObject->GetIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, Attribute]() { StructPicked(Attribute); })),
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}

	MenuBuilder.EndSection();

	return SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(500)
			[
				MenuBuilder.MakeWidget()
			]
		];
}

void SRoadLaneAttributeProfilePicker::StructPicked(const TSubclassOf<URoadLaneAttributeDescriptor>& Attribute)
{
	if (AttributeValueProperty && AttributeValueProperty->IsValidHandle() && AttributeDesctiptorProperty && AttributeDesctiptorProperty->IsValidHandle())
	{
		FScopedTransaction Transaction(LOCTEXT("OnStructPicked", "Set Struct"));

		AttributeValueProperty->NotifyPreChange();
		AttributeValueProperty->EnumerateRawData([&Attribute](void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
		{
			const URoadLaneAttributeDescriptor* DefailtAttribute = Attribute ? Attribute->GetDefaultObject<URoadLaneAttributeDescriptor>() : nullptr;

			if (auto* InstancedStruct = static_cast<TInstancedStruct<FRoadLaneAttributeValue>*>(RawData))
			{
				*InstancedStruct = TInstancedStruct<FRoadLaneAttributeValue>(DefailtAttribute->GetAttributeValueTemplate());
			}
			return true;
		});

		if (Attribute)
		{
			AttributeDesctiptorProperty->SetValueFromFormattedString(Attribute->GetPathName());
		}

		SetComboBoxContent(Attribute);

		// Property tree will be invalid after changing the struct type, force update.
		if (PropUtils.IsValid())
		{
			PropUtils->ForceRefresh();
		}
	}

	ComboButton->SetIsOpen(false);
	//OnStructPicked.ExecuteIfBound(InStruct);
}

void SRoadLaneAttributeProfilePicker::SetComboBoxContent(const TSubclassOf<URoadLaneAttributeDescriptor>& Attribute)
{
	const URoadLaneAttributeDescriptor* DefailtAttribute = Attribute ? Attribute->GetDefaultObject<URoadLaneAttributeDescriptor>() : nullptr;

	if (DefailtAttribute)
	{
		ComboBoxContent.Lable = DefailtAttribute->GetDisplayName();
		ComboBoxContent.Tooltip = DefailtAttribute->GetToolTip();
		ComboBoxContent.Icon = DefailtAttribute->GetIcon();
	}
	else
	{
		ComboBoxContent.Lable = LOCTEXT("AttributeEmpty_Lable", "Empty");
		ComboBoxContent.Tooltip = LOCTEXT("AttributeEmpty_ToolTip", "Attribute isn't set");
		ComboBoxContent.Icon = FSlateIcon();
	}
}

#undef LOCTEXT_NAMESPACE
