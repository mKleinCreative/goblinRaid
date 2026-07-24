/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadSplineComponentDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "RoadSplineComponent.h"
#include "MetaRoadEditorModule.h"
#include "DetailWidgetRow.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "RoadSplineComponentDetails"

TSharedRef<IDetailCustomization> FRoadSplineComponentDetails::MakeInstance()
{
	return MakeShareable(new FRoadSplineComponentDetails);
}

void FRoadSplineComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Hide the SplineCurves property
	//TSharedPtr<IPropertyHandle> SplineCurvesProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USplineComponent, SplineCurves));
	//SplineCurvesProperty->MarkHiddenByCustomization();


	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

	if (ObjectsBeingCustomized.Num() == 1)
	{
		if (URoadSplineComponent* Comp = Cast<URoadSplineComponent>(ObjectsBeingCustomized[0]))
		{
			if (!Comp->IsTemplate())
			{
				// SubGroup — editable text field + dropdown of existing sub-group names from sibling splines.
				// The road-spline "Selection" sub-mode editor lives in the mode toolkit's left panel now
				// (FRoadSelectionEmbeddedDetails), so the main Details only keeps this row.
				TSharedRef<IPropertyHandle> SubGroupHandle = DetailBuilder.GetProperty(
					GET_MEMBER_NAME_CHECKED(URoadSplineComponent, SubGroup));
				SubGroupHandle->MarkHiddenByCustomization();

				IDetailCategoryBuilder& RoadCategory = DetailBuilder.EditCategory("Road");

				// Build the widget before the AddCustomRow chain.
				// [&]() immediately-invoked lambda inside Slate operator[] is parsed by MSVC
				// as a C++ attribute specifier (C3750), so we construct the tree here instead.
				struct FSubGroupState { TSharedPtr<SMenuAnchor> Anchor; };
				TSharedPtr<FSubGroupState> SubGroupState = MakeShared<FSubGroupState>();

				auto GetGroupMenuContent = [Comp, SubGroupHandle]() -> TSharedRef<SWidget>
				{
					TArray<FName> Groups;
					if (AActor* Owner = Comp->GetOwner())
					{
						TArray<URoadSplineComponent*> Siblings;
						Owner->GetComponents(Siblings);
						for (auto* Sib : Siblings)
							if (!Sib->SubGroup.IsNone())
								Groups.AddUnique(Sib->SubGroup);
					}
					FMenuBuilder MenuBuilder(true, nullptr);
					MenuBuilder.AddMenuEntry(
						LOCTEXT("GroupNone", "(none)"), FText::GetEmpty(), FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([SubGroupHandle]()
						{
							SubGroupHandle->SetValue(NAME_None);
						})));
					if (!Groups.IsEmpty())
					{
						MenuBuilder.AddSeparator();
						for (const FName& ID : Groups)
						{
							MenuBuilder.AddMenuEntry(
								FText::FromName(ID), FText::GetEmpty(), FSlateIcon(),
								FUIAction(FExecuteAction::CreateLambda([SubGroupHandle, ID]()
								{
									SubGroupHandle->SetValue(ID);
								})));
						}
					}
					else
					{
						MenuBuilder.AddWidget(
							SNew(SBox).HAlign(HAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("NoGroups", "No sub-groups defined yet"))
								.Font(IDetailLayoutBuilder::GetDetailFont())
							],
							FText::GetEmpty());
					}
					return MenuBuilder.MakeWidget();
				};

				TSharedRef<SMenuAnchor> SubGroupWidget = SNew(SMenuAnchor)
					.Placement(MenuPlacement_ComboBox)
					.OnGetMenuContent_Lambda(GetGroupMenuContent)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SNew(SEditableTextBox)
							.Font(IDetailLayoutBuilder::GetDetailFont())
							.Text_Lambda([SubGroupHandle]()
							{
								FName Value;
								SubGroupHandle->GetValue(Value);
								return Value.IsNone() ? FText::GetEmpty() : FText::FromName(Value);
							})
							.OnTextCommitted_Lambda([SubGroupHandle](const FText& Text, ETextCommit::Type)
							{
								FString Str = Text.ToString().TrimStartAndEnd();
								SubGroupHandle->SetValue(Str.IsEmpty() ? NAME_None : FName(*Str));
							})
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Fill).Padding(FMargin(2, 0))
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.OnClicked_Lambda([SubGroupState]() -> FReply
							{
								if (SubGroupState->Anchor.IsValid())
									SubGroupState->Anchor->SetIsOpen(!SubGroupState->Anchor->IsOpen());
								return FReply::Handled();
							})
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("ComboButton.Arrow"))
								.ColorAndOpacity(FSlateColor::UseForeground())
							]
						]
					];
				SubGroupState->Anchor = SubGroupWidget;

				RoadCategory.AddCustomRow(LOCTEXT("SubGroup", "Sub Group"))
				.NameContent()
				[
					SubGroupHandle->CreatePropertyNameWidget()
				]
				.ValueContent()
				.MaxDesiredWidth(TOptional<float>())
				[
					SubGroupWidget
				];
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
