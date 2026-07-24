/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "ComponentVisualizers/RoadAttributeComponentVisualizer.h"
#include "CoreMinimal.h"
#include "Algo/AnyOf.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "UnrealWidgetFwd.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "EditorViewportCommands.h"
#include "LevelEditorActions.h"
#include "Components/SplineComponent.h"
#include "ScopedTransaction.h"
#include "ActorEditorUtils.h"
#include "WorldCollision.h"
#include "Widgets/Docking/SDockTab.h"
#include "SplineGeneratorPanel.h"
#include "EngineUtils.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Math/UnrealMathUtility.h"
#include "UnrealEdGlobals.h"
#include "Assets/RoadLaneAttributeDescriptor.h"
#include "MetaRoadEditorModule.h"
#include "MetaRoadEditorSettings.h"
#include "RoadEditorCommands.h"
#include "MetaRoadEditorStyle.h"
#include "Utils/DrawUtils.h"
#include "Utils/CompVisUtils.h"
#include "Widgets/SRoadKeyEditorOverlay.h"
#include "RoadLaneAttribute.h"
#include "MetaRoadModule.h"
#include "Settings/LevelEditorViewportSettings.h"

#define LOCTEXT_NAMESPACE "FRoadAttributeComponentVisualizer"

#define LOCTEXT_STR(InKey, InTextLiteral) FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(InTextLiteral, TEXT(LOCTEXT_NAMESPACE), InKey)


// -------------------------------------------------------------------------------------------------------------------------------------------------
class FRoadAttributeComponentVisualizerCommands : public TCommands<FRoadAttributeComponentVisualizerCommands>
{
public:
	FRoadAttributeComponentVisualizerCommands() : TCommands <FRoadAttributeComponentVisualizerCommands>
	(
		"RoadAttributeComponentVisualizerCommands",	// Context name for fast lookup
		LOCTEXT("RoadAttributeComponentVisualizerCommands", "Road Attribute Component Visualizer Commands"),	// Localized context name for displaying
		NAME_None,	// Parent
		FMetaRoadEditorStyle::Get().GetStyleSetName()
	)
	{
	}

	virtual void RegisterCommands() override
	{
		UI_COMMAND(CreateAttribute, "Create Attribute", "Create a new attribute for selected lane.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(DeleteAttribute, "Delete Attribute", "Delete attribute for selected lane.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(AddAttributeKey, "Add Key", "Add new key for selected lane attribute.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(DeleteAttributeKey, "Delete Key", "Delete selected attribute key.", EUserInterfaceActionType::Button, FInputChord());
	}

	virtual ~FRoadAttributeComponentVisualizerCommands()
	{
	}

public:
	TSharedPtr<FUICommandInfo> CreateAttribute;
	TSharedPtr<FUICommandInfo> DeleteAttribute;
	TSharedPtr<FUICommandInfo> AddAttributeKey;
	TSharedPtr<FUICommandInfo> DeleteAttributeKey;
};

// -------------------------------------------------------------------------------------------------------------------------------------------------
 
FRoadAttributeComponentVisualizer::FRoadAttributeComponentVisualizer()
	: FRoadSectionComponentVisualizer()
{
	FRoadAttributeComponentVisualizerCommands::Register();

	RoadScetionComponentVisualizerActions = MakeShareable(new FUICommandList);
	SelectionState->IsKeyValid.BindLambda([Self=TWeakObjectPtr<URoadSectionComponentVisualizerSelectionState>(SelectionState)]()
	{
		const URoadSplineComponent* Component = Self->GetSelectedSpline();
		const auto& Section = Component->GetLaneSection(Self->GetSelectedSectionIndex());
		const auto& Attributes = Self->GetSelectedLaneIndex() == MetaRoad::ZeroLaneIndex ? Section.Attributes : Section.GetLaneByIndex(Self->GetSelectedLaneIndex()).Attributes;

		auto* AttributeDescriptor = Section.FindAttribute(Self->GetSelectedLaneIndex(), Self->GetSelectedAttributeDescriptor());

		if (!AttributeDescriptor)
		{
			return false;
		}
		if (!AttributeDescriptor->Keys.IsValidIndex(Self->GetSelectedKeyIndex()))
		{
			return false;
		}
		return true;
	});
}

void FRoadAttributeComponentVisualizer::OnRegister()
{
	FRoadSectionComponentVisualizer::OnRegister();

	const auto& Commands = FRoadAttributeComponentVisualizerCommands::Get();


	RoadScetionComponentVisualizerActions->MapAction(
		Commands.CreateAttribute,
		FExecuteAction::CreateSP(this, &FRoadAttributeComponentVisualizer::OnCreateAttribute),
		FCanExecuteAction::CreateLambda([this]() 
		{ 
			if(SelectionState->GetState() < ERoadSectionSelectionState::Section)
			{
				return false;
			}

			auto& AttributeDescriptor = SelectionState->GetSelectedAttributeDescriptor();
			if (!IsValid(AttributeDescriptor.Get()))
			{
				return false;
			}

			auto* DefaultObject = AttributeDescriptor->GetDefaultObject<URoadLaneAttributeDescriptor>();
			check(DefaultObject);

			return DefaultObject->CanBeAddedTo(GetEditedSplineComponent(), SelectionState->GetSelectedSectionIndex(), SelectionState->GetSelectedLaneIndex());
		}));

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.DeleteAttribute,
		FExecuteAction::CreateSP(this, &FRoadAttributeComponentVisualizer::OnDeleteAttribute),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetState() >= ERoadSectionSelectionState::Section; }));

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.AddAttributeKey,
		FExecuteAction::CreateSP(this, &FRoadAttributeComponentVisualizer::OnAddKey),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetState() >= ERoadSectionSelectionState::Section; }));

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.DeleteAttributeKey,
		FExecuteAction::CreateSP(this, &FRoadAttributeComponentVisualizer::OnDeleteKey),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetState() == ERoadSectionSelectionState::Key; }));

}

