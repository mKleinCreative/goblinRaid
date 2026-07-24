/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "ComponentVisualizers/RoadSectionComponentVisualizer.h"
#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
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
#include "MetaRoadEditorModule.h"
#include "EditorMode/MetaRoadSelectionModel.h"
#include "MetaRoadEditorSettings.h"
#include "RoadEditorCommands.h"
#include "Utils/DrawUtils.h"
#include "Utils/CompVisUtils.h"
#include "Widgets/SRoadKeyEditorOverlay.h"
#include "MetaRoadEditorStyle.h"
#include "Assets/RoadProfileEditor/RoadProfileFactory.h"
#include "Assets/RoadProfile.h"
#include "AssetToolsModule.h"
#include "Factories/Factory.h"
#include "Settings/LevelEditorViewportSettings.h"

#define LOCTEXT_NAMESPACE "FRoadSectionComponentVisualizer"

#define LOCTEXT_STR(InKey, InTextLiteral) FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(InTextLiteral, TEXT(LOCTEXT_NAMESPACE), InKey)

DEFINE_LOG_CATEGORY_STATIC(LogSectionSplineComponentVisualizer, Log, All)


// -------------------------------------------------------------------------------------------------------------------------------------------------

void URoadSectionComponentVisualizerSelectionState::SetCashedData(const FVector& Position, const FQuat& Rotation, float SplineKey)
{
	CahedPosition = Position;
	CachedRotation = Rotation;
	CashedSplineKey = SplineKey;
}

void URoadSectionComponentVisualizerSelectionState::SetCashedDataAtSplineDistance(float SOffset)
{
	const URoadSplineComponent* SplineComp = GetSelectedSpline();
	check(SplineComp);
	float Key = SplineComp->GetInputKeyValueAtDistanceAlongSpline(SOffset);
	auto Pos = SplineComp->GetRoadPosition(SOffset, SplineComp->RoadLayout.ROffset.Eval(SOffset), ESplineCoordinateSpace::World);
	SetCashedData(Pos.Location, Pos.Quat, Key);
}

void URoadSectionComponentVisualizerSelectionState::SetCashedDataAtSplineInputKey(float Key)
{
	const URoadSplineComponent* SplineComp = GetSelectedSpline();
	check(SplineComp);
	float S = SplineComp->GetDistanceAlongSplineAtSplineInputKey(Key);
	auto Pos = SplineComp->GetRoadPosition(S, SplineComp->RoadLayout.ROffset.Eval(S), ESplineCoordinateSpace::World);
	SetCashedData(Pos.Location, Pos.Quat, Key);
}

void URoadSectionComponentVisualizerSelectionState::SetCashedDataAtLane(int SectionIndex, int LaneIndex, double SOffset, double Aplha)
{
	const URoadSplineComponent* SplineComp = GetSelectedSpline();
	check(SplineComp);
	auto Pos = SplineComp->GetRoadPosition(SectionIndex, LaneIndex, Aplha, SOffset, ESplineCoordinateSpace::World);
	float Key = SplineComp->GetInputKeyValueAtDistanceAlongSpline(SOffset);
	SetCashedData(Pos.Location, Pos.Quat, Key);
}

void URoadSectionComponentVisualizerSelectionState::ResetCahedData()
{
	CachedRotation = FQuat();
	CahedPosition = FVector::ZeroVector;
	CashedSplineKey = 0;
}

void URoadSectionComponentVisualizerSelectionState::UpdateSplineSelection() const
{
	if (URoadSplineComponent* Component = GetSelectedSpline())
	{
		Component->SetSelectedLane(SelectedSectionIndex, SelectedLaneIndex);

		TArray<TTuple<int, int>> Lanes;
		Lanes.Reserve(SelectedLanes.Num());
		for (const FRoadSectionLaneRef& Lane : SelectedLanes)
		{
			Lanes.Emplace(Lane.SectionIndex, Lane.LaneIndex);
		}
		Component->SetSelectedLanes(Lanes);

		Component->SetLoopSelected(State == ERoadSectionSelectionState::Loop);
	}
}

void URoadSectionComponentVisualizerSelectionState::PruneSelectedLanes()
{
	URoadSplineComponent* Component = GetSelectedSpline();
	if (!Component)
	{
		SelectedLanes.Reset();
		return;
	}

	SelectedLanes.RemoveAll([Component](const FRoadSectionLaneRef& Lane)
	{
		if (Lane.SectionIndex < 0 || Lane.SectionIndex >= Component->GetLaneSectionsNum())
		{
			return true;
		}
		if (Lane.LaneIndex == MetaRoad::ZeroLaneIndex)
		{
			return true;
		}
		return !Component->GetLaneSection(Lane.SectionIndex).CheckLaneIndex(Lane.LaneIndex);
	});
}

void URoadSectionComponentVisualizerSelectionState::ToggleLane(int32 SectionIndex, int32 LaneIndex)
{
	check(SplinePropertyPath.IsValid());

	if (LaneIndex == MetaRoad::ZeroLaneIndex)
	{
		return;
	}

	const FRoadSectionLaneRef Ref(SectionIndex, LaneIndex);
	const int32 Existing = SelectedLanes.IndexOfByKey(Ref);

	if (Existing != INDEX_NONE)
	{
		// Remove from the set.
		SelectedLanes.RemoveAt(Existing);

		const bool bWasPrimary = (SectionIndex == SelectedSectionIndex && LaneIndex == SelectedLaneIndex);
		if (bWasPrimary)
		{
			if (SelectedLanes.Num() > 0)
			{
				// Promote another remaining lane to primary.
				const FRoadSectionLaneRef& NewPrimary = SelectedLanes.Last();
				SelectedSectionIndex = NewPrimary.SectionIndex;
				SelectedLaneIndex = NewPrimary.LaneIndex;
				State = ERoadSectionSelectionState::Lane;
			}
			else
			{
				// Nothing left: drop to the section centerline of the toggled lane.
				SelectedSectionIndex = SectionIndex;
				SelectedLaneIndex = MetaRoad::ZeroLaneIndex;
				State = ERoadSectionSelectionState::Section;
			}
			SelectedKeyIndex = INDEX_NONE;
			SelectedTangentHandleType = ESelectedTangentHandle::None;
		}
	}
	else
	{
		// Add to the set and make it the new primary.
		SelectedLanes.Add(Ref);
		SelectedSectionIndex = SectionIndex;
		SelectedLaneIndex = LaneIndex;
		SelectedKeyIndex = INDEX_NONE;
		SelectedTangentHandleType = ESelectedTangentHandle::None;
		State = ERoadSectionSelectionState::Lane;
	}

	UpdateSplineSelection();
}

void URoadSectionComponentVisualizerSelectionState::FixState()
{
	// Drop any selection-set entries whose section/lane disappeared from the layout (external edits, undo).
	PruneSelectedLanes();

	ERoadSectionSelectionState StateVerified = GetStateVerified();
	if (StateVerified != State)
	{
		if (StateVerified < ERoadSectionSelectionState::Key)
		{
			SelectedKeyIndex = INDEX_NONE;
			SelectedTangentHandleType = ESelectedTangentHandle::None;
		}
		if (StateVerified < ERoadSectionSelectionState::Lane)
		{
			SelectedLaneIndex = MetaRoad::ZeroLaneIndex;
			SelectedLanes.Reset();
		}
		if (StateVerified < ERoadSectionSelectionState::Section)
		{
			SelectedSectionIndex = INDEX_NONE;
		}
		if (StateVerified < ERoadSectionSelectionState::Component)
		{
			SplinePropertyPath = FComponentPropertyPath();
		}
		State = StateVerified;
		//ResetCahedData();
	}

	// Always re-push: the set may have been pruned even when the primary state is unchanged.
	UpdateSplineSelection();

	// FixState can clear SplinePropertyPath directly (line above) — keep the module cache in sync (idempotent).
	FMetaRoadSelectionController::Get().SetCurrentSelectedSpline(GetSelectedSpline());
}

