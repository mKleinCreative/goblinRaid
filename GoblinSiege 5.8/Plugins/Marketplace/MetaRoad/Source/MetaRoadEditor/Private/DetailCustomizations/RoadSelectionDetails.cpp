/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadSelectionDetails.h"
#include "RoadEditorCommands.h"
#include "MetaRoadEditorModule.h"
#include "UObject/UObjectIterator.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "IDetailGroup.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "RichCurveEditorModel.h"
#include "CurveEditorCommands.h"
#include "Misc/Optional.h"
#include "CurveKeyDetails.h"
#include "Utils/PropertyEditorUtils.h"
#include "Utils/CurveUtils.h"
#include "IStructureDataProvider.h"
#include "Assets/RoadLaneAttributeDescriptor.h"
#include "ComponentVisualizers/RoadAttributeComponentVisualizer.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"

#define LOCTEXT_NAMESPACE "FRoadSelectionDetails"

// Neutral "nothing selected" row, shown when no road element is selected (or on a transient mode/visualizer desync).
static void AddNoSelectionRow(IDetailChildrenBuilder& ChildrenBuilder)
{
	ChildrenBuilder.AddCustomRow(LOCTEXT("NoneSelected", "None selected"))
	.RowTag(TEXT("NoneSelected"))
	[
		SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoneSelected", "No road elements are selected."))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
	];
}

namespace MetaRoad
{

class FInstancedStructProvider : public IStructureDataProvider
{
public:
	FInstancedStructProvider() = default;
	
	explicit FInstancedStructProvider(const TSharedPtr<IPropertyHandle>& InStructProperty)
		: StructProperty(InStructProperty)
	{
	}
	
	virtual ~FInstancedStructProvider() override {}

	void Reset()
	{
		StructProperty = nullptr;
	}
	
	virtual bool IsValid() const override
	{
		if (!StructProperty.IsValid() || !StructProperty->IsValidHandle())
		{
			return false;
		}

		bool bHasValidData = false;
		EnumerateInstances([&bHasValidData](const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)
		{
			if (ScriptStruct && Memory)
			{
				bHasValidData = true;
				return false; // Stop
			}
			return true; // Continue
		});

		return bHasValidData;
	}
	
	virtual const UStruct* GetBaseStructure() const override
	{
		// Taken from UClass::FindCommonBase
		auto FindCommonBaseStruct = [](const UScriptStruct* StructA, const UScriptStruct* StructB)
		{
			const UScriptStruct* CommonBaseStruct = StructA;
			while (CommonBaseStruct && StructB && !StructB->IsChildOf(CommonBaseStruct))
			{
				CommonBaseStruct = Cast<UScriptStruct>(CommonBaseStruct->GetSuperStruct());
			}
			return CommonBaseStruct;
		};

		const UScriptStruct* CommonStruct = nullptr;
		EnumerateInstances([&CommonStruct, &FindCommonBaseStruct](const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)
		{
			if (ScriptStruct)
			{
				CommonStruct = FindCommonBaseStruct(ScriptStruct, CommonStruct);
			}
			return true; // Continue
		});

		return CommonStruct;
	}

	virtual void GetInstances(TArray<TSharedPtr<FStructOnScope>>& OutInstances, const UStruct* ExpectedBaseStructure) const override
	{
		// The returned instances need to be compatible with base structure.
		// This function returns empty instances in case they are not compatible, with the idea that we have as many instances as we have outer objects.
		EnumerateInstances([&OutInstances, ExpectedBaseStructure](const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)
		{
			TSharedPtr<FStructOnScope> Result;
			
			if (ExpectedBaseStructure && ScriptStruct && ScriptStruct->IsChildOf(ExpectedBaseStructure))
			{
				Result = MakeShared<FStructOnScope>(ScriptStruct, Memory);
				Result->SetPackage(Package);
			}

			OutInstances.Add(Result);

			return true; // Continue
		});
	}
	
protected:

	void EnumerateInstances(TFunctionRef<bool(const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)> InFunc) const
	{
		if (!StructProperty.IsValid() || !StructProperty->IsValidHandle())
		{
			return;
		}
		
		TArray<UPackage*> Packages;
		StructProperty->GetOuterPackages(Packages);

		StructProperty->EnumerateRawData([&InFunc, &Packages](void* RawData, const int32 DataIndex, const int32 /*NumDatas*/)
		{
			const UScriptStruct* ScriptStruct = nullptr;
			uint8* Memory = nullptr;
			UPackage* Package = nullptr;
			if (FInstancedStruct* InstancedStruct = static_cast<FInstancedStruct*>(RawData))
			{
				ScriptStruct = InstancedStruct->GetScriptStruct();
				Memory = InstancedStruct->GetMutableMemory();
				if (ensureMsgf(Packages.IsValidIndex(DataIndex), TEXT("Expecting packges and raw data to match.")))
				{
					Package = Packages[DataIndex];
				}
			}

			return InFunc(ScriptStruct, Memory, Package);
		});
	}
	
	TSharedPtr<IPropertyHandle> StructProperty;
};

/**
 * Provides one FStructOnScope per selected lane, each aliasing the live FRoadLane (or its RoadZone
 * struct) memory inside the component. Feeding several instances to a single property row makes the
 * PropertyEditor edit them all at once and render differing values as "Multiple Values" natively.
 * Instances are resolved live on each query (memory addresses are not cached), like FInstancedStructProvider.
 */
class FRoadLaneStructProvider : public IStructureDataProvider
{
public:
	FRoadLaneStructProvider(URoadSplineComponent* InSpline, TArray<FRoadSectionLaneRef> InLanes, ERoadLaneStructScope InScope)
		: Spline(InSpline), Lanes(MoveTemp(InLanes)), Scope(InScope)
	{
	}