FRoadAttributeComponentVisualizer::~FRoadAttributeComponentVisualizer()
{
}

void FRoadAttributeComponentVisualizer::SetActiveAttributeDescriptor(const TSubclassOf<URoadLaneAttributeDescriptor>& AttributeDescriptor)
{
	URoadSectionComponentVisualizerSelectionState* State = GetSelectionState();
	if (!IsValid(State))
	{
		return;
	}

	// Switching to a different attribute invalidates the currently selected key (it referred to the
	// previous attribute), but the selected section/lane is attribute-independent — keep it so the user
	// stays on the same lane across attribute changes. SetSelectedAttributeDescriptor clears the key (and
	// demotes a Key selection back to Lane).
	State->SetSelectedAttributeDescriptor(AttributeDescriptor);
}

void FRoadAttributeComponentVisualizer::SelectAttributeKey(URoadSplineComponent* Spline, int32 SectionIndex, int32 LaneIndex,
	const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor, int32 KeyIndex)
{
	if (!IsValid(Spline) || !IsValid(Descriptor.Get()))
	{
		return;
	}

	URoadSectionComponentVisualizerSelectionState* State = GetSelectionState();
	if (!IsValid(State))
	{
		return;
	}

	FComponentPropertyPath Path(Spline);
	if (!Path.IsValid())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("SelectAttributeFromList", "Select Attribute"));
	State->Modify();

	State->SetSelectedSpline(Path);            // State = Component
	State->SetSelectedSection(SectionIndex);   // State = Section (lane defaults to ZeroLaneIndex)
	if (LaneIndex != MetaRoad::ZeroLaneIndex)
	{
		State->SetSelectedLane(LaneIndex);     // State = Lane
	}
	State->SetSelectedAttributeDescriptor(Descriptor);

	FRoadLaneSection& Section = Spline->GetLaneSection(SectionIndex);
	FRoadLaneAttribute* Attribute = Section.FindAttribute(LaneIndex, Descriptor);
	if (Attribute && Attribute->Keys.IsValidIndex(KeyIndex))
	{
		State->SetSelectedKeyIndex(KeyIndex);  // State = Key

		const FRoadLaneAttributeKey& Key = Attribute->Keys[KeyIndex];
		if (const FRoadLaneAttributeValue* Value = Key.GetValuePtr<FRoadLaneAttributeValue>())
		{
			const double S = Section.SOffset + Key.SOffset;
			const auto Pos = Spline->EvalAttributeAnchorPosition(SectionIndex, LaneIndex, S, *Value, ESplineCoordinateSpace::World);
			State->SetCashedData(Pos.Location, Pos.Quat, Spline->GetInputKeyValueAtDistanceAlongSpline(S));
		}
	}

	GEditor->RedrawLevelEditingViewports(true);
}