void URoadSectionComponentVisualizerSelectionState::ResetSelection(bool bSaveSplineSelection)
{
	SelectedSectionIndex = INDEX_NONE;
	SelectedLaneIndex = MetaRoad::ZeroLaneIndex;
	SelectedKeyIndex = INDEX_NONE;
	SelectedTangentHandleType = ESelectedTangentHandle::None;
	SelectedLanes.Reset();

	// Decide the post-reset State BEFORE pushing to the component: UpdateSplineSelection() derives the loop
	// highlight from State (SetLoopSelected(State == Loop)). Leaving State == Loop here would re-arm the loop
	// highlight on the still-selected spline. Keep the push before the (optional) SplinePropertyPath clear so
	// GetSelectedSpline() is still valid and the flag actually reaches the component.
	const bool bKeepSpline = bSaveSplineSelection && SplinePropertyPath.IsValid();
	State = bKeepSpline ? ERoadSectionSelectionState::Lane : ERoadSectionSelectionState::None;

	UpdateSplineSelection();

	if (!bKeepSpline)
	{
		SplinePropertyPath = FComponentPropertyPath();
	}

	ResetCahedData();

	// Single source of truth: publish the (possibly cleared) selected spline to the module.
	FMetaRoadSelectionController::Get().SetCurrentSelectedSpline(GetSelectedSpline());
}

void URoadSectionComponentVisualizerSelectionState::SetSelectedSpline(FComponentPropertyPath& InSplinePropertyPath)
{
	ResetSelection(false);

	check(InSplinePropertyPath.IsValid());

	State = ERoadSectionSelectionState::Component;
	SplinePropertyPath = InSplinePropertyPath;
	SelectedSectionIndex = INDEX_NONE;
	SelectedLaneIndex = MetaRoad::ZeroLaneIndex;
	SelectedKeyIndex = INDEX_NONE;
	SelectedTangentHandleType = ESelectedTangentHandle::None;

	FMetaRoadSelectionController::Get().SetCurrentSelectedSpline(GetSelectedSpline());
}

void URoadSectionComponentVisualizerSelectionState::SetSelectedSection(int32 InSelectedSectionIndex) 
{
	check(InSelectedSectionIndex >= 0);
	check(SplinePropertyPath.IsValid());
	check(uint8(State) >= uint8(ERoadSectionSelectionState::Component));

	State = ERoadSectionSelectionState::Section;
	SelectedSectionIndex = InSelectedSectionIndex;
	SelectedLaneIndex = MetaRoad::ZeroLaneIndex;
	SelectedKeyIndex = INDEX_NONE;
	SelectedTangentHandleType = ESelectedTangentHandle::None;
	SelectedLanes.Reset();

	UpdateSplineSelection();
}

void URoadSectionComponentVisualizerSelectionState::SetSelectedLoop()
{
	check(SplinePropertyPath.IsValid());
	check(uint8(State) >= uint8(ERoadSectionSelectionState::Component));

	State = ERoadSectionSelectionState::Loop;
	SelectedSectionIndex = INDEX_NONE;
	SelectedLaneIndex = MetaRoad::ZeroLaneIndex;
	SelectedKeyIndex = INDEX_NONE;
	SelectedTangentHandleType = ESelectedTangentHandle::None;
	SelectedLanes.Reset();

	UpdateSplineSelection();
}

void URoadSectionComponentVisualizerSelectionState::SetSelectedLane(int32 InSelectedLaneIndex) 
{
	check(SplinePropertyPath.IsValid());
	check(SelectedSectionIndex != INDEX_NONE);
	check(uint8(State) >= uint8(ERoadSectionSelectionState::Section));

	State = ERoadSectionSelectionState::Lane;
	SelectedLaneIndex = InSelectedLaneIndex;
	SelectedKeyIndex = INDEX_NONE;
	SelectedTangentHandleType = ESelectedTangentHandle::None;

	// Single-select replaces the whole selection set with this one lane.
	SelectedLanes.Reset();
	if (InSelectedLaneIndex != MetaRoad::ZeroLaneIndex)
	{
		SelectedLanes.Emplace(SelectedSectionIndex, InSelectedLaneIndex);
	}

	UpdateSplineSelection();
}

void URoadSectionComponentVisualizerSelectionState::SetSelectedAttributeDescriptor(const TSubclassOf<URoadLaneAttributeDescriptor>& AttributeDescriptor)
{
	SelectedAttributeDescriptor = AttributeDescriptor;
	// The selected key belonged to the previous attribute — clear it. If a key was selected, drop the state
	// back to Lane so the selected section/lane is preserved (instead of resetting the whole selection).
	SelectedKeyIndex = INDEX_NONE;
	SelectedTangentHandleType = ESelectedTangentHandle::None;
	if (State == ERoadSectionSelectionState::Key)
	{
		State = ERoadSectionSelectionState::Lane;
	}
}

void URoadSectionComponentVisualizerSelectionState::SetSelectedKeyIndex(int32 KeyIndex)
{
	check(SplinePropertyPath.IsValid());
	check(SelectedSectionIndex != INDEX_NONE);
	check(uint8(State) >= uint8(ERoadSectionSelectionState::Section));

	State = ERoadSectionSelectionState::Key;
	SelectedKeyIndex = KeyIndex;
	SelectedTangentHandleType = ESelectedTangentHandle::None;
}

void URoadSectionComponentVisualizerSelectionState::SetSelectedTangent(ESelectedTangentHandle TangentHandle)
{
	check(SplinePropertyPath.IsValid());
	check(SelectedSectionIndex != INDEX_NONE);
	check(SelectedKeyIndex >= 0);
	check(uint8(State) >= uint8(ERoadSectionSelectionState::Key));

	State = ERoadSectionSelectionState::KeyTangent;
	SelectedTangentHandleType = TangentHandle;
}