	virtual bool IsValid() const override
	{
		bool bAny = false;
		EnumerateInstances([&bAny](const UScriptStruct* ScriptStruct, uint8* Memory)
		{
			if (ScriptStruct && Memory)
			{
				bAny = true;
				return false; // Stop
			}
			return true; // Continue
		});
		return bAny;
	}

	virtual const UStruct* GetBaseStructure() const override
	{
		auto FindCommonBaseStruct = [](const UScriptStruct* StructA, const UScriptStruct* StructB)
		{
			const UScriptStruct* CommonBaseStruct = StructA;
			while (CommonBaseStruct && StructB && !StructB->IsChildOf(CommonBaseStruct))
			{
				CommonBaseStruct = Cast<UScriptStruct>(CommonBaseStruct->GetSuperStruct());
			}
			return CommonBaseStruct;
		};

		const UScriptStruct* CommonStruct = nullptr;
		EnumerateInstances([&CommonStruct, &FindCommonBaseStruct](const UScriptStruct* ScriptStruct, uint8* Memory)
		{
			if (ScriptStruct)
			{
				CommonStruct = FindCommonBaseStruct(ScriptStruct, CommonStruct);
			}
			return true; // Continue
		});
		return CommonStruct;
	}

	virtual void GetInstances(TArray<TSharedPtr<FStructOnScope>>& OutInstances, const UStruct* ExpectedBaseStructure) const override
	{
		UPackage* Package = Spline.IsValid() ? Spline->GetPackage() : nullptr;
		EnumerateInstances([&OutInstances, ExpectedBaseStructure, Package](const UScriptStruct* ScriptStruct, uint8* Memory)
		{
			TSharedPtr<FStructOnScope> Result;
			if (ExpectedBaseStructure && ScriptStruct && Memory && ScriptStruct->IsChildOf(ExpectedBaseStructure))
			{
				Result = MakeShared<FStructOnScope>(ScriptStruct, Memory);
				Result->SetPackage(Package);
			}
			OutInstances.Add(Result);
			return true; // Continue
		});
	}

protected:
	void EnumerateInstances(TFunctionRef<bool(const UScriptStruct* ScriptStruct, uint8* Memory)> InFunc) const
	{
		URoadSplineComponent* Comp = Spline.Get();
		if (!Comp)
		{
			return;
		}
		for (const FRoadSectionLaneRef& Ref : Lanes)
		{
			FRoadLane* Lane = Comp->GetRoadLane(Ref.SectionIndex, Ref.LaneIndex);
			if (!Lane)
			{
				continue;
			}

			const UScriptStruct* ScriptStruct = nullptr;
			uint8* Memory = nullptr;
			if (Scope == ERoadLaneStructScope::Lane)
			{
				ScriptStruct = FRoadLane::StaticStruct();
				Memory = reinterpret_cast<uint8*>(Lane);
			}
			else // ERoadLaneStructScope::RoadZone
			{
				ScriptStruct = Lane->RoadZone.GetScriptStruct();
				Memory = Lane->RoadZone.GetMutableMemory();
			}

			if (!InFunc(ScriptStruct, Memory))
			{
				return;
			}
		}
	}

	TWeakObjectPtr<URoadSplineComponent> Spline;
	TArray<FRoadSectionLaneRef> Lanes;
	ERoadLaneStructScope Scope;
};

} // MetaRoad

//-----------------------------------------------------------------------------------------------------------------------------

class SRoadLaneLanePicker : public SCompoundWidget
{
public:

	struct FItem
	{
		TObjectPtr<const UScriptStruct> Struct;
		FText Caption;
		FText ToolTip;
	};

	typedef typename SComboBox<TSharedPtr<FItem>>::FOnSelectionChanged FOnSelectionChanged;

	SLATE_BEGIN_ARGS(SRoadLaneLanePicker) {}
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		for (TObjectIterator<UScriptStruct> StructIt; StructIt; ++StructIt)
		{
			if (StructIt->IsChildOf(FRoadZone::StaticStruct()))
			{
				FString Caption = StructIt->GetName();
				FText ToolTip = StructIt->GetToolTipText();

				Caption.RemoveFromStart(TEXT("RoadRoadZone"), ESearchCase::CaseSensitive);
				Caption.RemoveFromStart(TEXT("RoadLane"), ESearchCase::CaseSensitive);

				if (Caption.IsEmpty())
				{
					Caption = TEXT("None");
					ToolTip = FText::FromString(TEXT("None"));
				}

				LaneTypesComboList.Add(MakeShared<FItem>(
					*StructIt,
					FText::FromString(Caption),
					ToolTip
				));
			}
		}