void FRoadAttributeComponentVisualizer::ConfigureKeyOverlay()
{
	TWeakObjectPtr<URoadSectionComponentVisualizerSelectionState> WeakState(SelectionState);

	struct FAttrSel
	{
		URoadSectionComponentVisualizerSelectionState* State = nullptr;
		URoadSplineComponent* Spline = nullptr;
		FRoadLaneSection* Section = nullptr;
		FRoadLaneAttribute* Attribute = nullptr;
		FRoadLaneAttributeValue* Value = nullptr;
		int32 LaneIndex = MetaRoad::ZeroLaneIndex;
		int32 KeyIndex = INDEX_NONE;
		bool IsValid() const { return Attribute != nullptr; }
	};

	auto Resolve = [WeakState]() -> FAttrSel
	{
		FAttrSel Sel;
		URoadSectionComponentVisualizerSelectionState* State = WeakState.Get();
		if (!State || State->GetStateVerified() != ERoadSectionSelectionState::Key)
		{
			return Sel;
		}
		URoadSplineComponent* Spline = State->GetSelectedSpline();
		if (!Spline)
		{
			return Sel;
		}
		const int32 LaneIndex = State->GetSelectedLaneIndex();
		const int32 KeyIndex = State->GetSelectedKeyIndex();
		FRoadLaneSection& Section = Spline->GetLaneSection(State->GetSelectedSectionIndex());
		FRoadLaneAttribute* Attribute = Section.FindAttribute(LaneIndex, State->GetSelectedAttributeDescriptor());
		if (!Attribute || !Attribute->Keys.IsValidIndex(KeyIndex))
		{
			return Sel;
		}
		Sel.State = State;
		Sel.Spline = Spline;
		Sel.Section = &Section;
		Sel.Attribute = Attribute;
		Sel.Value = Attribute->Keys[KeyIndex].GetValuePtr<FRoadLaneAttributeValue>();
		Sel.LaneIndex = LaneIndex;
		Sel.KeyIndex = KeyIndex;
		return Sel;
	};

	// Refresh the cached gizmo position for the key at NewKeyIndex (mirrors HandleInputDelta).
	auto UpdateCached = [](const FAttrSel& Sel, int32 NewKeyIndex)
	{
		FRoadLaneAttributeValue* Value = Sel.Attribute->Keys[NewKeyIndex].GetValuePtr<FRoadLaneAttributeValue>();
		if (!Value)
		{
			return;
		}
		const double S = Sel.Section->SOffset + Sel.Attribute->Keys[NewKeyIndex].SOffset;
		const auto Pos = Sel.Spline->EvalAttributeAnchorPosition(Sel.State->GetSelectedSectionIndex(), Sel.LaneIndex, S, *Value, ESplineCoordinateSpace::World);
		Sel.State->SetCashedData(Pos.Location, Pos.Quat, Sel.Spline->GetInputKeyValueAtDistanceAlongSpline(S));
	};

	auto NotifyChanged = [](URoadSplineComponent* Spline)
	{
		Spline->MarkRoadStateDirty();
		Spline->UpdateMagicTransform();
		Spline->UpdateLandscape();
		Spline->MarkRenderStateDirty();
		GEditor->RedrawLevelEditingViewports(true);
	};

	auto GetSOffset = [Resolve]() -> TOptional<double>
	{
		FAttrSel Sel = Resolve();
		if (!Sel.IsValid())
		{
			return TOptional<double>();
		}
		// Relative SOffset within the section (the stored key offset).
		return Sel.Attribute->Keys[Sel.KeyIndex].SOffset;
	};

	auto CommitSOffset = [Resolve, UpdateCached, NotifyChanged](double NewValue, ETextCommit::Type)
	{
		FAttrSel Sel = Resolve();
		if (!Sel.IsValid())
		{
			return;
		}

		const FScopedTransaction Transaction(LOCTEXT("SetAttributeKeySOffset", "Set Attribute Key SOffset"));
		Sel.Spline->Modify();
		Sel.State->Modify();

		// Input is the relative SOffset (key offset) within the section; clamp to the lane range.
		auto Rang = Sel.Spline->GetLaneRang(Sel.State->GetSelectedSectionIndex(), Sel.LaneIndex);
		const double MinSOffset = Rang.StartS - Sel.Section->SOffset;
		const double MaxSOffset = Rang.EndS - Sel.Section->SOffset;
		const double NewSOffset = FMath::Clamp(NewValue, MinSOffset, MaxSOffset);
		Sel.Attribute->Keys[Sel.KeyIndex].SOffset = NewSOffset;
		Sel.Attribute->Keys.Sort([](const FRoadLaneAttributeKey& A, const FRoadLaneAttributeKey& B) { return A.SOffset < B.SOffset; });
		const int32 NewKeyIndex = CompVisUtils::FindBestFit(Sel.Attribute->Keys, [NewSOffset](const FRoadLaneAttributeKey& It) { return FMath::Abs(NewSOffset - It.SOffset); });
		Sel.State->SetSelectedKeyIndex(NewKeyIndex);
		UpdateCached(Sel, NewKeyIndex);

		NotifyChanged(Sel.Spline);
	};

	auto GetAlpha = [Resolve]() -> TOptional<double>
	{
		FAttrSel Sel = Resolve();
		if (!Sel.IsValid() || !Sel.Value)
		{
			return TOptional<double>();
		}
		return Sel.Value->GetKeyAlpha();
	};

	auto CommitAlpha = [Resolve, UpdateCached, NotifyChanged](double NewValue, ETextCommit::Type)
	{
		FAttrSel Sel = Resolve();
		if (!Sel.IsValid() || !Sel.Value || !Sel.Value->SupportsAlphaEditing())
		{
			return;
		}

		const FScopedTransaction Transaction(LOCTEXT("SetAttributeKeyAlpha", "Set Attribute Key Alpha"));
		Sel.Spline->Modify();
		Sel.State->Modify();

		Sel.Value->SetKeyAlpha(NewValue);
		UpdateCached(Sel, Sel.KeyIndex);

		NotifyChanged(Sel.Spline);
	};

	auto AlphaRowVisibility = [Resolve]() -> EVisibility
	{
		FAttrSel Sel = Resolve();
		return (Sel.IsValid() && Sel.Value && Sel.Value->SupportsAlphaEditing())
			? EVisibility::Visible : EVisibility::Collapsed;
	};

	auto Visibility = [WeakState]() -> EVisibility
	{
		URoadSectionComponentVisualizerSelectionState* State = WeakState.Get();
		return (State && State->GetStateVerified() == ERoadSectionSelectionState::Key)
			? EVisibility::Visible : EVisibility::Collapsed;
	};

	// Header reflects the currently selected attribute: "<DisplayName> Key".
	auto HeaderText = [WeakState]() -> FText
	{
		URoadSectionComponentVisualizerSelectionState* State = WeakState.Get();
		if (State)
		{
			const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor = State->GetSelectedAttributeDescriptor();
			if (Descriptor)
			{
				if (URoadLaneAttributeDescriptor* CDO = Descriptor->GetDefaultObject<URoadLaneAttributeDescriptor>())
				{
					return FText::Format(LOCTEXT("AttributeKeyHeaderFmt", "{0} Key"), CDO->GetDisplayName());
				}
			}
		}
		return LOCTEXT("AttributeKeyHeader", "Attribute Key");
	};

	TSharedRef<SWidget> AlphaRow = RoadKeyOverlay::MakeNumericRow(
		LOCTEXT("AttributeAlpha", "Alpha"),
		TAttribute<TOptional<double>>::Create(GetAlpha),
		SNumericEntryBox<double>::FOnValueCommitted::CreateLambda(CommitAlpha),
		true,
		0.0,
		1.0);
	AlphaRow->SetVisibility(TAttribute<EVisibility>::Create(AlphaRowVisibility));

	TSharedRef<SWidget> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			RoadKeyOverlay::MakeNumericRow(
				LOCTEXT("AttributeSOffset", "S"),
				TAttribute<TOptional<double>>::Create(GetSOffset),
				SNumericEntryBox<double>::FOnValueCommitted::CreateLambda(CommitSOffset))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			AlphaRow
		];

	OverlayController = MakeShared<FRoadKeyOverlayController>();
	OverlayController->Setup(Content, TAttribute<FText>::Create(HeaderText), TAttribute<EVisibility>::Create(Visibility));
}


void FRoadAttributeComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	if (!ShouldDraw(Component))
	{
		return;
	}

	if (OverlayController)
	{
		OverlayController->EnsureAddedToActiveViewport();
	}

	auto& AttributeDescriptor = SelectionState->GetSelectedAttributeDescriptor();
	if (!AttributeDescriptor)
	{
		return;
	}

	URoadLaneAttributeDescriptor* AttributeDescriptorDefaultObject = AttributeDescriptor->GetDefaultObject<URoadLaneAttributeDescriptor>();

	const URoadSplineComponent* SplineComp = CastChecked<const URoadSplineComponent>(Component);
	const bool bIsEditingComponent = GetEditedSplineComponent() == SplineComp;

	const float GrabHandleSize = 14.0f +GetDefault<ULevelEditorViewportSettings>()->SelectedSplinePointSizeAdjustment;

	for (int32 SectionIndex = 0; SectionIndex < SplineComp->GetLaneSectionsNum(); ++SectionIndex)
	{
		const auto& Section = SplineComp->GetLaneSection(SectionIndex);
		const bool bIsSectionSelected = SelectionState->IsSelected(SplineComp, SectionIndex, MetaRoad::ZeroLaneIndex);

		// Draw not found center lane attribute
		if (!Section.FindAttribute(MetaRoad::ZeroLaneIndex, AttributeDescriptor))
		{
			PDI->SetHitProxy(new HRoadLaneAttributeVisProxy(SplineComp, SectionIndex, MetaRoad::ZeroLaneIndex, AttributeDescriptor));
			const FColor& Color = SelectionState->IsSelected(SplineComp, SectionIndex, MetaRoad::ZeroLaneIndex)
				? FMetaRoadColors::SelectedColor 
				: FMetaRoadColors::EmptyColor;
			DrawUtils::DrawLaneBorder(PDI, SplineComp, SectionIndex, MetaRoad::ZeroLaneIndex, Color, Color, SDPG_Foreground, 4.0, 0, true);
			PDI->SetHitProxy(NULL);
		}
			
		// Draw attribute lines
		for (int LaneIndex = -Section.Left.Num(); LaneIndex <= Section.Right.Num(); ++LaneIndex)
		{
			if (auto* Attribute = Section.FindAttribute(LaneIndex, AttributeDescriptor))
			{
				const bool bIsLaneSelected = SelectionState->IsSelected(SplineComp, SectionIndex, LaneIndex);

				for (int AttributeIndex = 0; AttributeIndex < Attribute->Keys.Num(); ++AttributeIndex)
				{
					const bool bIsAttributeSelected = SelectionState->IsSelected(SplineComp, SectionIndex, LaneIndex, AttributeDescriptor, AttributeIndex);

					URoadLaneAttributeDescriptor::FAttributeDrawParams Params;
					Params.PDI = PDI;
					Params.Spline = SplineComp;
					Params.Attribute = Attribute;
					Params.SectionIndex = SectionIndex;
					Params.LaneIndex = LaneIndex;
					Params.AttributeIndex = AttributeIndex;
					Params.bLowAccent = !bIsLaneSelected;
					Params.bIsAttributeSelected = bIsAttributeSelected;

					PDI->SetHitProxy(new HRoadLaneAttributeSegmentVisProxy(SplineComp, SectionIndex, LaneIndex, AttributeDescriptor, AttributeIndex));
					AttributeDescriptorDefaultObject->Draw(Params);
					PDI->SetHitProxy(NULL);

					if (bIsLaneSelected)
					{
						PDI->SetHitProxy(new HRoadLaneAttributeKeyVisProxy(SplineComp, SectionIndex, LaneIndex, AttributeDescriptor, AttributeIndex));
						AttributeDescriptorDefaultObject->DrawKey(Params);
						PDI->SetHitProxy(NULL);
					}
				}
			}
		}
	}

	if (bIsEditingComponent)
	{
		if (SelectionState->GetState() == ERoadSectionSelectionState::Section || SelectionState->GetState() == ERoadSectionSelectionState::Lane)
		{
			DrawUtils::DrawCrossSpline(PDI, SplineComp, SelectionState->GetCachedSplineKey(), FMetaRoadColors::CrossSplineColor, SDPG_Foreground);
		}
	}
}