ERoadSectionSelectionState URoadSectionComponentVisualizerSelectionState::GetStateVerified() const
{
	URoadSplineComponent* Component = GetSelectedSpline();

	// Loop is a sibling of Section (not part of the None<Component<Section<Lane<Key linear chain), so verify
	// it explicitly before the ordered checks below. Requires a valid closed spline with a valid LoopedRoadZone.
	if (State == ERoadSectionSelectionState::Loop)
	{
		if (!IsValid(Component))
		{
			return ERoadSectionSelectionState::None;
		}
		if (!Component->IsClosedLoop() || !Component->GetRoadLayout().LoopedRoadZone.IsValid())
		{
			return ERoadSectionSelectionState::Component;
		}
		return ERoadSectionSelectionState::Loop;
	}

	if (State > ERoadSectionSelectionState::None)
	{
		if (!IsValid(Component))
		{
			return ERoadSectionSelectionState::None;
		}

		if (State > ERoadSectionSelectionState::Component)
		{
			if (SelectedSectionIndex < 0 || SelectedSectionIndex >= Component->GetLaneSectionsNum())
			{
				return ERoadSectionSelectionState::Component;
			}

			if (State == ERoadSectionSelectionState::Section)
			{
				if (SelectedLaneIndex != MetaRoad::ZeroLaneIndex)
				{
					return ERoadSectionSelectionState::Component;
				}
			}

			if (State >= ERoadSectionSelectionState::Lane)
			{
				const auto& Section = Component->GetLaneSection(SelectedSectionIndex);
				if (SelectedLaneIndex != MetaRoad::ZeroLaneIndex && !Section.CheckLaneIndex(SelectedLaneIndex))
				{
					return ERoadSectionSelectionState::Component;
				}

				if (State >= ERoadSectionSelectionState::Key)
				{
					if (!IsKeyValid.IsBound() || !IsKeyValid.Execute())
					{
						return ERoadSectionSelectionState::Lane;
					}
				}
			}
		}
	}

	return State;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------
class FRoadSectionComponentVisualizerCommands : public TCommands<FRoadSectionComponentVisualizerCommands>
{
public:
	FRoadSectionComponentVisualizerCommands() : TCommands <FRoadSectionComponentVisualizerCommands>
	(
		"RoadSectionComponentVisualizer",	// Context name for fast lookup
		LOCTEXT("RoadSectionComponentVisualizer", "Road Spline Section Component Visualizer"),	// Localized context name for displaying
		NAME_None,	// Parent
		FMetaRoadEditorStyle::Get().GetStyleSetName()
	)
	{
	}

	virtual void RegisterCommands() override
	{
		UI_COMMAND(SplitFullSection, "Split Full Section", "Split road section at the cursor location.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(SplitSideSection, "Split Side Section", "Split road section at the cursor location.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(DeleteSection, "Delete Section", "Delete current road section.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(CreatProfile, "Create Profile", "Create the road profile asset (URoadProfile) from selected road section.", EUserInterfaceActionType::Button, FInputChord());

		UI_COMMAND(AddLaneToLeft,  "Add Lane to Left", "Add a new road lane to the left sida of the currently selected lane.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(AddLaneToRight, "Add Lane to Right", "Add a new road lane to the right sida of the currently selected lane.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(DeleteLane, "Delete Lane", "Delete selected lane.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(ReverseLane, "Reverse Direction", "Reverse direction for selected lane.", EUserInterfaceActionType::ToggleButton, FInputChord());
	}

	virtual ~FRoadSectionComponentVisualizerCommands()
	{
	}

public:
	TSharedPtr<FUICommandInfo> SplitFullSection;
	TSharedPtr<FUICommandInfo> SplitSideSection;
	TSharedPtr<FUICommandInfo> DeleteSection;
	TSharedPtr<FUICommandInfo> CreatProfile;

	TSharedPtr<FUICommandInfo> AddLaneToLeft;
	TSharedPtr<FUICommandInfo> AddLaneToRight;
	TSharedPtr<FUICommandInfo> DeleteLane;
	TSharedPtr<FUICommandInfo> ReverseLane;
};

// -------------------------------------------------------------------------------------------------------------------------------------------------
 
FRoadSectionComponentVisualizer::FRoadSectionComponentVisualizer()
	: FComponentVisualizer()
{
	FRoadSectionComponentVisualizerCommands::Register();

	RoadScetionComponentVisualizerActions = MakeShareable(new FUICommandList);
	// Borrow the shared selection state from the module-owned model (outlives this visualizer's swap).
	SelectionState = FMetaRoadSelectionController::Get().GetSelectionModel()->GetSectionState();
}

void FRoadSectionComponentVisualizer::OnRegister()
{
	const auto& Commands = FRoadSectionComponentVisualizerCommands::Get();

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.SplitFullSection,
		FExecuteAction::CreateSP(this, &FRoadSectionComponentVisualizer::OnSplitSection, true),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetState() >= ERoadSectionSelectionState::Section; }));

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.SplitSideSection,
		FExecuteAction::CreateSP(this, &FRoadSectionComponentVisualizer::OnSplitSection, false),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetState() >= ERoadSectionSelectionState::Section; }));

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.DeleteSection,
		FExecuteAction::CreateSP(this, &FRoadSectionComponentVisualizer::OnDeleteSection),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetState() >= ERoadSectionSelectionState::Section; }));

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.CreatProfile,
		FExecuteAction::CreateSP(this, &FRoadSectionComponentVisualizer::OnCretaeProfile),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetState() >= ERoadSectionSelectionState::Section; }));

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.AddLaneToLeft,
		FExecuteAction::CreateSP(this, &FRoadSectionComponentVisualizer::OnAddLane, true),
		FCanExecuteAction::CreateLambda([this]() { return  SelectionState->GetState() >= ERoadSectionSelectionState::Section; }));

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.AddLaneToRight,
		FExecuteAction::CreateSP(this, &FRoadSectionComponentVisualizer::OnAddLane, false),
		FCanExecuteAction::CreateLambda([this]() { return  SelectionState->GetState() >= ERoadSectionSelectionState::Section; }));

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.DeleteLane,
		FExecuteAction::CreateSP(this, &FRoadSectionComponentVisualizer::OnDeleteLane),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetState() >= ERoadSectionSelectionState::Lane && SelectionState->GetSelectedLaneIndex() != MetaRoad::ZeroLaneIndex; }));

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.ReverseLane,
		FExecuteAction::CreateSP(this, &FRoadSectionComponentVisualizer::OnReverseLane),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetState() >= ERoadSectionSelectionState::Lane && SelectionState->GetSelectedLaneIndex() != MetaRoad::ZeroLaneIndex; }),
		FIsActionChecked::CreateSP(this, &FRoadSectionComponentVisualizer::IsLaneReverse));

	ConfigureKeyOverlay();
}

void FRoadSectionComponentVisualizer::ConfigureKeyOverlay()
{
	TWeakObjectPtr<URoadSectionComponentVisualizerSelectionState> WeakState(SelectionState);

	auto GetSOffset = [WeakState]() -> TOptional<double>
	{
		URoadSectionComponentVisualizerSelectionState* State = WeakState.Get();
		if (!State || State->GetStateVerified() < ERoadSectionSelectionState::Section)
		{
			return TOptional<double>();
		}
		URoadSplineComponent* Spline = State->GetSelectedSpline();
		if (!Spline)
		{
			return TOptional<double>();
		}
		return Spline->GetLaneSection(State->GetSelectedSectionIndex()).SOffset;
	};

	auto CommitSOffset = [WeakState](double NewValue, ETextCommit::Type)
	{
		URoadSectionComponentVisualizerSelectionState* State = WeakState.Get();
		if (!State || State->GetStateVerified() < ERoadSectionSelectionState::Section)
		{
			return;
		}
		URoadSplineComponent* Spline = State->GetSelectedSpline();
		if (!Spline)
		{
			return;
		}
		const int SectionIndex = State->GetSelectedSectionIndex();
		if (SectionIndex <= 0)
		{
			return; // Section 0 is fixed at SOffset 0.
		}

		const FScopedTransaction Transaction(LOCTEXT("SetSectionSOffset", "Set Section SOffset"));
		Spline->Modify();
		State->Modify();

		auto& Sections = Spline->GetLaneSections();
		FRoadLaneSection& Section = Sections[SectionIndex];
		const double MinS = Sections[SectionIndex - 1].SOffset + 1.0;
		const double MaxS = (SectionIndex < Sections.Num() - 1)
			? Sections[SectionIndex + 1].SOffset - 1.0
			: Spline->GetSplineLength();
		Section.SOffset = FMath::Clamp(NewValue, MinS, MaxS);

		Spline->UpdateLaneSectionBounds();
		State->SetCashedDataAtSplineDistance(Section.SOffset);

		Spline->MarkRoadStateDirty();
		Spline->UpdateMagicTransform();
		Spline->UpdateLandscape();
		Spline->MarkRenderStateDirty();
		GEditor->RedrawLevelEditingViewports(true);
	};

	auto IsEnabled = [WeakState]() -> bool
	{
		URoadSectionComponentVisualizerSelectionState* State = WeakState.Get();
		return State && State->GetSelectedSectionIndex() > 0;
	};

	auto Visibility = [WeakState]() -> EVisibility
	{
		URoadSectionComponentVisualizerSelectionState* State = WeakState.Get();
		return (State && State->GetStateVerified() >= ERoadSectionSelectionState::Section)
			? EVisibility::Visible : EVisibility::Collapsed;
	};

	TSharedRef<SWidget> Content = RoadKeyOverlay::MakeNumericRow(
		LOCTEXT("SectionSOffset", "S"),
		TAttribute<TOptional<double>>::Create(GetSOffset),
		SNumericEntryBox<double>::FOnValueCommitted::CreateLambda(CommitSOffset),
		TAttribute<bool>::Create(IsEnabled));

	OverlayController = MakeShared<FRoadKeyOverlayController>();
	OverlayController->Setup(Content, LOCTEXT("SectionKeyHeader", "Section"), TAttribute<EVisibility>::Create(Visibility));
}