		ChildSlot
		[
			SAssignNew(LaneTypesComboBox, SComboBox<TSharedPtr<FItem>>)
			.OptionsSource(&LaneTypesComboList)
			.ItemStyle(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>(TEXT("TableView.Row")))
			.OnGenerateWidget_Lambda([](TSharedPtr<SRoadLaneLanePicker::FItem> Item)
			{
				return SNew(STextBlock)
					.Text(Item->Caption)
					.ToolTipText(Item->ToolTip)
					.Font(IDetailLayoutBuilder::GetDetailFont());
			})
			.OnSelectionChanged(InArgs._OnSelectionChanged)
			[
				SNew(STextBlock)
				.Text_Lambda([this](){
					if (bShowMultipleValues)
					{
						return NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
					}
					auto SelectedType = LaneTypesComboBox->GetSelectedItem();
					return (SelectedType.IsValid())
						? SelectedType->Caption
						: FText::GetEmpty();
				})
				.ToolTipText_Lambda([this]() {
					if (bShowMultipleValues)
					{
						return NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
					}
					auto SelectedType = LaneTypesComboBox->GetSelectedItem();
					return (SelectedType.IsValid())
						? SelectedType->ToolTip
						: FText::GetEmpty();
				})
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
	}

	void SetSelectedItem(const UScriptStruct* Item)
	{
		bShowMultipleValues = false;
		if (auto* Found = LaneTypesComboList.FindByPredicate([Item](const TSharedPtr<FItem>& It) { return It->Struct == Item; }))
		{
			LaneTypesComboBox->SetSelectedItem(*Found);
		}
		else
		{
			LaneTypesComboBox->ClearSelection();
		}
	}

	/** Show "Multiple Values" when the selected lanes have differing RoadZone types. */
	void SetMultipleValues()
	{
		bShowMultipleValues = true;
		LaneTypesComboBox->ClearSelection();
	}

private:
	TSharedPtr<SComboBox<TSharedPtr<FItem>>> LaneTypesComboBox;
	TArray<TSharedPtr<FItem>> LaneTypesComboList;
	bool bShowMultipleValues = false;

};

//-----------------------------------------------------------------------------------------------------------------------------


FRoadSelectionDetails::FRoadSelectionDetails(URoadSplineComponent* InOwningComponent, URoadSectionComponentVisualizerSelectionState * SelectionState,  IDetailLayoutBuilder& DetailBuilder)
	: RoadSplineComp(nullptr)
	, SelectionState(SelectionState)
{
	check(InOwningComponent);
	check(SelectionState);

	if (InOwningComponent->IsTemplate())
	{
		// For blueprints, SplineComp will be set to the preview actor in UpdateValues().
		RoadSplineComp = nullptr;
		RoadSplineCompArchetype = InOwningComponent;
	}
	else
	{
		RoadSplineComp = InOwningComponent;
		RoadSplineCompArchetype = nullptr;
	}

	TSharedPtr<IPropertyHandle> RoadLayoutProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(URoadSplineComponent, RoadLayout));
	SectionsProperty = RoadLayoutProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLayout, Sections));
	check(SectionsProperty);

	LoopedRoadZoneProperty = RoadLayoutProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLayout, LoopedRoadZone));
	LoopedRoadZoneTexAngleProperty = RoadLayoutProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLayout, LoopedRoadZoneTexAngle));
	LoopedRoadZoneTexScaleProperty = RoadLayoutProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLayout, LoopedRoadZoneTexScale));


	
}

void FRoadSelectionDetails::SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren)
{
	OnRegenerateChildren = InOnRegenerateChildren;
}

void FRoadSelectionDetails::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
}