bool FRoadAttributeComponentVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	if (VisProxy && VisProxy->Component.IsValid())
	{
		if (VisProxy->IsA(HRoadLaneAttributeSegmentVisProxy::StaticGetType()))
		{
			HRoadLaneAttributeSegmentVisProxy* Proxy = (HRoadLaneAttributeSegmentVisProxy*)VisProxy;
			const FScopedTransaction Transaction(LOCTEXT("SelectRoadSectionLaneAttributKey", "Select Road Lane Attribut Key"));
			SelectionState->Modify();

			if (const URoadSplineComponent* SplineComp = UpdateSelectedComponentAndSectionAndLane(VisProxy))
			{
				SelectionState->SetSelectedAttributeDescriptor(Proxy->AttributeDescriptor);
				SelectionState->SetSelectedKeyIndex(Proxy->AttributeIndex);

				const FRoadLaneSection& Section = SplineComp->GetLaneSection(Proxy->SectionIndex);

				if (VisProxy->IsA(HRoadLaneAttributeKeyVisProxy::StaticGetType()))
				{
					if (const FRoadLaneAttribute* Attribute = Section.FindAttribute(Proxy->LaneIndex, Proxy->AttributeDescriptor))
					{
						const auto& Key = Attribute->Keys[Proxy->AttributeIndex];
						if (auto* Value = Key.GetValuePtr<FRoadLaneAttributeValue>())
						{
							const double S = Section.SOffset + Key.SOffset;
							const auto Pos = SplineComp->EvalAttributeAnchorPosition(Proxy->SectionIndex, Proxy->LaneIndex, S, *Value, ESplineCoordinateSpace::World);
							SelectionState->SetCashedData(Pos.Location, Pos.Quat, SplineComp->GetInputKeyValueAtDistanceAlongSpline(S));
						}

					}
				}
				else
				{
					auto Rang = SplineComp->GetLaneRang(Proxy->SectionIndex, Proxy->LaneIndex);
					const float Key = SplineComp->KeyAtRayHit(Rang.StartS, Rang.EndS, Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection() * 50000.0f);
					SelectionState->SetCashedDataAtSplineInputKey(Key);
				}
			}
			GEditor->RedrawLevelEditingViewports(true);
			return true;
		}
	}

	return FRoadSectionComponentVisualizer::VisProxyHandleClick(InViewportClient, VisProxy, Click);
}