FRoadSectionComponentVisualizer::~FRoadSectionComponentVisualizer()
{
	//FRoadSectionComponentVisualizerCommands::Unregister();
	EndEditing();
	SelectionState->ConditionalBeginDestroy();
}

void FRoadSectionComponentVisualizer::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (SelectionState)
	{
		Collector.AddReferencedObject(SelectionState);
	}
}

bool FRoadSectionComponentVisualizer::ShouldDraw(const UActorComponent* Component) const
{
	const URoadSplineComponent* SplineComp = Cast<const URoadSplineComponent>(Component);
	if (!SplineComp)
	{
		return false;
	}

	if (!SplineComp->IsVisibleInEditor())
	{
		return false;
	}

	// Allow draw only one manuale selected components
	/*
	TArray<TObjectPtr<URoadSplineComponent>> OwnerComponents;
	SplineComp->GetOwner()->GetComponents(OwnerComponents);
	if (OwnerComponents.Num() > 1 && SplineComp->SceneProxy && !SplineComp->SceneProxy->IsIndividuallySelected())
	{
		return false;
	}
	*/

	return true;
}

void FRoadSectionComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	if (!ShouldDraw(Component))
	{
		return;
	}

	if (OverlayController)
	{
		OverlayController->EnsureAddedToActiveViewport();
	}

	const URoadSplineComponent* SplineComp = CastChecked<const URoadSplineComponent>(Component);

	const bool bIsEditingComponent = GetEditedSplineComponent() == SplineComp;
	const float GrabHandleSize = 14.0f +GetDefault<ULevelEditorViewportSettings>()->SelectedSplinePointSizeAdjustment;

	// Draw sections line
	for (int32 SectionIndex = 0; SectionIndex < SplineComp->GetLaneSectionsNum(); ++SectionIndex)
	{
		PDI->SetHitProxy(new HRoadLaneVisProxy(SplineComp, SectionIndex, MetaRoad::ZeroLaneIndex));

		const FColor& Color = SelectionState->IsSelected(SplineComp, SectionIndex, MetaRoad::ZeroLaneIndex)
			? FMetaRoadColors::SelectedColor : FMetaRoadColors::AccentColorHi;
		DrawUtils::DrawLaneBorder(PDI, SplineComp, SectionIndex, 0, Color, Color, SDPG_Foreground, 4.0, 0.0, true);
			
		if (bIsEditingComponent)
		{
			PDI->SetHitProxy(new HRoadSectionKeyVisProxy(SplineComp, SectionIndex));
			FVector Location = SplineComp->EvalLanePoistion(SectionIndex, MetaRoad::ZeroLaneIndex, SplineComp->GetLaneSection(SectionIndex).SOffset, 0.0, ESplineCoordinateSpace::World);
			PDI->DrawPoint(Location, Color, GrabHandleSize, SDPG_Foreground);
			PDI->SetHitProxy(NULL);
		}

		PDI->SetHitProxy(NULL);
	}

	if (bIsEditingComponent)
	{
		if (SelectionState->GetState() == ERoadSectionSelectionState::Section || SelectionState->GetState() == ERoadSectionSelectionState::Lane)
		{
			DrawUtils::DrawCrossSpline(PDI, SplineComp, SelectionState->GetCachedSplineKey(), FMetaRoadColors::CrossSplineColor, SDPG_Foreground);
		}
	}
}

void FRoadSectionComponentVisualizer::DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
}

const URoadSplineComponent* FRoadSectionComponentVisualizer::UpdateSelectedComponentAndSectionAndLane(HComponentVisProxy* VisProxy)
{
	check(SelectionState);

	const URoadSplineComponent* NewSplineComp = CastChecked<const URoadSplineComponent>(VisProxy->Component.Get());

	AActor* OldSplineOwningActor = SelectionState->GetSplinePropertyPath().GetParentOwningActor();
	FComponentPropertyPath NewSplinePropertyPath(NewSplineComp);
	SelectionState->SetSelectedSpline(NewSplinePropertyPath);
	SelectionState->SetSelectedAttributeDescriptor(FMetaRoadSelectionController::Get().GetSelectedAttributeDescriptor());
	AActor* NewSplineOwningActor = NewSplinePropertyPath.GetParentOwningActor();

	if (NewSplinePropertyPath.IsValid())
	{
		if (OldSplineOwningActor != NewSplineOwningActor)
		{
			// Reset selection state if we are selecting a different actor to the one previously selected
			SelectionState->ResetSelection(true);
		}

		URoadSplineComponent* Spline = GetEditedSplineComponent();
		//DeselectedInEditorDelegateHandle = Spline->OnDeselectedInEditor.AddRaw(this, &FRoadSectionComponentVisualizer::OnDeselectedInEditor);

		SelectionState->SetCashedDataAtSplineInputKey(0.0);
	}
	else
	{
		SelectionState->ResetSelection(false);
		return nullptr;
	}

	CompVisUtils::DeselectAllExcept(NewSplineComp);

	if (VisProxy->IsA(HRoadSectionVisProxy::StaticGetType()))
	{
		HRoadSectionVisProxy* SectionProxy = (HRoadSectionVisProxy*)VisProxy;
		check(SectionProxy->SectionIndex >= 0);
		check(SectionProxy->SectionIndex < NewSplineComp->GetLaneSectionsNum());
		SelectionState->SetSelectedSection(SectionProxy->SectionIndex);
		const FRoadLaneSection& Section = NewSplineComp->GetLaneSection(SectionProxy->SectionIndex);

		if (VisProxy->IsA(HRoadLaneVisProxy::StaticGetType()))
		{
			HRoadLaneVisProxy* LaneProxy = (HRoadLaneVisProxy*)VisProxy;
			check(LaneProxy->LaneIndex == MetaRoad::ZeroLaneIndex || Section.CheckLaneIndex(LaneProxy->LaneIndex));
			SelectionState->SetSelectedLane(LaneProxy->LaneIndex);
		}
	}

	return NewSplineComp;
}