void FRoadSelectionDetails::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	auto& Commands = FRoadEditorCommands::Get();

	const ERoadSectionSelectionState State = IsValid(SelectionState) ? SelectionState->GetStateVerified() : ERoadSectionSelectionState::None;
	ERoadSelectionMode EditorMode = FMetaRoadSelectionController::Get().GetRoadSelectionMode();

	if (State == ERoadSectionSelectionState::Loop && SelectionState->GetSelectedSpline() == RoadSplineComp)
	{
		AddLoopedRoadZoneRows(ChildrenBuilder);
	}
	else if (State >= ERoadSectionSelectionState::Section && SelectionState->GetSelectedSpline() == RoadSplineComp)
	{

		const int SectionIndex = SelectionState->GetSelectedSectionIndex();
		const int LaneIndex = SelectionState->GetSelectedLaneIndex();

		TSharedPtr<IPropertyHandle> SectionPropertyHandle = SectionsProperty->AsArray()->GetElement(SectionIndex);
		TSharedPtr<IPropertyHandle> LanePropertyHandle;

		if (State >= ERoadSectionSelectionState::Lane)
		{
			if (LaneIndex > 0)
			{
				LanePropertyHandle = SectionPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneSection, Right))->AsArray()->GetElement(LaneIndex - 1);
			}
			else if (LaneIndex < 0)
			{
				LanePropertyHandle = SectionPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneSection, Left))->AsArray()->GetElement(-LaneIndex - 1);
			}
		}

		if (State == ERoadSectionSelectionState::Section || (State == ERoadSectionSelectionState::Lane && LaneIndex == 0))
		{
			const static TSet<FName> BlackListProperties = {
				GET_MEMBER_NAME_CHECKED(FRoadLaneSection, Left),
				GET_MEMBER_NAME_CHECKED(FRoadLaneSection, Right),

				// Attributes is excluded to prevent a crash that occurs during SplitSection() (and other cases).
				// When Attributes is displayed, the engine creates FInstancedStructDataDetails for
				// each TInstancedStruct<FRoadLaneAttributeValue> (Keys[i].Value) and ticks it every
				// frame. After SplitSection() calls Trim(), Keys[0] is removed from the array, but
				// the FDetailItemNode for Keys[0].Value stays alive (held by TSharedRef). On the next
				// tick, the engine calls GetValueBaseAddress() which accesses Keys[0] via
				// ContainerPtrToValuePtr(array, 0) on an empty TArray - crashing with an access
				// violation. Attributes are editable in Attribute Mode instead.
				GET_MEMBER_NAME_CHECKED(FRoadLaneSection, Attributes),
			};

			for (TFieldIterator<FProperty> PropertyIt(FRoadLaneSection::StaticStruct()); PropertyIt; ++PropertyIt)
			{
				if (!BlackListProperties.Contains(PropertyIt->GetFName()))
				{
					ChildrenBuilder.AddProperty(SectionPropertyHandle->GetChildHandle(PropertyIt->GetFName()).ToSharedRef());
				}
			}

			// Attributes of the section centre line (ZeroLaneIndex).
			AddAttributeList(ChildrenBuilder, SectionIndex, MetaRoad::ZeroLaneIndex);
		}
		else if (State == ERoadSectionSelectionState::Lane)
		{
			if (LanePropertyHandle)
			{
				ChildrenBuilder.AddCustomRow(LOCTEXT("SelectedLane_Instance_Search", "Road Zone"))
					.NameContent()
					[
						SNew(SBox)
						.Content()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("RoadZone_Caption", "Road Zone"))
							.ToolTipText(LOCTEXT("RoadZone_ToolTip", "Data that fits into the lane"))
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
					]
					.ValueContent()
					//.MinDesiredWidth(166.0f)
					[
						SAssignNew(RoadLaneLanePicker, SRoadLaneLanePicker)
						.OnSelectionChanged_Lambda([this, LanePropertyHandle](TSharedPtr<SRoadLaneLanePicker::FItem> Selection, ESelectInfo::Type SelectionType)
						{
							if (Selection && SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Lane)
							{
								int SectionIndex = SelectionState->GetSelectedSectionIndex();
								int LaneIndex = SelectionState->GetSelectedLaneIndex();
								check(LaneIndex != 0);
								auto& Lane = RoadSplineComp->GetLaneSection(SectionIndex).GetLaneByIndex(LaneIndex);
								auto PropertHandle = LanePropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLane, RoadZone));

								if (Selection->Struct != Lane.RoadZone.GetScriptStruct())
								{
									FScopedTransaction Transaction(LOCTEXT("ChangeRoadZone", "Change Road Zone"));

									PropertHandle->NotifyPreChange();
									Lane.RoadZone.InitializeAsScriptStruct(Selection->Struct);

									// Apply the same RoadZone type to every other selected lane (multi-edit).
									RoadSplineComp->Modify();
									for (const FRoadSectionLaneRef& Ref : SelectionState->GetSelectedLanes())
									{
										if (Ref.SectionIndex == SectionIndex && Ref.LaneIndex == LaneIndex)
										{
											continue; // primary already changed above
										}
										FRoadLane* OtherLane = RoadSplineComp->GetRoadLane(Ref.SectionIndex, Ref.LaneIndex);
										if (OtherLane && OtherLane->RoadZone.GetScriptStruct() != Selection->Struct)
										{
											OtherLane->RoadZone.InitializeAsScriptStruct(Selection->Struct);
										}
									}

									RoadSplineComp->UpdateRoadLayout();
									PropertHandle->NotifyPostChange(EPropertyChangeType::ValueSet);

									RoadSplineComp->MarkRenderStateDirty();

									OnRegenerateChildren.ExecuteIfBound();
								}
							}
						})
					];

				// One instance per selected lane (falling back to the primary). Each property row spans all
				// of them: editing writes to every lane, and differing values render as "Multiple Values".
				TArray<FRoadSectionLaneRef> EditLanes = SelectionState->GetSelectedLanes();
				if (EditLanes.Num() == 0)
				{
					EditLanes.Emplace(SectionIndex, LaneIndex);
				}

				LaneStructProvider = MakeShared<MetaRoad::FRoadLaneStructProvider>(RoadSplineComp, EditLanes, ERoadLaneStructScope::Lane);
				RoadZoneStructProvider = MakeShared<MetaRoad::FRoadLaneStructProvider>(RoadSplineComp, EditLanes, ERoadLaneStructScope::RoadZone);

				// Reflect the RoadZone type in the picker; show "Multiple Values" if the selection mixes types.
				{
					const FRoadLane* PrimaryLanePtr = RoadSplineComp->GetRoadLane(SectionIndex, LaneIndex);
					const UScriptStruct* PrimaryRoadZoneType = PrimaryLanePtr ? PrimaryLanePtr->RoadZone.GetScriptStruct() : nullptr;
					bool bMixedTypes = false;
					for (const FRoadSectionLaneRef& Ref : EditLanes)
					{
						const FRoadLane* RefLane = RoadSplineComp->GetRoadLane(Ref.SectionIndex, Ref.LaneIndex);
						if (RefLane && RefLane->RoadZone.GetScriptStruct() != PrimaryRoadZoneType)
						{
							bMixedTypes = true;
							break;
						}
					}

					if (bMixedTypes)
					{
						RoadLaneLanePicker->SetMultipleValues();
					}
					else if (PrimaryRoadZoneType)
					{
						RoadLaneLanePicker->SetSelectedItem(PrimaryRoadZoneType);
					}
				}

				// RoadZone fields: iterate the common base struct so that, when types differ, only shared
				// fields are shown. Field-name agnostic — new RoadZone fields appear automatically.
				if (const UStruct* RoadZoneBase = RoadZoneStructProvider->GetBaseStructure())
				{
					for (TFieldIterator<FProperty> PropertyIt(RoadZoneBase); PropertyIt; ++PropertyIt)
					{
						if (PropertyIt->HasAnyPropertyFlags(CPF_Edit))
						{
							AddMultiLaneStructRow(ChildrenBuilder, RoadZoneStructProvider, PropertyIt->GetFName());
						}
					}
				}

				const static TSet<FName> BlackListProperties = {
					GET_MEMBER_NAME_CHECKED(FRoadLane, RoadZone),
					GET_MEMBER_NAME_CHECKED(FRoadLane, Width),
					GET_MEMBER_NAME_CHECKED(FRoadLane, PredecessorConnection),
					GET_MEMBER_NAME_CHECKED(FRoadLane, SuccessorConnection),

					// Attributes excluded for the same reason as in the Section Mode blacklist above.
					GET_MEMBER_NAME_CHECKED(FRoadLane, Attributes),
				};

				// FRoadLane fields (Direction, …) across all selected lanes. Field-name agnostic.
				for (TFieldIterator<FProperty> PropertyIt(FRoadLane::StaticStruct()); PropertyIt; ++PropertyIt)
				{
					if (!BlackListProperties.Contains(PropertyIt->GetFName()))
					{
						AddMultiLaneStructRow(ChildrenBuilder, LaneStructProvider, PropertyIt->GetFName());
					}
				}

				// Attributes attached to this lane.
				AddAttributeList(ChildrenBuilder, SectionIndex, LaneIndex);
			}
		}
		else if (State == ERoadSectionSelectionState::Key || State == ERoadSectionSelectionState::KeyTangent)
		{
			const int AttributeIndex = SelectionState->GetSelectedKeyIndex();
			if (EditorMode == ERoadSelectionMode::Width)
			{
				auto WidthPropertyHandle = LanePropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLane, Width));
				check(WidthPropertyHandle);

				TSharedPtr<IPropertyHandle> WidthKeyPropertyHandle = WidthPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRichCurve, Keys))->AsArray()->GetElement(AttributeIndex);
				if (WidthKeyPropertyHandle)
				{
					FRichCurve* RichCurve = PropertyEditorUtils::GetFirstData<FRichCurve>(WidthPropertyHandle);
					FRichCurveKey* CurveKey = PropertyEditorUtils::GetFirstData<FRichCurveKey>(WidthKeyPropertyHandle);
					if (ensure(RichCurve && CurveKey))
					{
						FKeyHandle KeyHandle = CurveUtils::GetKeyHandle(*RichCurve, AttributeIndex);
						check(KeyHandle != FKeyHandle::Invalid());
						auto CurveKeyDetails = MakeShared<MetaRoad::FCurveKeyDetails>(WidthPropertyHandle.ToSharedRef(), KeyHandle, RoadSplineComp);
						CurveKeyDetails->OnTangenModeChanged.BindLambda([this]()
						{
							if(IsValid(RoadSplineComp))
							{
								RoadSplineComp->UpdateMagicTransform();
								RoadSplineComp->MarkRenderStateDirty();
								GEditor->RedrawLevelEditingViewports(true);
							}
						});
						ChildrenBuilder.AddCustomBuilder(CurveKeyDetails);
					}

					auto TimeProperty = WidthKeyPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRichCurveKey, Time));
					TimeProperty->SetPropertyDisplayName(LOCTEXT("SelectedWidthKey_TimeLabel", "SOffset"));
					TimeProperty->SetToolTipText(LOCTEXT("SelectedWidthKey_TimeTipText", "Spline offset [cm] starts from current road section"));

					ChildrenBuilder.AddProperty(TimeProperty.ToSharedRef());
					ChildrenBuilder.AddProperty(WidthKeyPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRichCurveKey, Value)).ToSharedRef());
					//ChildrenBuilder.AddProperty(WidthKeyPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRichCurveKey, InterpMode)).ToSharedRef());
					//ChildrenBuilder.AddProperty(WidthKeyPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRichCurveKey, TangentMode)).ToSharedRef());
					//ChildrenBuilder.AddProperty(WidthKeyPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRichCurveKey, TangentWeightMode)).ToSharedRef());

					/*
					if (ensure(CurveKey && CurveKey))
					{
						AddTextRow(ChildrenBuilder,
							LOCTEXT("SelectedWidthKey_ArriveTangent_Search", "Arrive Tangent"),
							LOCTEXT("SelectedWidthKey_ArriveTangent_Name", "Arrive Tangent"),
							[CurveKey]() { return FText::AsNumber(CurveKey->ArriveTangent); }
						);
						AddTextRow(ChildrenBuilder,
							LOCTEXT("SelectedWidthKey_LeaveTangent_Search", "Leave Tangent"),
							LOCTEXT("SelectedWidthKey_LeaveTangent_Name", "Leave Tangent"),
							[CurveKey]() { return FText::AsNumber(CurveKey->LeaveTangent); }
						);
						AddTextRow(ChildrenBuilder,
							LOCTEXT("SelectedWidthKey_ArriveTangentWeight_Search", "Arrive Tangent Weight"),
							LOCTEXT("SelectedWidthKey_ArriveTangentWeight_Name", "Arrive Tangent Weight"),
							[CurveKey]() { return FText::AsNumber(CurveKey->ArriveTangentWeight); }
						);
						AddTextRow(ChildrenBuilder,
							LOCTEXT("SelectedWidthKey_LeaveTangentWeight_Search", "Leave Tangent Weight"),
							LOCTEXT("SelectedWidthKey_LeaveTangentWeight_Name", "Leave Tangent Weight"),
							[CurveKey]() { return FText::AsNumber(CurveKey->LeaveTangentWeight); }
						);
					}
					*/

				}
			}
			else if (EditorMode == ERoadSelectionMode::Attribute)
			{
				URoadLaneAttributeDescriptor* DefaultAttributeObject = SelectedAttributeDescriptor.Get() ? SelectedAttributeDescriptor->GetDefaultObject<URoadLaneAttributeDescriptor>() : nullptr;
				if (!IsValid(DefaultAttributeObject))
				{
					return;
				}

				auto AttributesPropertyHandle = LaneIndex == MetaRoad::ZeroLaneIndex
					? SectionPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneSection, Attributes)) 
					: LanePropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLane, Attributes));
				uint32 NumEl = 0;
				check(AttributesPropertyHandle->AsMap()->GetNumElements(NumEl) == FPropertyAccess::Success);
				for (uint32 ChildIndex = 0; ChildIndex < NumEl; ++ChildIndex)
				{
					TSharedPtr<IPropertyHandle> AttributePropertyHandle = AttributesPropertyHandle->GetChildHandle(ChildIndex);
					if (AttributePropertyHandle.IsValid())
					{
						TSharedPtr<IPropertyHandle> KeyProperty = AttributePropertyHandle->GetKeyHandle();
						if (KeyProperty.IsValid())
						{
							const UObject* KeyValue;
							if (KeyProperty->GetValue(KeyValue) == FPropertyAccess::Success)
							{
								if (KeyValue == SelectionState->GetSelectedAttributeDescriptor())
								{
									TSharedPtr<IPropertyHandle> KeyPropertHandle = AttributePropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneAttribute, Keys))->GetChildHandle(AttributeIndex);
									check(KeyPropertHandle);

									ChildrenBuilder.AddProperty(KeyPropertHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneAttributeKey, SOffset)).ToSharedRef());

									TSharedPtr<IPropertyHandle> ValuePropertHandle = KeyPropertHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneAttributeKey, Value));
									check(ValuePropertHandle);

									RoadLaneAttributeStruct = MakeShared<MetaRoad::FInstancedStructProvider>(ValuePropertHandle); 
									for (TFieldIterator<FProperty> PropertyIt(RoadLaneAttributeStruct->GetBaseStructure()); PropertyIt; ++PropertyIt)
									{
										if (auto* Row = ChildrenBuilder.AddExternalStructureProperty(RoadLaneAttributeStruct.ToSharedRef(), PropertyIt->GetFName()))
										{
											if (auto PropertyHandle = Row->GetPropertyHandle())
											{
												PropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([ValuePropertHandle]()
												{
													ValuePropertHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
												}));
												PropertyHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateLambda([ValuePropertHandle]()
												{
													ValuePropertHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
												}));
											}
										}
									}
									break;
								}
							}
						}
					}
				}
			}
		}
		else
		{
			// Section-mode state disagreement (e.g. a transient mode/visualizer desync) — render the neutral
			// placeholder instead of asserting.
			AddNoSelectionRow(ChildrenBuilder);
		}
	}
	else
	{
		AddNoSelectionRow(ChildrenBuilder);
	}
}