bool FRoadAttributeComponentVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	if (FRoadSectionComponentVisualizer::HandleInputDelta(ViewportClient, Viewport, DeltaTranslate, DeltaRotate, DeltaScale))
	{
		return true;
	}

	check(SelectionState);
	auto State = SelectionState->GetStateVerified();

	if (State != ERoadSectionSelectionState::Key)
	{
		return false;
	}

	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	FRoadLaneSection& Section = SplineComp->GetLaneSection(SelectionState->GetSelectedSectionIndex());
	
	const FVector WidgetLocationWorld = SelectionState->GetCashedPosition() + DeltaTranslate;
	const float ClosestKey = SplineComp->FindInputKeyClosestToWorldLocation(WidgetLocationWorld);
	const float ClosestS = SplineComp->GetDistanceAlongSplineAtSplineInputKey(ClosestKey);

	auto& AttributeDescriptor = SelectionState->GetSelectedAttributeDescriptor();
	int SectionIndex = SelectionState->GetSelectedSectionIndex();
	int LaneIndex = SelectionState->GetSelectedLaneIndex();
	int AttributeIndex = SelectionState->GetSelectedKeyIndex();

	if (auto* Attribute = Section.FindAttribute(LaneIndex, AttributeDescriptor))
	{
		auto& Key = Attribute->Keys[AttributeIndex];

		// S-axis: snap key to nearest spline point, clamped to the lane range (relative SOffset >= 0).
		auto Rang = SplineComp->GetLaneRang(SectionIndex, LaneIndex);
		const double NewSOffset = FMath::Clamp(ClosestS, Rang.StartS, Rang.EndS) - Section.SOffset;
		Key.SOffset = NewSOffset;

		// R-axis: update Alpha from lateral drag (before Sort — Key ref is valid here)
		if (auto* Value = Key.GetValuePtr<FRoadLaneAttributeValue>())
		{
			if (Value->SupportsAlphaEditing())
			{
				const FVector SplinePos     = SplineComp->GetLocationAtDistanceAlongSpline(ClosestS, ESplineCoordinateSpace::World);
				const FVector SplineTangent = SplineComp->GetTangentAtDistanceAlongSpline(ClosestS, ESplineCoordinateSpace::World).GetSafeNormal();
				// Road R-axis: (WorldUp × Tangent) = right-pointing, same convention as CalcSliceTransformAtSplineOffset
				const FVector RoadRightDir  = (FVector::UpVector ^ SplineTangent).GetSafeNormal();

				// Lateral offset of the dragged position relative to the road spline (excluding road's own R-shift)
				const double AnchorR = FVector::DotProduct(WidgetLocationWorld - SplinePos, RoadRightDir)
				                     - SplineComp->EvalROffset(ClosestS);

				const bool bIsZeroLane = (LaneIndex == MetaRoad::ZeroLaneIndex);
				double LaneInnerR, LaneOuterR;
				if (bIsZeroLane)
				{
					const auto Edges = Section.GetCenterLaneEdgeROffsets(ClosestS);
					LaneInnerR = Edges.Key;
					LaneOuterR = Edges.Value;
				}
				else
				{
					LaneInnerR = Section.EvalLaneROffset(LaneIndex, ClosestS, 0.0);
					LaneOuterR = Section.EvalLaneROffset(LaneIndex, ClosestS, 1.0);
				}

				Value->SetAlphaFromROffset(AnchorR, LaneInnerR, LaneOuterR, bIsZeroLane);
			}
		}

		// Re-sort keys and track the moved key at its new position
		Attribute->Keys.Sort([](auto& A, auto& B) { return A.SOffset < B.SOffset; });
		const int NewKeyIndex = CompVisUtils::FindBestFit(Attribute->Keys, [NewSOffset](auto& It) { return FMath::Abs(NewSOffset - It.SOffset); });
		SelectionState->SetSelectedKeyIndex(NewKeyIndex);

		// Update cached gizmo position for the moved key (centre-lane vs normal split lives in the helper).
		if (auto* Value = Attribute->Keys[NewKeyIndex].GetValuePtr<FRoadLaneAttributeValue>())
		{
			const double S = Section.SOffset + Attribute->Keys[NewKeyIndex].SOffset;
			const auto Pos = SplineComp->EvalAttributeAnchorPosition(SectionIndex, LaneIndex, S, *Value, ESplineCoordinateSpace::World);
			SelectionState->SetCashedData(Pos.Location, Pos.Quat, SplineComp->GetInputKeyValueAtDistanceAlongSpline(S));
		}
	}
		
	SplineComp->MarkRoadStateDirty();
	SplineComp->UpdateMagicTransform();
	SplineComp->MarkRenderStateDirty();
	GEditor->RedrawLevelEditingViewports(true);
	return true;
}


void FRoadAttributeComponentVisualizer::OnCreateAttribute()
{
	const TSubclassOf<URoadLaneAttributeDescriptor>& AttributeDescriptor = SelectionState->GetSelectedAttributeDescriptor();
	if (!IsValid(AttributeDescriptor.Get()))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("CreateAttribute", "Create Attribute"));
	// Default behavior: replace any existing attribute with a single key at the lane start (SOffset 0)
	// using the descriptor's value template (invalid view => template).
	AddAttributeKeyToSelection(AttributeDescriptor, TConstStructView<FRoadLaneAttributeValue>(), 0.0, /*bReplaceExisting*/ true);
}

void FRoadAttributeComponentVisualizer::OnDeleteAttribute()
{
	auto& AttributeDescriptor = SelectionState->GetSelectedAttributeDescriptor();

	if (SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Section && IsValid(AttributeDescriptor.Get()))
	{
		const FScopedTransaction Transaction(LOCTEXT("DeleteAttribute", "Delete Attribute"));

		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		SplineComp->Modify();

		int SectionIndex = SelectionState->GetSelectedSectionIndex();
		int LaneIndex = SelectionState->GetSelectedLaneIndex();
		auto& SelectedSection = GetEditedSplineComponent()->GetLaneSection(SectionIndex);
		auto& Attributes = LaneIndex == MetaRoad::ZeroLaneIndex ? SelectedSection.Attributes : SelectedSection.GetLaneByIndex(LaneIndex).Attributes;
		Attributes.Remove(AttributeDescriptor.Get());

		SplineComp->MarkRoadStateDirty();
		SplineComp->UpdateLandscape();
		GEditor->RedrawLevelEditingViewports(true);
	}
}

namespace
{
	// Collect the target lanes for a pin action: the multi-lane selection, or the primary (section, lane)
	// when the set is empty (e.g. only a section / the section centre is selected).
	TArray<FRoadSectionLaneRef> GatherTargetLanes(URoadSectionComponentVisualizerSelectionState* State)
	{
		TArray<FRoadSectionLaneRef> Targets = State->GetSelectedLanes();
		if (Targets.Num() == 0)
		{
			Targets.Emplace(State->GetSelectedSectionIndex(), State->GetSelectedLaneIndex());
		}
		return Targets;
	}
}