bool FRoadSectionComponentVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{

	bool bVisProxyClickHandled = false;

	if(VisProxy && VisProxy->Component.IsValid())
	{
		if (VisProxy->IsA(HRoadLaneVisProxy::StaticGetType()))
		{
			HRoadLaneVisProxy* Proxy = (HRoadLaneVisProxy*)VisProxy;

			// Ctrl+Click on a real lane of the already-selected spline toggles it in/out of the
			// multi-selection set (may span sections). Anything else is a single-select replace.
			const URoadSplineComponent* CurrentSpline = SelectionState->GetSelectedSpline();
			const bool bCtrlToggle =
				Click.IsControlDown() &&
				Proxy->LaneIndex != MetaRoad::ZeroLaneIndex &&
				CurrentSpline != nullptr &&
				CurrentSpline == Proxy->Component.Get() &&
				SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Section;

			if (bCtrlToggle)
			{
				const FScopedTransaction Transaction(LOCTEXT("ToggleRoadSectionLane", "Toggle Road Lane Selection"));
				SelectionState->Modify();
				SelectionState->ToggleLane(Proxy->SectionIndex, Proxy->LaneIndex);

				auto Rang = CurrentSpline->GetLaneRang(Proxy->SectionIndex, Proxy->LaneIndex);
				const float Key = CurrentSpline->KeyAtRayHit(Rang.StartS, Rang.EndS, Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection() * 50000.0f);
				SelectionState->SetCashedDataAtSplineInputKey(Key);

				bVisProxyClickHandled = true;
			}
			else
			{
				const FScopedTransaction Transaction(LOCTEXT("SelectRoadSectionLane", "Select Road Lane"));
				SelectionState->Modify();
				if (const URoadSplineComponent* SplineComp = UpdateSelectedComponentAndSectionAndLane(VisProxy))
				{
					if (Proxy->LaneIndex == MetaRoad::ZeroLaneIndex)
					{
						SelectionState->SetSelectedSection(Proxy->SectionIndex);
					}
					else
					{
						SelectionState->SetSelectedLane(Proxy->LaneIndex);
					}

					bVisProxyClickHandled = true;

					const auto& Section = SplineComp->GetLaneSection(Proxy->SectionIndex);
					auto Rang = SplineComp->GetLaneRang(Proxy->SectionIndex, Proxy->LaneIndex);
					const float Key = SplineComp->KeyAtRayHit(Rang.StartS, Rang.EndS, Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection() * 50000.0f);
					SelectionState->SetCashedDataAtSplineInputKey(Key);
				}
			}
		}
		else if (VisProxy->IsA(HRoadLoopVisProxy::StaticGetType()))
		{
			// Click on the filled interior of a closed spline → select the looped fill area.
			const FScopedTransaction Transaction(LOCTEXT("SelectRoadLoop", "Select Road Loop Fill"));
			SelectionState->Modify();
			if (UpdateSelectedComponentAndSectionAndLane(VisProxy))
			{
				SelectionState->SetSelectedLoop();
				bVisProxyClickHandled = true;
			}
		}
		else
		{
			const FScopedTransaction Transaction(LOCTEXT("UnselectRoad", "Unselect Road"));
			SelectionState->Modify();
			SelectionState->ResetSelection(false);
		}
	}

	if (bVisProxyClickHandled)
	{
		GEditor->RedrawLevelEditingViewports(true);
	}

	return bVisProxyClickHandled;
}

URoadSplineComponent* FRoadSectionComponentVisualizer::GetEditedSplineComponent() const
{
	check(SelectionState);

	URoadSplineComponent* SplineComp = SelectionState->GetSelectedSpline();

	if (SplineComp && CompVisUtils::IsSelectedInViewport(SplineComp))
	{
		return SplineComp;
	}

	return nullptr;
}

int32 FRoadSectionComponentVisualizer::AddAttributeKeyToLaneNoRefresh(
	int32 SectionIndex, int32 LaneIndex,
	const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor,
	TConstStructView<FRoadLaneAttributeValue> Value,
	double SOffset, bool bReplaceExisting)
{
	if (!IsValid(Descriptor.Get()))
	{
		return INDEX_NONE;
	}

	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	if (!SplineComp)
	{
		return INDEX_NONE;
	}

	URoadLaneAttributeDescriptor* DefaultObject = Descriptor->GetDefaultObject<URoadLaneAttributeDescriptor>();
	check(DefaultObject);

	if (!DefaultObject->CanBeAddedTo(SplineComp, SectionIndex, LaneIndex))
	{
		return INDEX_NONE;
	}

	// Effective value: caller-provided, or the descriptor's template.
	const TConstStructView<FRoadLaneAttributeValue> Effective = Value.IsValid() ? Value : DefaultObject->GetAttributeValueTemplate();
	const UScriptStruct* Struct = Effective.GetScriptStruct();
	const void* Mem = Effective.GetMemory();
	if (!Struct || !Mem)
	{
		return INDEX_NONE;
	}

	// Resolve the attribute map: section centre line vs a real lane (CanBeAddedTo above validated it).
	FRoadLaneSection& Section = SplineComp->GetLaneSection(SectionIndex);
	TMap<TSoftClassPtr<URoadLaneAttributeDescriptor>, FRoadLaneAttribute>* Attributes = &Section.Attributes;
	if (LaneIndex != MetaRoad::ZeroLaneIndex)
	{
		FRoadLane* Lane = SplineComp->GetRoadLane(SectionIndex, LaneIndex);
		if (!Lane)
		{
			return INDEX_NONE;
		}
		Attributes = &Lane->Attributes;
	}

	SplineComp->Modify();

	FRoadLaneAttribute& Attribute = Attributes->FindOrAdd(Descriptor.Get());

	if (bReplaceExisting)
	{
		Attribute.Reset();
		Attribute.SetScriptStruct(Struct);
	}
	else if (!Attribute.GetScriptStruct())
	{
		Attribute.SetScriptStruct(Struct);
	}

	// Type-mismatch guard: an existing attribute under this descriptor must share the value struct.
	if (Attribute.GetScriptStruct() != Struct)
	{
		return INDEX_NONE;
	}

	return Attribute.UpdateOrAddTypedKey(SOffset, Mem, Struct);
}

int32 FRoadSectionComponentVisualizer::AddAttributeKeyToLane(
	int32 SectionIndex, int32 LaneIndex,
	const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor,
	TConstStructView<FRoadLaneAttributeValue> Value,
	double SOffset, bool bReplaceExisting)
{
	const int32 KeyIndex = AddAttributeKeyToLaneNoRefresh(SectionIndex, LaneIndex, Descriptor, Value, SOffset, bReplaceExisting);
	if (KeyIndex != INDEX_NONE)
	{
		if (URoadSplineComponent* SplineComp = GetEditedSplineComponent())
		{
			SplineComp->MarkRoadStateDirty();
			SplineComp->UpdateLandscape();
			GEditor->RedrawLevelEditingViewports(true);
		}
	}
	return KeyIndex;
}

int32 FRoadSectionComponentVisualizer::AddAttributeKeyToSelection(
	const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor,
	TConstStructView<FRoadLaneAttributeValue> Value,
	double SOffset, bool bReplaceExisting)
{
	check(SelectionState);

	if (SelectionState->GetStateVerified() < ERoadSectionSelectionState::Section)
	{
		return INDEX_NONE;
	}

	return AddAttributeKeyToLane(
		SelectionState->GetSelectedSectionIndex(),
		SelectionState->GetSelectedLaneIndex(),
		Descriptor, Value, SOffset, bReplaceExisting);
}