void FRoadSelectionDetails::AddAttributeList(IDetailChildrenBuilder& ChildrenBuilder, int32 SectionIndex, int32 LaneIndex)
{
	if (!RoadSplineComp || SectionIndex < 0 || SectionIndex >= RoadSplineComp->GetLaneSectionsNum())
	{
		return;
	}

	FRoadLaneSection& Section = RoadSplineComp->GetLaneSection(SectionIndex);
	const TMap<TSoftClassPtr<URoadLaneAttributeDescriptor>, FRoadLaneAttribute>& Attributes =
		(LaneIndex == MetaRoad::ZeroLaneIndex) ? Section.Attributes : Section.GetLaneByIndex(LaneIndex).Attributes;

	// Rebuild the source list.
	AttributeListItems.Reset();
	for (const auto& Pair : Attributes)
	{
		TSharedPtr<FRoadAttrListItem> Item = MakeShared<FRoadAttrListItem>();
		Item->SoftClass = Pair.Key;
		Item->ResolvedClass = Pair.Key.LoadSynchronous();
		Item->bValid = (Item->ResolvedClass.Get() != nullptr);
		AttributeListItems.Add(Item);
	}

	if (AttributeListItems.Num() == 0)
	{
		// No attributes on this lane/section — show a message instead of the list.
		ChildrenBuilder.AddCustomRow(LOCTEXT("AttributesRow_Search", "Attributes"))
		.WholeRowContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AttributesRow_Caption", "Attributes"))
				.Font(IDetailLayoutBuilder::GetDetailFontBold())
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(2.f, 4.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoAttributes", "No attributes."))
				.Font(IDetailLayoutBuilder::GetDetailFontItalic())
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];
		return;
	}

	TWeakObjectPtr<URoadSplineComponent> WeakSpline(RoadSplineComp);

	auto OnDelete = [this, WeakSpline, SectionIndex, LaneIndex](TSoftClassPtr<URoadLaneAttributeDescriptor> SoftClass)
	{
		URoadSplineComponent* Spline = WeakSpline.Get();
		if (!Spline || SectionIndex < 0 || SectionIndex >= Spline->GetLaneSectionsNum())
		{
			return;
		}
		const FScopedTransaction Transaction(LOCTEXT("DeleteAttributeFromList", "Delete Attribute"));
		Spline->Modify();

		if (LaneIndex == MetaRoad::ZeroLaneIndex)
		{
			// Section centre line: single target.
			Spline->GetLaneSection(SectionIndex).Attributes.Remove(SoftClass);
		}
		else
		{
			// Real lane: remove from every selected lane (multi-edit), falling back to this one.
			TArray<FRoadSectionLaneRef> Targets = IsValid(SelectionState) ? SelectionState->GetSelectedLanes() : TArray<FRoadSectionLaneRef>();
			if (Targets.Num() == 0)
			{
				Targets.Emplace(SectionIndex, LaneIndex);
			}
			for (const FRoadSectionLaneRef& Ref : Targets)
			{
				if (FRoadLane* Lane = Spline->GetRoadLane(Ref.SectionIndex, Ref.LaneIndex))
				{
					Lane->Attributes.Remove(SoftClass);
				}
			}
		}

		Spline->MarkRoadStateDirty();
		Spline->UpdateLandscape();
		Spline->MarkRenderStateDirty();
		GEditor->RedrawLevelEditingViewports(true);
		OnRegenerateChildren.ExecuteIfBound();
	};

	auto OnActivate = [WeakSpline, SectionIndex, LaneIndex](TSharedPtr<FRoadAttrListItem> Item)
	{
		if (!Item.IsValid() || !Item->bValid)
		{
			return;
		}
		URoadSplineComponent* Spline = WeakSpline.Get();
		if (!Spline)
		{
			return;
		}
		FMetaRoadSelectionController& Module = FMetaRoadSelectionController::Get();
		Module.SetAttributeEditorMode(Item->ResolvedClass);

		auto AttrViz = StaticCastSharedPtr<FRoadAttributeComponentVisualizer>(Module.GetComponentVisualizer());
		if (AttrViz.IsValid())
		{
			AttrViz->SelectAttributeKey(Spline, SectionIndex, LaneIndex, Item->ResolvedClass, 0);
		}
	};

	TSharedRef<SListView<TSharedPtr<FRoadAttrListItem>>> ListView =
		SNew(SListView<TSharedPtr<FRoadAttrListItem>>)
		.ListItemsSource(&AttributeListItems)
		.SelectionMode(ESelectionMode::Single)
		.OnMouseButtonDoubleClick_Lambda([OnActivate](TSharedPtr<FRoadAttrListItem> Item)
		{
			OnActivate(Item);
		})
		.OnGenerateRow_Lambda([OnDelete](TSharedPtr<FRoadAttrListItem> Item, const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>
		{
			FText NameText;
			FText TooltipText;
			const FSlateBrush* IconBrush = FAppStyle::GetBrush("Icons.Warning");
			FSlateColor TextColor = FSlateColor::UseForeground();

			if (Item->bValid)
			{
				if (URoadLaneAttributeDescriptor* CDO = Item->ResolvedClass->GetDefaultObject<URoadLaneAttributeDescriptor>())
				{
					NameText = CDO->GetDisplayName();
					TooltipText = CDO->GetToolTip();
					if (const FSlateBrush* Brush = CDO->GetIcon().GetIcon())
					{
						IconBrush = Brush;
					}
				}
			}
			else
			{
				NameText = FText::FromString(Item->SoftClass.ToString());
				TooltipText = LOCTEXT("AttrMissingDescriptor", "Attribute descriptor is missing or could not be loaded.");
				TextColor = FStyleColors::Error;
			}

			return SNew(STableRow<TSharedPtr<FRoadAttrListItem>>, OwnerTable)
			.ToolTipText(TooltipText)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.f, 1.f)
				[
					SNew(SImage)
					.Image(IconBrush)
					.ColorAndOpacity(TextColor)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center).Padding(4.f, 1.f)
				[
					SNew(STextBlock)
					.Text(NameText)
					.ColorAndOpacity(TextColor)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.f, 1.f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText(LOCTEXT("DeleteAttributeTooltip", "Delete attribute"))
					.OnClicked_Lambda([OnDelete, Item]() -> FReply
					{
						OnDelete(Item->SoftClass);
						return FReply::Handled();
					})
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Delete"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
			];
		});

	ChildrenBuilder.AddCustomRow(LOCTEXT("AttributesRow_Search", "Attributes"))
	.WholeRowContent()
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AttributesRow_Caption", "Attributes"))
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
		]
		+ SVerticalBox::Slot().AutoHeight()
		.Padding(0.f, 2.0)
		[
			ListView
		]
	];
}

