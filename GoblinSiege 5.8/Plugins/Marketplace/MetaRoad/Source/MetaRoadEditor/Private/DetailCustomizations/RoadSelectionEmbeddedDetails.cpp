/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadSelectionEmbeddedDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "RoadSelectionDetails.h"
#include "RoadSplineDetails.h"
#include "RoadOffsetDetails.h"
#include "RoadSplineComponent.h"
#include "MetaRoadEditorModule.h"
#include "EditorMode/MetaRoadSelectionController.h"
#include "EditorMode/MetaRoadSelectionModel.h"
#include "ComponentVisualizers/RoadSectionComponentVisualizer.h" // URoadSectionComponentVisualizerSelectionState
#include "ComponentVisualizers/RoadOffsetComponentVisualizer.h"  // URoadOffsetComponentVisualizerSelectionState
#include "Assets/RoadLaneAttributeDescriptor.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "FRoadSelectionEmbeddedDetails"

/** Live text for the "Selection" category header: describes what is currently selected across all sub-modes.
 *  Read fresh each paint (bound via STextBlock::Text_Lambda) so it tracks spline AND intra-spline changes
 *  (lane->lane, key select, ...) without any details-view rebuild. Falls back to "Selection". */
static FText MakeSelectionTitle()
{
	FMetaRoadSelectionController& Controller = FMetaRoadSelectionController::Get();
	UMetaRoadSelectionModel* Model = Controller.GetSelectionModel();

	switch (Controller.GetRoadSelectionMode())
	{
	case ERoadSelectionMode::Spline:
		return LOCTEXT("Title_Spline", "Spline");

	case ERoadSelectionMode::Section:
	case ERoadSelectionMode::Width:
	case ERoadSelectionMode::Attribute:
	{
		URoadSectionComponentVisualizerSelectionState* S = Model ? Model->GetSectionState() : nullptr;
		if (S)
		{
			const ERoadSectionSelectionState State = S->GetStateVerified();
			switch (State)
			{
			case ERoadSectionSelectionState::Loop:
				return LOCTEXT("Title_Loop", "Looped Fill");

			case ERoadSectionSelectionState::Section:
				return FText::Format(LOCTEXT("Title_Section", "Section {0}"), S->GetSelectedSectionIndex());

			case ERoadSectionSelectionState::Lane:
			{
				if (S->GetSelectedLaneIndex() == MetaRoad::ZeroLaneIndex)
				{
					return FText::Format(LOCTEXT("Title_Section", "Section {0}"), S->GetSelectedSectionIndex());
				}
				const TArray<FRoadSectionLaneRef>& Lanes = S->GetSelectedLanes();
				if (Lanes.Num() > 1)
				{
					// Disambiguate with the section index only when the selection spans more than one section.
					TSet<int32> DistinctSections;
					for (const FRoadSectionLaneRef& Ref : Lanes) { DistinctSections.Add(Ref.SectionIndex); }
					const bool bMultiSection = DistinctSections.Num() > 1;

					FString Joined;
					for (int32 i = 0; i < Lanes.Num(); ++i)
					{
						if (i > 0) { Joined += TEXT(", "); }
						Joined += bMultiSection
							? FString::Printf(TEXT("%d:%d"), Lanes[i].SectionIndex, Lanes[i].LaneIndex)
							: FString::FromInt(Lanes[i].LaneIndex);
					}
					return FText::Format(LOCTEXT("Title_Lanes", "Lanes {0}"), FText::FromString(Joined));
				}
				return FText::Format(LOCTEXT("Title_Lane", "Lane {0}"), S->GetSelectedLaneIndex());
			}

			case ERoadSectionSelectionState::Key:
			case ERoadSectionSelectionState::KeyTangent:
			{
				const int32 KeyIndex = S->GetSelectedKeyIndex();
				if (Controller.GetRoadSelectionMode() == ERoadSelectionMode::Width)
				{
					return FText::Format(LOCTEXT("Title_WidthKey", "Width Key {0}"), KeyIndex);
				}
				URoadLaneAttributeDescriptor* Desc = S->GetSelectedAttributeDescriptor().Get()
					? S->GetSelectedAttributeDescriptor()->GetDefaultObject<URoadLaneAttributeDescriptor>()
					: nullptr;
				if (IsValid(Desc))
				{
					return FText::Format(LOCTEXT("Title_AttrKey", "{0} Key {1}"), Desc->GetDisplayName(), KeyIndex);
				}
				return FText::Format(LOCTEXT("Title_Key", "Key {0}"), KeyIndex);
			}

			default:
				break;
			}
		}
		break;
	}

	case ERoadSelectionMode::Offset:
	{
		URoadOffsetComponentVisualizerSelectionState* O = Model ? Model->GetOffsetState() : nullptr;
		if (O && O->GetSelectedKeyVerified() != INDEX_NONE)
		{
			return FText::Format(LOCTEXT("Title_OffsetKey", "Offset Key {0}"), O->GetSelectedKeyVerified());
		}
		return LOCTEXT("Title_Offset", "Offset");
	}

	default:
		break;
	}

	return LOCTEXT("Title_Selection", "Selection");
}