int32 FRoadSectionComponentVisualizer::AddAttributeKeyToAllSelectedLanes(
	const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor,
	TConstStructView<FRoadLaneAttributeValue> Value,
	double SOffset, bool bReplaceExisting)
{
	check(SelectionState);

	if (SelectionState->GetStateVerified() < ERoadSectionSelectionState::Section)
	{
		return INDEX_NONE;
	}

	const int32 PrimarySection = SelectionState->GetSelectedSectionIndex();
	const int32 PrimaryLane = SelectionState->GetSelectedLaneIndex();

	const TArray<FRoadSectionLaneRef> Lanes = GatherSelectedRealLanes();
	if (Lanes.Num() == 0)
	{
		// Only the section centerline is selected: same behaviour as the single-lane entry point.
		return AddAttributeKeyToLane(PrimarySection, PrimaryLane, Descriptor, Value, SOffset, bReplaceExisting);
	}

	// Mutate every selected lane, then refresh ONCE (avoids N× UpdateLandscape/redraw).
	int32 PrimaryKeyIndex = INDEX_NONE;
	bool bAnyChanged = false;
	for (const FRoadSectionLaneRef& Lane : Lanes)
	{
		const int32 KeyIndex = AddAttributeKeyToLaneNoRefresh(Lane.SectionIndex, Lane.LaneIndex, Descriptor, Value, SOffset, bReplaceExisting);
		if (KeyIndex != INDEX_NONE)
		{
			bAnyChanged = true;
		}
		if (Lane.SectionIndex == PrimarySection && Lane.LaneIndex == PrimaryLane)
		{
			PrimaryKeyIndex = KeyIndex;
		}
	}

	if (bAnyChanged)
	{
		if (URoadSplineComponent* SplineComp = GetEditedSplineComponent())
		{
			SplineComp->MarkRoadStateDirty();
			SplineComp->UpdateLandscape();
			GEditor->RedrawLevelEditingViewports(true);
		}
	}
	return PrimaryKeyIndex;
}

UActorComponent* FRoadSectionComponentVisualizer::GetEditedComponent() const
{
	return Cast<UActorComponent>(GetEditedSplineComponent());
}

bool FRoadSectionComponentVisualizer::GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const
{
	if (URoadSplineComponent* SplineComp = GetEditedSplineComponent())
	{
		if (SelectionState->GetState() == ERoadSectionSelectionState::Section || SelectionState->GetState() > ERoadSectionSelectionState::Lane)
		{
			//const float S = SplineComp->Sections[SelectionState->GetSelectedSectionIndex()].SOffset;
			OutLocation = SelectionState->GetCashedPosition();// SplineComp->GetWorldLocationAtDistanceAlongSpline(S);
			return true;
		}
	}
	return false;
}

bool FRoadSectionComponentVisualizer::GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const
{
	if (ViewportClient->GetWidgetCoordSystemSpace() == COORD_Local || ViewportClient->GetWidgetMode() == UE::Widget::WM_Rotate)
	{
		if (URoadSplineComponent* SplineComp = GetEditedSplineComponent())
		{
			if (SelectionState->GetState() == ERoadSectionSelectionState::Section || SelectionState->GetState() > ERoadSectionSelectionState::Lane)
			{
				OutMatrix = FRotationMatrix::Make(SelectionState->GetCachedRotation());
				return true;
			}
		}
	}

	return false;
}

bool FRoadSectionComponentVisualizer::IsVisualizingArchetype() const
{
	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	return (SplineComp && SplineComp->GetOwner() && FActorEditorUtils::IsAPreviewOrInactiveActor(SplineComp->GetOwner()));
}

bool FRoadSectionComponentVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	if (!SplineComp)
	{
		return false;
	}

	check(SelectionState);
	auto State = SelectionState->GetStateVerified();

	if (State == ERoadSectionSelectionState::Section)
	{
		const FVector WidgetLocationWorld = SelectionState->GetCashedPosition() + DeltaTranslate;
		const float ClosestKey = SplineComp->FindInputKeyClosestToWorldLocation(WidgetLocationWorld);
		const float ClosestS = SplineComp->GetDistanceAlongSplineAtSplineInputKey(ClosestKey);

		FRoadLaneSection& Section = SplineComp->GetLaneSection(SelectionState->GetSelectedSectionIndex());
		Section.SOffset = ClosestS;
		SelectionState->SetCashedDataAtSplineInputKey(ClosestKey);

		SplineComp->UpdateLaneSectionBounds();
		SplineComp->UpdateMagicTransform();
		SplineComp->MarkRenderStateDirty();
		GEditor->RedrawLevelEditingViewports(true);
		return true;
	}
	else if (State == ERoadSectionSelectionState::Lane)
	{
		//return true;
	}


	return false;
}

bool FRoadSectionComponentVisualizer::HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	bool bHandled = false;

	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp != nullptr)
	{
		// Something external has changed the number of lane sactions, meaning that the cached selected are no longer valid
		if (SelectionState->GetSelectedSectionIndex() != INDEX_NONE && SelectionState->GetSelectedSectionIndex() >= SplineComp->GetLaneSectionsNum())
		{
			EndEditing();
			return false;
		}
	}

	if (Event == IE_Pressed)
	{
		bHandled = RoadScetionComponentVisualizerActions->ProcessCommandBindings(Key, FSlateApplication::Get().GetModifierKeys(), false);
	}

	return bHandled;
}

bool FRoadSectionComponentVisualizer::HandleModifiedClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	return false;
}

bool FRoadSectionComponentVisualizer::HasFocusOnSelectionBoundingBox(FBox& OutBoundingBox)
{
	return false;
}

bool FRoadSectionComponentVisualizer::HandleSnapTo(const bool bInAlign, const bool bInUseLineTrace, const bool bInUseBounds, const bool bInUsePivot, AActor* InDestination)
{
	return false;
}

void FRoadSectionComponentVisualizer::TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove)
{
	if (bInDidMove)
	{
		// After dragging, notify that the spline curves property has changed one last time, this time as a EPropertyChangeType::ValueSet :
		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		check(SplineComp != nullptr);
		//NotifyPropertiesModified(SplineComp, { SplineCurvesProperty, SplineSegmentsProperty }, EPropertyChangeType::ValueSet);

		// 
		if (GetSelectionState()->GetState() == ERoadSectionSelectionState::Section)
		{
			// The section(s) may be deleted, so this situation needs to be handled here.

			ERoadSectionSelectionState StateVerified = SelectionState->GetStateVerified();
			double SOffset = 0;
			bool bValid = false;
			if (StateVerified >= ERoadSectionSelectionState::Section)
			{
				SOffset = SplineComp->GetLaneSection(SelectionState->GetSelectedSectionIndex()).SOffset ;
				bValid = true;
			}

			SplineComp->TrimLaneSections();

			SelectionState->FixState();

			if (bValid)
			{
				SelectionState->SetSelectedSection(CompVisUtils::FindBestFit(SplineComp->GetLaneSections(), [SOffset](auto& It) { return FMath::Abs(SOffset - It.SOffset); }));
			}
		}
		else
		{
			SplineComp->TrimLaneSections();
		}
		
		SplineComp->MarkRoadStateDirty();
		SplineComp->UpdateLandscape();
		SplineComp->MarkRenderStateDirty();
		GEditor->RedrawLevelEditingViewports(true);
	}
}