uint32 FRoadSelectionDetails::ComputeSelectedLanesSignature() const
{
	uint32 Hash = 0;
	if (IsValid(SelectionState))
	{
		const TArray<FRoadSectionLaneRef>& Lanes = SelectionState->GetSelectedLanes();
		Hash = ::GetTypeHash(Lanes.Num());
		for (const FRoadSectionLaneRef& Ref : Lanes)
		{
			// Order-independent: fold per-element hashes by addition.
			Hash += HashCombine(::GetTypeHash(Ref.SectionIndex), ::GetTypeHash(Ref.LaneIndex));
		}
	}
	return Hash;
}

void FRoadSelectionDetails::AddMultiLaneStructRow(IDetailChildrenBuilder& ChildrenBuilder, const TSharedPtr<IStructureDataProvider>& Provider, FName PropertyName)
{
	IDetailPropertyRow* Row = ChildrenBuilder.AddExternalStructureProperty(Provider, PropertyName);
	if (!Row)
	{
		return;
	}
	TSharedPtr<IPropertyHandle> Handle = Row->GetPropertyHandle();
	if (!Handle.IsValid())
	{
		return;
	}

	TWeakPtr<FRoadSelectionDetails> WeakSelf = SharedThis(this);

	// Pre-change: the PropertyEditor has already opened a transaction; Modify the component so the edit
	// to the aliased lane memory is captured for undo.
	const FSimpleDelegate PreChange = FSimpleDelegate::CreateLambda([WeakSelf]()
	{
		if (TSharedPtr<FRoadSelectionDetails> Self = WeakSelf.Pin())
		{
			if (IsValid(Self->RoadSplineComp))
			{
				Self->RoadSplineComp->Modify();
			}
		}
	});

	// Post-change: PostEditChangeChainProperty/PostEditChangeProperty do not fire for external structs,
	// so reproduce what URoadSplineComponent does there for a RoadLayout edit.
	const FSimpleDelegate PostChange = FSimpleDelegate::CreateLambda([WeakSelf]()
	{
		if (TSharedPtr<FRoadSelectionDetails> Self = WeakSelf.Pin())
		{
			if (IsValid(Self->RoadSplineComp))
			{
				Self->RoadSplineComp->UpdateRoadLayout();
				Self->RoadSplineComp->UpdateMagicTransform();
				Self->RoadSplineComp->UpdateBounds();
				Self->RoadSplineComp->MarkRoadStateDirty();
				Self->RoadSplineComp->UpdateLandscape();
				Self->RoadSplineComp->MarkRenderStateDirty();
				GEditor->RedrawLevelEditingViewports(true);
			}
		}
	});

	Handle->SetOnPropertyValuePreChange(PreChange);
	Handle->SetOnChildPropertyValuePreChange(PreChange);
	Handle->SetOnPropertyValueChanged(PostChange);
	Handle->SetOnChildPropertyValueChanged(PostChange);
}