bool FRoadAttributeComponentVisualizer::HasAttributeOnLane(int32 SectionIndex, int32 LaneIndex, const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor)
{
	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	if (!SplineComp || !IsValid(Descriptor.Get()) || SectionIndex < 0 || SectionIndex >= SplineComp->GetLaneSectionsNum())
	{
		return false;
	}

	const TSoftClassPtr<URoadLaneAttributeDescriptor> Key(Descriptor.Get());
	if (LaneIndex == MetaRoad::ZeroLaneIndex)
	{
		return SplineComp->GetLaneSection(SectionIndex).Attributes.Contains(Key);
	}
	const FRoadLane* Lane = SplineComp->GetRoadLane(SectionIndex, LaneIndex);
	return Lane && Lane->Attributes.Contains(Key);
}

void FRoadAttributeComponentVisualizer::PinAttributeToSelectedLanes(const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor)
{
	if (SelectionState->GetStateVerified() < ERoadSectionSelectionState::Section || !IsValid(Descriptor.Get()))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("PinAttribute", "Add Attribute"));
	URoadSplineComponent* SplineComp = GetEditedSplineComponent();

	bool bAny = false;
	for (const FRoadSectionLaneRef& Ref : GatherTargetLanes(SelectionState))
	{
		// Only add where missing; leave lanes that already have the attribute (and their keys) untouched.
		if (HasAttributeOnLane(Ref.SectionIndex, Ref.LaneIndex, Descriptor))
		{
			continue;
		}
		if (AddAttributeKeyToLaneNoRefresh(Ref.SectionIndex, Ref.LaneIndex, Descriptor,
			TConstStructView<FRoadLaneAttributeValue>(), 0.0, /*bReplaceExisting*/ true) != INDEX_NONE)
		{
			bAny = true;
		}
	}

	if (bAny)
	{
		SplineComp->MarkRoadStateDirty();
		SplineComp->UpdateLandscape();
		SplineComp->MarkRenderStateDirty();
		GEditor->RedrawLevelEditingViewports(true);
	}
}

void FRoadAttributeComponentVisualizer::UnpinAttributeFromSelectedLanes(const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor)
{
	if (SelectionState->GetStateVerified() < ERoadSectionSelectionState::Section || !IsValid(Descriptor.Get()))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("UnpinAttribute", "Remove Attribute"));
	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	SplineComp->Modify();

	for (const FRoadSectionLaneRef& Ref : GatherTargetLanes(SelectionState))
	{
		if (Ref.LaneIndex == MetaRoad::ZeroLaneIndex)
		{
			SplineComp->GetLaneSection(Ref.SectionIndex).Attributes.Remove(Descriptor.Get());
		}
		else if (FRoadLane* Lane = SplineComp->GetRoadLane(Ref.SectionIndex, Ref.LaneIndex))
		{
			Lane->Attributes.Remove(Descriptor.Get());
		}
	}

	SplineComp->MarkRoadStateDirty();
	SplineComp->UpdateLandscape();
	SplineComp->MarkRenderStateDirty();
	GEditor->RedrawLevelEditingViewports(true);
}

void FRoadAttributeComponentVisualizer::OnAddKey()
{
	auto& AttributeDescriptor = SelectionState->GetSelectedAttributeDescriptor();

	if (SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Section && IsValid(AttributeDescriptor))
	{
		const FScopedTransaction Transaction(LOCTEXT("AddAttributeKey", "Add Attribute Value"));

		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		SplineComp->Modify();

		const int SectionIndex = SelectionState->GetSelectedSectionIndex();
		const int LaneIndex = SelectionState->GetSelectedLaneIndex();
		auto& SelectedSection = GetEditedSplineComponent()->GetLaneSection(SectionIndex);
		
		const double S0 = SplineComp->GetDistanceAlongSplineAtSplineInputKey(SelectionState->GetCachedSplineKey());
		auto Rang = SplineComp->GetLaneRang(SectionIndex, LaneIndex);
		const double S = S0 - Rang.StartS;

		if (S0 < Rang.StartS || S0 > Rang.EndS)
		{
			UE_LOG(LogMetaRoad, Error, TEXT("FRoadAttributeComponentVisualizer::OnAddKey() S %f not in [%f %f]"), S, Rang.StartS, Rang.EndS);
			return;
		}

		auto* Attribute = SelectedSection.FindAttribute(LaneIndex, AttributeDescriptor);
		if (!Attribute)
		{
			return;
		}

		check(Attribute->GetScriptStruct());

		const int AttributeIndex = Attribute->FindKeyBeforeOrAt(S);
		int NewAttributeIndex = INDEX_NONE;

		if (AttributeIndex == INDEX_NONE)
		{
			TArray<uint8> Memory;
			Memory.SetNum(Attribute->GetScriptStruct()->GetStructureSize());
			Attribute->GetScriptStruct()->InitializeStruct(Memory.GetData());
			NewAttributeIndex = Attribute->UpdateOrAddTypedKey(S, Memory.GetData(), Attribute->GetScriptStruct());
		}
		else
		{
			const FRoadLaneAttributeValue& TemplayeValue = Attribute->Keys[AttributeIndex].GetValue<FRoadLaneAttributeValue>();
			NewAttributeIndex = Attribute->UpdateOrAddTypedKey(S, &TemplayeValue, Attribute->GetScriptStruct());
		}
		
		if (NewAttributeIndex != INDEX_NONE)
		{
			const auto& Key = Attribute->Keys[NewAttributeIndex];

			SelectionState->Modify();
			SelectionState->SetCashedDataAtLane(SectionIndex, LaneIndex, SelectedSection.SOffset + Key.SOffset, Key.GetValue<FRoadLaneAttributeValue>().GetKeyAlpha());
			SelectionState->SetSelectedKeyIndex(NewAttributeIndex);
		}

		SplineComp->MarkRoadStateDirty();
		SplineComp->UpdateMagicTransform();
		SplineComp->MarkRenderStateDirty();
		SplineComp->UpdateLandscape();
		GEditor->RedrawLevelEditingViewports(true);
	}
}