void FRoadSectionComponentVisualizer::EndEditing()
{
	if (OverlayController)
	{
		OverlayController->Shutdown();
	}

	// Ignore if there is an undo/redo operation in progress
	if (!GIsTransacting)
	{
		if (IsValid(SelectionState))
		{
			SelectionState->ResetSelection(false);
		}
	}
}

void FRoadSectionComponentVisualizer::OnSplitSection(bool bFull)
{
	auto State = SelectionState->GetStateVerified();
	if (State < ERoadSectionSelectionState::Section)
	{
		return;

	}
	
	ERoadLaneSectionSide Side;
	if (bFull)
	{
		Side = ERoadLaneSectionSide::Both;
	}
	else
	{
		if (State >= ERoadSectionSelectionState::Lane)
		{
			int LaneIndex = SelectionState->GetSelectedLaneIndex();
			if (LaneIndex != MetaRoad::ZeroLaneIndex)
			{
				Side = LaneIndex > 0 ? ERoadLaneSectionSide::Right : ERoadLaneSectionSide::Left;
			}
			else
			{
				return;
			}
		}
		else
		{
			return;
		}
	}

	const FScopedTransaction Transaction(LOCTEXT("SpliteSection", "Split Section"));

	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	SplineComp->Modify();

	
	int NewSectionIndex =  SplineComp->SplitSection( SelectionState->GetCachedSplineKey(), Side);
	if (NewSectionIndex != INDEX_NONE)
	{

		SelectionState->Modify();
		SelectionState->SetSelectedSection(NewSectionIndex);
		//SelectionState->SetSelectedSegmentIndex(INDEX_NONE);
		//SelectionState->SetCashedPosition(FVector::ZeroVector);
		//SelectionState->SetCachedRotation(SplineComp->GetQuaternionAtSplinePoint(SelectionState->GetLastKeyIndexSelected(), ESplineCoordinateSpace::World));
		GEditor->RedrawLevelEditingViewports(true);
	}
	
}

void FRoadSectionComponentVisualizer::OnDeleteSection()
{
	if (SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Section)
	{
		const FScopedTransaction Transaction(LOCTEXT("DeleteSection", "Delete Scection"));

		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		SplineComp->Modify();
		SplineComp->GetLaneSections().RemoveAt(SelectionState->GetSelectedSectionIndex());
		SplineComp->UpdateRoadLayout();
		SplineComp->UpdateLaneSectionBounds();
		SplineComp->TrimLaneSections();
		SplineComp->UpdateMagicTransform();
		SplineComp->UpdateLandscape();
		SplineComp->MarkRenderStateDirty();

		SelectionState->Modify();
		SelectionState->SetSelectedLane(0);

		GEditor->RedrawLevelEditingViewports(true);
	}
}

void FRoadSectionComponentVisualizer::OnCretaeProfile()
{
	if (SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Section)
	{
		URoadSplineComponent* SplineComp = GetEditedSplineComponent();

		UFactory* FactoryInstance = DuplicateObject<UFactory>(GetDefault<URoadProfileFactory>(), GetTransientPackage());
		// Ensure this object is not GC for the duration of CreateAssetWithDialog
		FactoryInstance->AddToRoot();
		FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
		URoadProfile* NewAsset = Cast<URoadProfile>(AssetToolsModule.Get().CreateAssetWithDialog(FactoryInstance->GetSupportedClass(), FactoryInstance));
		if (NewAsset != nullptr)
		{
			NewAsset->AssignFromRoadSection(SplineComp->GetRoadLayout(), SelectionState->GetSelectedSectionIndex());
		}
		FactoryInstance->RemoveFromRoot();
	}
}


void FRoadSectionComponentVisualizer::OnAddLane(bool bOnLeft)
{
	if (SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Section)
	{
		const FScopedTransaction Transaction(LOCTEXT("AddLane", "Add Lane"));

		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		SplineComp->Modify();

		int SectionIndex = SelectionState->GetSelectedSectionIndex();
		int LaneIndex = SelectionState->GetSelectedLaneIndex();
		auto& SelectedSection = GetEditedSplineComponent()->GetLaneSection(SectionIndex);
		
		FRoadLane NewLane{};

		if (LaneIndex == MetaRoad::ZeroLaneIndex)
		{
			NewLane.RoadZone.InitializeAs<FRoadZoneDriving>();
			NewLane.Width.AddKey(0, MetaRoad::DefaultRoadLaneWidth);
		}
		else // Copy lane profile
		{
			auto& SelectedLane = SelectedSection.GetLaneByIndex(LaneIndex);
			NewLane.RoadZone = SelectedLane.RoadZone;
			NewLane.Width.AddKey(0, MetaRoad::DefaultRoadLaneWidth);
			NewLane.Direction = SelectedLane.Direction;
			if (SelectedLane.Width.GetNumKeys() && SelectedLane.Width.Keys[0].Value > UE_KINDA_SMALL_NUMBER)
			{
				NewLane.Width.Keys[0].Value = SelectedLane.Width.Keys[0].Value;
			}
		}

		NewLane.Width.Keys[0].InterpMode = ERichCurveInterpMode::RCIM_Cubic;
		NewLane.Width.Keys[0].TangentMode = ERichCurveTangentMode::RCTM_Auto;

		int NewSalactedLaneIndex = MetaRoad::ZeroLaneIndex;

		if (LaneIndex == MetaRoad::ZeroLaneIndex)
		{
			if (bOnLeft)
			{
				SelectedSection.Left.Insert(NewLane, 0);
				NewSalactedLaneIndex = -1;
			}
			else
			{
				SelectedSection.Right.Insert(NewLane, 0);
				NewSalactedLaneIndex = 1;
			}
		}
		else if (LaneIndex > 0)
		{
			SelectedSection.Right.Insert(MoveTemp(NewLane), LaneIndex - 1 + (bOnLeft ? 0 : 1));
			NewSalactedLaneIndex = LaneIndex + (bOnLeft ? 0 : 1);
		}
		else // LaneIndex <0
		{
			SelectedSection.Left.Insert(MoveTemp(NewLane), -LaneIndex - 1 + (bOnLeft ? 1 : 0));
			NewSalactedLaneIndex = LaneIndex - (bOnLeft ? 1 : 0);
		}

		SplineComp->UpdateRoadLayout();
		SplineComp->UpdateMagicTransform();
		SplineComp->UpdateLandscape();
		SplineComp->MarkRenderStateDirty();

		SelectionState->Modify();
		SelectionState->SetSelectedLane(NewSalactedLaneIndex);
		GEditor->RedrawLevelEditingViewports(true);
	}
}

TArray<FRoadSectionLaneRef> FRoadSectionComponentVisualizer::GatherSelectedRealLanes() const
{
	// All selected lanes (multi-selection), falling back to the primary when the set is empty.
	TArray<FRoadSectionLaneRef> Lanes = SelectionState->GetSelectedLanes();
	if (Lanes.Num() == 0)
	{
		const int SectionIndex = SelectionState->GetSelectedSectionIndex();
		const int LaneIndex = SelectionState->GetSelectedLaneIndex();
		if (SectionIndex != INDEX_NONE && LaneIndex != MetaRoad::ZeroLaneIndex)
		{
			Lanes.Emplace(SectionIndex, LaneIndex);
		}
	}

	// Never operate on the centerline reference.
	Lanes.RemoveAll([](const FRoadSectionLaneRef& Lane) { return Lane.LaneIndex == MetaRoad::ZeroLaneIndex; });
	return Lanes;
}