void FRoadSelectionDetails::AddLoopedRoadZoneRows(IDetailChildrenBuilder& ChildrenBuilder)
{
	if (!IsValid(RoadSplineComp) || !LoopedRoadZoneProperty.IsValid())
	{
		AddNoSelectionRow(ChildrenBuilder);
		return;
	}

	// RoadZone type picker for LoopedRoadZone (FRoadZone / Driving / Sidewalk) — mirrors the lane RoadZone picker.
	ChildrenBuilder.AddCustomRow(LOCTEXT("LoopRoadZone_Search", "Road Zone"))
		.NameContent()
		[
			SNew(SBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("RoadZone_Caption", "Road Zone"))
				.ToolTipText(LOCTEXT("LoopRoadZone_ToolTip", "Fill zone of the closed spline interior"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		]
		.ValueContent()
		[
			SAssignNew(LoopedZonePicker, SRoadLaneLanePicker)
			.OnSelectionChanged_Lambda([this](TSharedPtr<SRoadLaneLanePicker::FItem> Selection, ESelectInfo::Type)
			{
				if (Selection && IsValid(RoadSplineComp) && SelectionState->GetStateVerified() == ERoadSectionSelectionState::Loop)
				{
					FRoadLayout& Layout = RoadSplineComp->GetRoadLayout();
					if (Selection->Struct != Layout.LoopedRoadZone.GetScriptStruct())
					{
						FScopedTransaction Transaction(LOCTEXT("ChangeLoopedRoadZone", "Change Looped Road Zone"));
						LoopedRoadZoneProperty->NotifyPreChange();
						RoadSplineComp->Modify();
						Layout.LoopedRoadZone.InitializeAsScriptStruct(Selection->Struct);
						RoadSplineComp->UpdateRoadLayout();
						LoopedRoadZoneProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
						RoadSplineComp->MarkRoadStateDirty();
						RoadSplineComp->MarkRenderStateDirty();
						OnRegenerateChildren.ExecuteIfBound();
					}
				}
			})
		];

	if (const UScriptStruct* ZoneType = RoadSplineComp->GetRoadLayout().LoopedRoadZone.GetScriptStruct())
	{
		LoopedZonePicker->SetSelectedItem(ZoneType);
	}

	// Flattened RoadZone fields (field-name agnostic) over the single LoopedRoadZone instanced struct.
	LoopedZoneProvider = MakeShared<MetaRoad::FInstancedStructProvider>(LoopedRoadZoneProperty);
	if (const UStruct* ZoneBase = LoopedZoneProvider->GetBaseStructure())
	{
		for (TFieldIterator<FProperty> PropertyIt(ZoneBase); PropertyIt; ++PropertyIt)
		{
			if (PropertyIt->HasAnyPropertyFlags(CPF_Edit))
			{
				AddMultiLaneStructRow(ChildrenBuilder, LoopedZoneProvider, PropertyIt->GetFName());
			}
		}
	}

	// UV tex angle / scale (real RoadLayout handles). They affect only the baked mesh, so rebuild on change.
	TWeakPtr<FRoadSelectionDetails> WeakSelf = SharedThis(this);
	const FSimpleDelegate TexPostChange = FSimpleDelegate::CreateLambda([WeakSelf]()
	{
		if (TSharedPtr<FRoadSelectionDetails> Self = WeakSelf.Pin())
		{
			if (IsValid(Self->RoadSplineComp))
			{
				Self->RoadSplineComp->MarkRoadStateDirty();
				Self->RoadSplineComp->MarkRenderStateDirty();
				GEditor->RedrawLevelEditingViewports(true);
			}
		}
	});
	auto AddScalar = [&](const TSharedPtr<IPropertyHandle>& Handle)
	{
		if (Handle.IsValid())
		{
			ChildrenBuilder.AddProperty(Handle.ToSharedRef());
			Handle->SetOnPropertyValueChanged(TexPostChange);
		}
	};
	AddScalar(LoopedRoadZoneTexAngleProperty);
	AddScalar(LoopedRoadZoneTexScaleProperty);
}

void FRoadSelectionDetails::Tick(float DeltaTime)
{
	// If this is a blueprint spline, always update the spline component based on 
	// the spline component visualizer's currently edited spline component.
	if (RoadSplineCompArchetype)
	{
		URoadSplineComponent* EditedSplineComp = IsValid(SelectionState) ? SelectionState->GetSelectedSpline() : nullptr;

		if (!EditedSplineComp || (EditedSplineComp->GetArchetype() != RoadSplineCompArchetype))
		{
			return;
		}

		RoadSplineComp = EditedSplineComp;
	}

	if (!RoadSplineComp)
	{
		return;
	}

	bool bNeedsRebuild = false;

	int NewSectionIndex = INDEX_NONE;
	int NewLaneIndex = MetaRoad::ZeroLaneIndex;
	TSubclassOf<URoadLaneAttributeDescriptor> NewAttributeProfile = nullptr;
	int NewAttributeIndex = INDEX_NONE;

	ERoadSectionSelectionState State = SelectionState->GetStateVerified();
	if (State > ERoadSectionSelectionState::Component)
	{
		NewSectionIndex = SelectionState->GetSelectedSectionIndex();
		NewLaneIndex = SelectionState->GetSelectedLaneIndex();
	}
	if (State == ERoadSectionSelectionState::Key)
	{
		NewAttributeProfile = SelectionState->GetSelectedAttributeDescriptor();
		NewAttributeIndex = SelectionState->GetSelectedKeyIndex();
	}

	const uint32 NewLanesSignature = ComputeSelectedLanesSignature();
	const bool bNewLoopSelected = (State == ERoadSectionSelectionState::Loop);

	bNeedsRebuild = NewSectionIndex != SelectedSectiontIndex || NewLaneIndex != SelectedLaneIndex || NewAttributeProfile != SelectedAttributeDescriptor || NewAttributeIndex != SelectedKeyIndex || NewLanesSignature != SelectedLanesSignature || bNewLoopSelected != bSelectedLoopCached;

	SelectedSectiontIndex = NewSectionIndex;
	SelectedLaneIndex = NewLaneIndex;
	SelectedAttributeDescriptor = NewAttributeProfile;
	SelectedKeyIndex = NewAttributeIndex;
	SelectedLanesSignature = NewLanesSignature;
	bSelectedLoopCached = bNewLoopSelected;

	if (bNeedsRebuild)
	{
		RoadLaneAttributeStruct.Reset();
		LaneStructProvider.Reset();
		RoadZoneStructProvider.Reset();
		LoopedZoneProvider.Reset();
		OnRegenerateChildren.ExecuteIfBound();
	}
}

FName FRoadSelectionDetails::GetName() const
{
	static const FName Name("RoadSelectionDetails");
	return Name;
}


#undef LOCTEXT_NAMESPACE