void FRoadAttributeComponentVisualizer::OnDeleteKey()
{
	if (SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Key)
	{
		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		auto& AttributeDescriptor = SelectionState->GetSelectedAttributeDescriptor();
		const int SectionIndex = SelectionState->GetSelectedSectionIndex();
		const int LaneIndex = SelectionState->GetSelectedLaneIndex();
		const int AttributeIndex = SelectionState->GetSelectedKeyIndex();
		auto& SelectedSection = GetEditedSplineComponent()->GetLaneSection(SectionIndex);
		
		if (auto* Attribute = SelectedSection.FindAttribute(LaneIndex, AttributeDescriptor))
		{
			if (Attribute->Keys.Num() > 1)
			{
				const FScopedTransaction Transaction(LOCTEXT("DeleteAttributeKey", "Delete Attribute Key"));

				SplineComp->Modify();
				Attribute->Keys.RemoveAt(AttributeIndex);
				SelectionState->Modify();
				SelectionState->SetSelectedLane(LaneIndex);

				SplineComp->MarkRoadStateDirty();
				SplineComp->UpdateMagicTransform();
				SplineComp->MarkRenderStateDirty();
				SplineComp->UpdateLandscape();
				GEditor->RedrawLevelEditingViewports(true);
			}
		}
	}
}


void FRoadAttributeComponentVisualizer::GenerateChildContextMenuSections(FMenuBuilder& InMenuBuilder) const
{
	check(SelectionState);
	auto State = SelectionState->GetStateVerified();

	auto& AttributeDescriptor = SelectionState->GetSelectedAttributeDescriptor();

	if (State >= ERoadSectionSelectionState::Section && IsValid(AttributeDescriptor.Get()))
	{
		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		const int SelectionIndex = SelectionState->GetSelectedSectionIndex();
		const int LaneIndex = SelectionState->GetSelectedLaneIndex();
		const FRoadLaneSection& Section = SplineComp->GetLaneSection(SelectionIndex);
		const auto* Attribute = Section.FindAttribute(LaneIndex, AttributeDescriptor);

		auto* DefaultObject = AttributeDescriptor->GetDefaultObject<URoadLaneAttributeDescriptor>();
		check(DefaultObject);

		auto DisplayName = DefaultObject->GetDisplayName();

		InMenuBuilder.BeginSection("RoadLaneAttribute", FText::Format(LOCTEXT("ContextMenuRoadAttribute_Section", "Attribute - {0}"), DisplayName));
	
		if (!Attribute)
		{
			InMenuBuilder.AddMenuEntry(
				FRoadAttributeComponentVisualizerCommands::Get().CreateAttribute, 
				NAME_None, 
				LOCTEXT("ContextMenuRoadAttribute_CreateAttribute", "Create Attribute"),
				FText::Format(LOCTEXT("ContextMenuRoadAttribute_CreateAttribute_ToolTip", "Create '{0}' attribute for selected lane"), DisplayName)
			);
		}
		else
		{
			InMenuBuilder.AddMenuEntry(
				FRoadAttributeComponentVisualizerCommands::Get().DeleteAttribute,
				NAME_None,
				LOCTEXT("ContextMenuRoadAttribute_DeleteAttribute", "Delete Attribute"),
				FText::Format(LOCTEXT("ContextMenuRoadAttribute_DeleteAttribute_ToolTip", "Delete '{0}' attribute for selected lane"), DisplayName)
			);
			InMenuBuilder.AddMenuEntry(
				FRoadAttributeComponentVisualizerCommands::Get().AddAttributeKey,
				NAME_None,
				LOCTEXT("ContextMenuRoadAttribute_AddAttributeKey", "Add Key"),
				FText::Format(LOCTEXT("ContextMenuRoadAttribute_AddAttributeKey_ToolTip", "Add key for '{0}' attribute"), DisplayName)
			);
			InMenuBuilder.AddMenuEntry(
				FRoadAttributeComponentVisualizerCommands::Get().DeleteAttributeKey,
				NAME_None,
				LOCTEXT("ContextMenuRoadAttribute_DeleteAttributeKey", "Delete Key"),
				FText::Format(LOCTEXT("ContextMenuRoadAttribute_DeleteAttributeKey_ToolTip", "Delete key for '{0}' attribute"), DisplayName)
			);
		}
	
		InMenuBuilder.EndSection();
	}	
	
}

#undef LOCTEXT_NAMESPACE