void FRoadSectionComponentVisualizer::OnDeleteLane()
{
	if (SelectionState->GetStateVerified() < ERoadSectionSelectionState::Lane)
	{
		return;
	}

	TArray<FRoadSectionLaneRef> Lanes = GatherSelectedRealLanes();
	if (Lanes.Num() == 0)
	{
		return;
	}

	// Delete outer-to-inner per side so earlier removals don't shift not-yet-removed indices.
	// Sorting by descending |LaneIndex| handles both sides (Left and Right arrays are independent).
	Lanes.Sort([](const FRoadSectionLaneRef& A, const FRoadSectionLaneRef& B)
	{
		if (A.SectionIndex != B.SectionIndex)
		{
			return A.SectionIndex < B.SectionIndex;
		}
		return FMath::Abs(A.LaneIndex) > FMath::Abs(B.LaneIndex);
	});

	const FScopedTransaction Transaction(LOCTEXT("DeleteLane", "Delete Lane(s)"));

	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	SplineComp->Modify();

	for (const FRoadSectionLaneRef& Lane : Lanes)
	{
		FRoadLaneSection& Section = SplineComp->GetLaneSection(Lane.SectionIndex);
		if (Lane.LaneIndex > 0)
		{
			if (Section.Right.IsValidIndex(Lane.LaneIndex - 1))
			{
				Section.Right.RemoveAt(Lane.LaneIndex - 1);
			}
		}
		else // LaneIndex < 0
		{
			if (Section.Left.IsValidIndex(-Lane.LaneIndex - 1))
			{
				Section.Left.RemoveAt(-Lane.LaneIndex - 1);
			}
		}
	}

	SplineComp->MarkRenderStateDirty();
	SplineComp->UpdateRoadLayout();
	SplineComp->UpdateMagicTransform();
	SplineComp->UpdateLandscape();

	SelectionState->Modify();
	SelectionState->SetSelectedLane(MetaRoad::ZeroLaneIndex);

	GEditor->RedrawLevelEditingViewports(true);
}

void FRoadSectionComponentVisualizer::OnReverseLane()
{
	if (SelectionState->GetStateVerified() < ERoadSectionSelectionState::Lane)
	{
		return;
	}

	TArray<FRoadSectionLaneRef> Lanes = GatherSelectedRealLanes();
	if (Lanes.Num() == 0)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("ReverseLane", "Reverse Lane(s)"));

	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	SplineComp->Modify();

	for (const FRoadSectionLaneRef& Lane : Lanes)
	{
		if (FRoadLane* SelectedLane = SplineComp->GetRoadLane(Lane.SectionIndex, Lane.LaneIndex))
		{
			SelectedLane->Direction = SelectedLane->Direction == ERoadLaneDirection::Default ? ERoadLaneDirection::Invert : ERoadLaneDirection::Default;
		}
	}

	SplineComp->MarkRenderStateDirty();
	SplineComp->UpdateRoadLayout();
	SplineComp->UpdateMagicTransform();
	SplineComp->UpdateLandscape();
	GEditor->RedrawLevelEditingViewports(true);
}

bool FRoadSectionComponentVisualizer::IsLaneReverse()
{
	if (SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Lane)
	{
		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		int SectionIndex = SelectionState->GetSelectedSectionIndex();
		int LaneIndex = SelectionState->GetSelectedLaneIndex();

		if (LaneIndex != MetaRoad::ZeroLaneIndex)
		{
			auto& SelectedSection = GetEditedSplineComponent()->GetLaneSection(SectionIndex);
			auto& SelectedLane = SelectedSection.GetLaneByIndex(LaneIndex);

			return SelectedLane.Direction == ERoadLaneDirection::Invert;
		}
	}
	return false;
}

TSharedPtr<SWidget> FRoadSectionComponentVisualizer::GenerateContextMenu() const
{
	FMenuBuilder MenuBuilder(true, RoadScetionComponentVisualizerActions);

	GenerateContextMenuSections(MenuBuilder);

	TSharedPtr<SWidget> MenuWidget = MenuBuilder.MakeWidget();
	return MenuWidget;
}


void FRoadSectionComponentVisualizer::GenerateContextMenuSections(FMenuBuilder& InMenuBuilder) const
{
	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp != nullptr)
	{
		check(SelectionState);

		auto State = SelectionState->GetStateVerified();

		if (State == ERoadSectionSelectionState::Section || State >= ERoadSectionSelectionState::Lane)
		{
			InMenuBuilder.BeginSection("RoadScection", LOCTEXT("ContextMenuRoadScection", "Road Scection"));
			InMenuBuilder.AddMenuEntry(FRoadSectionComponentVisualizerCommands::Get().SplitFullSection);
			InMenuBuilder.AddMenuEntry(FRoadSectionComponentVisualizerCommands::Get().SplitSideSection, 
				NAME_None, 
				FText::Format( LOCTEXT("ContextMenuRoadScection_SideSplit", "Split {0} section"), 
					SelectionState->GetSelectedLaneIndex() < 0 
					? LOCTEXT("ContextMenuRoadScection_SplitLeftSide", "Left")
					: LOCTEXT("ContextMenuRoadScection_SideRightSide", "Right")
				),
				TAttribute<FText>(),
				SelectionState->GetSelectedLaneIndex() < 0 ? FSlateIcon("MetaRoadEditor", "RoadSectionComponentVisualizer.SplitLeftSection") : FSlateIcon("MetaRoadEditor", "RoadSectionComponentVisualizer.SplitRightSection")
				
			);
			InMenuBuilder.AddMenuEntry(FRoadSectionComponentVisualizerCommands::Get().DeleteSection);
			InMenuBuilder.AddMenuEntry(FRoadSectionComponentVisualizerCommands::Get().CreatProfile);
			InMenuBuilder.EndSection();

			InMenuBuilder.BeginSection("RoadLane", LOCTEXT("ContextMenuRoadLane", "Road Lane"));
			InMenuBuilder.AddMenuEntry(FRoadSectionComponentVisualizerCommands::Get().AddLaneToLeft);
			InMenuBuilder.AddMenuEntry(FRoadSectionComponentVisualizerCommands::Get().AddLaneToRight);
			InMenuBuilder.AddMenuEntry(FRoadSectionComponentVisualizerCommands::Get().DeleteLane);
			InMenuBuilder.AddMenuEntry(FRoadSectionComponentVisualizerCommands::Get().ReverseLane);
			InMenuBuilder.EndSection();
		}

		GenerateChildContextMenuSections(InMenuBuilder);

		InMenuBuilder.PushCommandList(FMetaRoadEditorModule::Get().GetCommandList().ToSharedRef());

		InMenuBuilder.BeginSection("Utils", LOCTEXT("ContextMenuUtils", "Utils"));
		InMenuBuilder.AddMenuEntry(FRoadEditorCommands::Get().FitWidth);
		InMenuBuilder.AddMenuEntry(FRoadEditorCommands::Get().AttachTo);
		InMenuBuilder.EndSection();

		InMenuBuilder.BeginSection("Visualization", LOCTEXT("Visualization", "Visualization"));
		InMenuBuilder.AddMenuEntry(FRoadEditorCommands::Get().HideSelectedSpline);
		InMenuBuilder.AddMenuEntry(FRoadEditorCommands::Get().UnhideAllSpline);
		InMenuBuilder.EndSection();
		
	}
}

#undef LOCTEXT_NAMESPACE