TSharedRef<IDetailCustomization> FRoadSelectionEmbeddedDetails::MakeInstance()
{
	return MakeShareable(new FRoadSelectionEmbeddedDetails);
}

void FRoadSelectionEmbeddedDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	if (ObjectsBeingCustomized.Num() != 1)
	{
		return;
	}

	URoadSplineComponent* Comp = Cast<URoadSplineComponent>(ObjectsBeingCustomized[0].Get());
	if (!Comp || Comp->IsTemplate())
	{
		return;
	}

	// Section/Width/Attribute read their (always-valid) selection state directly from the module-owned model, so
	// no visualizer reach-through / assert is involved. Offset/Spline construct builders that drive the live
	// visualizer's editing methods; those are safe because a sub-mode swap suppresses this refresh
	// (bUpdatingComponentVisualizer), so CustomizeDetails only runs when RoadSelectionMode and the visualizer agree.
	URoadSectionComponentVisualizerSelectionState* SectionState = FMetaRoadSelectionController::Get().GetSelectionModel()->GetSectionState();
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Selection", FText::GetEmpty(), ECategoryPriority::Important);

	// Live category title describing the current selection (replaces the static "Selection" label). Reads the
	// selection each paint, so it tracks intra-spline changes (lane->lane, key) without a details-view rebuild.
	Category.HeaderContent(
		SNew(SBox)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.TextStyle(&FAppStyle::Get(), "DetailsView.CategoryTextStyle")
			.Text_Lambda(&MakeSelectionTitle)
		],
		/*bWholeRowContent=*/ true);

	switch (FMetaRoadSelectionController::Get().GetRoadSelectionMode())
	{
	case ERoadSelectionMode::Spline:
		Category.AddCustomBuilder(MakeShareable(new FRoadSplineDetails(Comp)));
		break;
	case ERoadSelectionMode::Section:
	case ERoadSelectionMode::Width:
	case ERoadSelectionMode::Attribute:
		Category.AddCustomBuilder(MakeShareable(new FRoadSelectionDetails(Comp, SectionState, DetailBuilder)));
		break;
	case ERoadSelectionMode::Offset:
		Category.AddCustomBuilder(MakeShareable(new FRoadOffsetDetails(Comp, DetailBuilder)));
		break;
	default:
		break;
	}

	// Show only the "Selection" category: hide the component's reflected categories (Spline, Landscape,
	// Tags, ...) and the SceneComponent transform widget ("TransformCommon", added by FSceneComponentDetails).
	static const FName SelectionCategory("Selection");
	TArray<FName> CategoryNames;
	DetailBuilder.GetCategoryNames(CategoryNames);
	for (const FName& CategoryName : CategoryNames)
	{
		if (CategoryName != SelectionCategory)
		{
			DetailBuilder.HideCategory(CategoryName);
		}
	}
	DetailBuilder.HideCategory(FName("TransformCommon"));
}

#undef LOCTEXT_NAMESPACE
