/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "EditorMode/MetaRoadEditorMode.h"
#include "EditorMode/MetaRoadEditorModeToolkit.h"
#include "EditorMode/MetaRoadBakeHost.h"
#include "EditorMode/MetaRoadVisibilitySettings.h" // bDrawBoundaries (debug-line render) + bShowWireframe
#include "EditorMode/MetaRoadFbxExportSettings.h"
#include "EditorMode/MetaRoadFbxExportHost.h"
#include "RoadEditorCommands.h"

#include "Editor.h"
#include "EditorModeManager.h"
#include "Async/Async.h" // AsyncTask (defer mode exit out of Tick)
#include "Tools/EdModeInteractiveToolsContext.h"
#include "InteractiveToolQueryInterfaces.h"
#include "BaseGizmos/TransformGizmoUtil.h"
#include "Snapping/ModelingSceneSnappingManager.h"
#include "Scene/LevelObjectsObserver.h"
#include "Engine/World.h" // FWorldDelegates

#include "LevelEditor.h"
#include "ILevelEditor.h"
#include "SLevelViewport.h"

#include "ModelingTools/DrawRoadTool.h"
#include "MetaRoadEditorModule.h" // FMetaRoadEditorModule::SetEditorModeActive / SetPreviewMode
#include "MetaRoadActor.h"
#include "EngineUtils.h" // TActorIterator
#include "EditorMode/MetaRoadPreviewManager.h"
#include "RoadSplineComponent.h"
#include "Selection.h"

#if METAROAD_PRO
#include "ModelingTools/DrawCrosswalkTool.h"
#include "ModelingTools/DrawChevronMarkingTool.h"
#include "ModelingTools/IntersectionDrawTool.h"
#include "ModelingTools/DrawRoundaboutTool.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaRoadEditorMode)


#define LOCTEXT_NAMESPACE "UMetaRoadEditorMode"


const FEditorModeID UMetaRoadEditorMode::EM_MetaRoadEditorModeId = TEXT("EM_MetaRoadEditorMode");


UMetaRoadEditorMode::UMetaRoadEditorMode()
{
	Info = FEditorModeInfo(
		EM_MetaRoadEditorModeId,
		LOCTEXT("MetaRoadEditorModeName", "Meta Road"),
		FSlateIcon("MetaRoadEditor", "RoadEditor.MetaRoadToolsTabButton", "RoadEditor.MetaRoadToolsTabButton.Small"),
		true);
}

UMetaRoadEditorMode::UMetaRoadEditorMode(FVTableHelper& Helper)
	: UBaseLegacyWidgetEdMode(Helper)
{
}

UMetaRoadEditorMode::~UMetaRoadEditorMode()
{
}

bool UMetaRoadEditorMode::ProcessEditDelete()
{
	if (UEdMode::ProcessEditDelete())
	{
		return true;
	}

	// for now we disable deleting in an Accept-style tool because it can result in crashes if we are deleting target object
	if ( GetToolManager()->HasAnyActiveTool() && GetToolManager()->GetActiveTool(EToolSide::Mouse)->HasAccept() )
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("CannotDeleteWarning", "Cannot delete objects while this Tool is active"), EToolMessageLevel::UserWarning);
		return true;
	}

	return false;
}


bool UMetaRoadEditorMode::ProcessEditCut()
{
	// for now we disable cutting in an Accept-style tool because it can result in crashes if we are deleting target object
	if (GetToolManager()->HasAnyActiveTool() && GetToolManager()->GetActiveTool(EToolSide::Mouse)->HasAccept())
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("CannotCutWarning", "Cannot cut objects while this Tool is active"), EToolMessageLevel::UserWarning);
		return true;
	}

	return false;
}


void UMetaRoadEditorMode::ActorSelectionChangeNotify()
{
	// In Preview mode, follow the selection: build the live mesh preview for the selected road actors.
	RebuildPreviewForSelection();
	// Re-point the left-panel selection editor at the newly selected road.
	RefreshSelectionPanel();
}

void UMetaRoadEditorMode::OnEditorSelectionChanged(UObject* /*NewSelection*/)
{
	RebuildPreviewForSelection();
	RefreshSelectionPanel();
}

void UMetaRoadEditorMode::OnPostUndoRedo()
{
	// Undo/redo restores the visualizer selection state directly (bypassing the setters that keep the module
	// cache in sync), so re-point the selection panel — it reads the live, undo-restored selection from the model.
	RefreshSelectionPanel();
}

void UMetaRoadEditorMode::RefreshSelectionPanel()
{
	if (Toolkit.IsValid())
	{
		StaticCast<FMetaRoadEditorModeToolkit*>(Toolkit.Get())->RefreshSelectionDetailsView();
	}
}

void UMetaRoadEditorMode::SetViewMode(EMetaRoadViewMode InViewMode)
{
	if (ViewMode == InViewMode)
	{
		return;
	}
	ViewMode = InViewMode;

	// Drives line-only schematic rendering (road proxies skip the lane fill in Preview).
	FMetaRoadEditorModule::SetPreviewMode(ViewMode == EMetaRoadViewMode::Preview);

	if (PreviewManager)
	{
		// The mesh-preview pipelines follow the view; the preset-editing session (working copies + preset
		// dropdown) is independent and survives the toggle — the manager only tears down/rebuilds pipelines.
		PreviewManager->SetMeshPreviewEnabled(ViewMode == EMetaRoadViewMode::Preview);
	}

	// Re-sync the manager's actor set from the current selection. The dedup cache no-ops when unchanged, so
	// this only does work when the selection actually differs. In Preview this feeds the mesh pipelines; in
	// Schematic + Preset sub-mode it keeps the working copies tracking the selection.
	RebuildPreviewForSelection();

	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports(true);
	}
}

EMetaRoadPreviewStatus UMetaRoadEditorMode::GetPreviewStatus() const
{
	return (ViewMode == EMetaRoadViewMode::Preview && PreviewManager)
		? PreviewManager->GetPreviewStatus()
		: EMetaRoadPreviewStatus::Idle;
}

bool UMetaRoadEditorMode::HasActivePreview() const
{
	return ViewMode == EMetaRoadViewMode::Preview && PreviewManager && PreviewManager->HasPreview();
}

void UMetaRoadEditorMode::RequestPreviewRebuild()
{
	if (ViewMode == EMetaRoadViewMode::Preview && PreviewManager)
	{
		PreviewManager->RequestRebuildAll();
	}
}

void UMetaRoadEditorMode::CancelPreviewRebuild()
{
	if (ViewMode == EMetaRoadViewMode::Preview && PreviewManager)
	{
		PreviewManager->CancelPreviewBuild();
	}
}

TArray<UObject*> UMetaRoadEditorMode::GetPresetWorkingObjects() const
{
	return PreviewManager ? PreviewManager->GetWorkingSettingsObjects() : TArray<UObject*>();
}

void UMetaRoadEditorMode::ApplyPresetToSelected()
{
	if (PreviewManager)
	{
		PreviewManager->ApplyWorkingToSelected();
	}
}

bool UMetaRoadEditorMode::HasSelectedRoad() const
{
	return GatherSelectedRoadActors().Num() > 0;
}

TArray<TWeakObjectPtr<AMetaRoad>> UMetaRoadEditorMode::GatherSelectedRoadActors() const
{
	TArray<TWeakObjectPtr<AMetaRoad>> RoadActors;
	if (GEditor)
	{
		if (USelection* Selection = GEditor->GetSelectedActors())
		{
			for (FSelectionIterator It(*Selection); It; ++It)
			{
				// Only AMetaRoad actors feed the pipeline; splines on other actor types are ignored.
				if (AMetaRoad* Actor = Cast<AMetaRoad>(*It))
				{
					if (Actor->FindComponentByClass<URoadSplineComponent>())
					{
						RoadActors.Add(Actor);
					}
				}
			}
		}
	}
	return RoadActors;
}

TArray<TWeakObjectPtr<AMetaRoad>> UMetaRoadEditorMode::GatherAllRoadActors() const
{
	TArray<TWeakObjectPtr<AMetaRoad>> RoadActors;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AMetaRoad> It(World); It; ++It)
		{
			if (It->FindComponentByClass<URoadSplineComponent>())
			{
				RoadActors.Add(*It);
			}
		}
	}
	return RoadActors;
}

void UMetaRoadEditorMode::BakeSelected()
{
	BakeActors(GatherSelectedRoadActors());
}

void UMetaRoadEditorMode::BakeAll()
{
	BakeActors(GatherAllRoadActors());
}

void UMetaRoadEditorMode::ExportSelectedToFBX()
{
	ExportActorsToFBX(GatherSelectedRoadActors());
}

void UMetaRoadEditorMode::ExportAllToFBX()
{
	ExportActorsToFBX(GatherAllRoadActors());
}

void UMetaRoadEditorMode::ExportActorsToFBX(const TArray<TWeakObjectPtr<AMetaRoad>>& RoadActors)
{
	// Avoid clashing with an in-flight async bake (which mutates the same _Gen actors) or another export.
	if (IsBaking() || IsExporting())
	{
		return;
	}

	FbxExportHost = NewObject<UMetaRoadFbxExportHost>(this);
	FbxExportHost->BeginExportAsync(GetWorld(), RoadActors, UMetaRoadFbxExportSettings::Get());

	// If the export finished synchronously (e.g. nothing to export), drop the host now.
	if (!FbxExportHost->IsExporting())
	{
		FbxExportHost = nullptr;
	}
}

void UMetaRoadEditorMode::ClearSelected()
{
	if (IsBaking())
	{
		return;
	}
	UMetaRoadBakeHost::ClearGenerated(GatherSelectedRoadActors());
	SetGeneratedActorsHidden(true);
}

void UMetaRoadEditorMode::ClearAll()
{
	if (IsBaking())
	{
		return;
	}
	UMetaRoadBakeHost::ClearGenerated(GatherAllRoadActors());
	SetGeneratedActorsHidden(true);
}

void UMetaRoadEditorMode::BakeActors(const TArray<TWeakObjectPtr<AMetaRoad>>& RoadActors)
{
	// Block re-entry while a bake is already running.
	if (IsBaking() || RoadActors.Num() == 0)
	{
		return;
	}

	// Asynchronous bake: the host builds the meshes in the background and is ticked from Tick() until done
	// (it shows a cook-style notification + Cancel). On completion the _Gen actors are hidden (see Tick()).
	BakeHost = NewObject<UMetaRoadBakeHost>(this);
	BakeHost->BeginBakeAsync(GetWorld(), RoadActors);
}

void UMetaRoadEditorMode::RebuildPreviewForSelection()
{
	// Feed the manager the selected road actors whenever it needs them: in Preview view (to drive the mesh
	// pipelines) or in the Preset sub-mode (to keep the working-copy session tracking the selection even in
	// Schematic view, where the mesh preview is off but the Preset panel/dropdown stay live).
	const bool bPresetSubMode = FMetaRoadSelectionController::Get().GetRoadSelectionMode() == ERoadSelectionMode::Preset;
	if (!PreviewManager || !GEditor || (ViewMode != EMetaRoadViewMode::Preview && !bPresetSubMode))
	{
		return;
	}

	const TArray<TWeakObjectPtr<AMetaRoad>> RoadActors = GatherSelectedRoadActors();

	// Skip if the selected road-actor set is unchanged (selection notifications fire repeatedly).
	auto SameSet = [](const TArray<TWeakObjectPtr<AMetaRoad>>& A, const TArray<TWeakObjectPtr<AMetaRoad>>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (const TWeakObjectPtr<AMetaRoad>& Item : A)
		{
			if (!B.Contains(Item))
			{
				return false;
			}
		}
		return true;
	};

	if (SameSet(RoadActors, PreviewedActors))
	{
		return;
	}

	PreviewedActors = RoadActors;
	PreviewManager->SetPreviewActors(RoadActors);

	// In Preset sub-mode the working copies were rebuilt for the new selection — refresh the panel Details.
	if (PreviewManager->IsPresetEditing() && Toolkit.IsValid())
	{
		StaticCast<FMetaRoadEditorModeToolkit*>(Toolkit.Get())->RefreshPresetPanel();
	}
}


bool UMetaRoadEditorMode::CanAutoSave() const
{
	// prevent autosave if any tool is active
	return GetToolManager()->HasAnyActiveTool() == false;
}

bool UMetaRoadEditorMode::ShouldDrawWidget() const
{
	// hide standard xform gizmo if we have an active tool, unless it explicitly opts in via the IInteractiveToolEditorGizmoAPI
	if (GetInteractiveToolsContext() != nullptr && GetToolManager()->HasAnyActiveTool())
	{
		IInteractiveToolEditorGizmoAPI* GizmoAPI = Cast<IInteractiveToolEditorGizmoAPI>(GetToolManager()->GetActiveTool(EToolSide::Left));
		if (!GizmoAPI || !GizmoAPI->GetAllowStandardEditorGizmos())
		{
			return false;
		}
	}

	return UBaseLegacyWidgetEdMode::ShouldDrawWidget();
}

void UMetaRoadEditorMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	Super::Tick(ViewportClient, DeltaTime);

	// Keep the preview manager's Preset-editing state in sync with the "Preset" Edit sub-mode. Entering it
	// forces Preview display so the working-copy edits are visible.
	if (PreviewManager)
	{
		const bool bPresetSubMode = (FMetaRoadSelectionController::Get().GetRoadSelectionMode() == ERoadSelectionMode::Preset);
		if (PreviewManager->IsPresetEditing() != bPresetSubMode)
		{
			PreviewManager->SetPresetEditing(bPresetSubMode);
			if (bPresetSubMode && ViewMode != EMetaRoadViewMode::Preview)
			{
				SetViewMode(EMetaRoadViewMode::Preview);
			}
			if (Toolkit.IsValid())
			{
				StaticCast<FMetaRoadEditorModeToolkit*>(Toolkit.Get())->RefreshPresetPanel();
			}
		}
	}

	// When the edit sub-mode changes (tile click), rebuild the left-panel selection editor so it shows the
	// matching Spline/Section/Offset/Width/Attribute editor for the same selected road.
	const int32 CurrentSelectionMode = static_cast<int32>(FMetaRoadSelectionController::Get().GetRoadSelectionMode());
	if (CurrentSelectionMode != LastSelectionModeForPanel)
	{
		LastSelectionModeForPanel = CurrentSelectionMode;
		RefreshSelectionPanel();
	}

	if (ViewMode == EMetaRoadViewMode::Preview && PreviewManager)
	{
		PreviewManager->Tick(DeltaTime);

		// Apply the global wireframe debug toggle live (new pipelines pick it up at build time; this
		// handles toggling it on already-built previews).
		const bool bWireframe = UMetaRoadVisibilitySettings::Get()->bShowWireframe;
		if (bWireframe != bLastAppliedWireframe)
		{
			bLastAppliedWireframe = bWireframe;
			PreviewManager->SetWireframe(bWireframe);
		}
	}

	// Drive the asynchronous Bake (any view mode). When it ends, drop the host; on a completed bake (not a
	// cancel/abort) leave the Meta Road mode entirely.
	if (BakeHost)
	{
		if (!BakeHost->TickBake(DeltaTime))
		{
			const bool bCompleted = BakeHost->DidComplete();
			BakeHost = nullptr;

			if (bCompleted)
			{
				// Exit the mode once the bake finishes. Deferred to the next game-thread tick so we don't
				// deactivate the mode from inside its own Tick (Exit() would tear this down mid-loop).
				const FEditorModeID ModeId = EM_MetaRoadEditorModeId;
				AsyncTask(ENamedThreads::GameThread, [ModeId]()
				{
					if (GLevelEditorModeTools().IsModeActive(ModeId))
					{
						GLevelEditorModeTools().DeactivateMode(ModeId);
					}
				});
				return; // mode is going away — don't touch it further this tick
			}

			SetGeneratedActorsHidden(true);
		}
	}

	// Drive the FBX export (one road per tick; shows its own progress notification). Drop the host when done.
	if (FbxExportHost)
	{
		if (!FbxExportHost->TickExport(DeltaTime))
		{
			FbxExportHost = nullptr;
		}
	}

	if (Toolkit.IsValid())
	{
		FMetaRoadEditorModeToolkit* MetaRoadToolkit = (FMetaRoadEditorModeToolkit*)Toolkit.Get();
		MetaRoadToolkit->ShowRealtimeAndModeWarnings(ViewportClient->IsRealtime() == false);
	}
}

void UMetaRoadEditorMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	UBaseLegacyWidgetEdMode::Render(View, Viewport, PDI);

	// Draw the live preview's debug geometry: triangulation boundaries (gated by the Visibility "Draw
	// Boundaries" option) plus any operator DebugDraw lines (e.g. spline-mesh "Draw reference splines",
	// which is always drained so its own toggle controls visibility).
	if (ViewMode == EMetaRoadViewMode::Preview && PreviewManager)
	{
		PreviewManager->RenderDebugLines(PDI, UMetaRoadVisibilitySettings::Get()->bDrawBoundaries);
	}
}

void UMetaRoadEditorMode::Enter()
{
	UEdMode::Enter();

	// forward shutdown requests
	GetToolManager()->OnToolShutdownRequest.BindLambda([this](UInteractiveToolManager*, UInteractiveTool* Tool, EToolShutdownType ShutdownType)
	{
		GetInteractiveToolsContext()->EndTool(ShutdownType);
		return true;
	});

	// register gizmo helper
	UE::TransformGizmoUtil::RegisterTransformGizmoContextObject(GetInteractiveToolsContext());

	// register snapping manager
	UE::Geometry::RegisterSceneSnappingManager(GetInteractiveToolsContext());
	SceneSnappingManager = UE::Geometry::FindModelingSceneSnappingManager(GetToolManager());

	// register level objects observer that will update the snapping manager as the scene changes
	LevelObjectsObserver = MakeShared<FLevelObjectsObserver>();
	LevelObjectsObserver->OnActorAdded.AddLambda([this](AActor* Actor)
	{
		if (SceneSnappingManager)
		{
			SceneSnappingManager->OnActorAdded(Actor, [](UPrimitiveComponent*) { return true; });
		}
	});
	LevelObjectsObserver->OnActorRemoved.AddLambda([this](AActor* Actor)
	{
		if (SceneSnappingManager)
		{
			SceneSnappingManager->OnActorRemoved(Actor);
		}
	});
	// observer will auto-populate w/ the current level, but must have registered the handlers first!
	LevelObjectsObserver->Initialize(GetWorld());

	// record switching behavior to restore on exit
	ToolSwitchModeToRestoreOnExit = GetInteractiveToolsContext()->ToolManager->GetToolSwitchMode();
	// default to NOT applying changes when switching between tools without accepting
	GetInteractiveToolsContext()->ToolManager->SetToolSwitchMode(EToolManagerToolSwitchMode::CancelIfAble);

	// register tools
	const FRoadEditorCommands& ToolManagerCommands = FRoadEditorCommands::Get();
	RegisterTool(ToolManagerCommands.BeginDrawNewRoad, TEXT("BeginDrawNewRoad"), NewObject<UDrawRoadToolBuilder>());

#if METAROAD_PRO
	RegisterTool(ToolManagerCommands.BeginDrawIntersection, TEXT("BeginDrawIntersection"), NewObject<UIntersectionDrawToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginDrawCrosswalk, TEXT("BeginDrawCrosswalk"), NewObject<UDrawCrosswalkToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginDrawChevronMarking, TEXT("BeginDrawChevronMarking"), NewObject<UDrawChevronMarkingToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginDrawRoundabout, TEXT("BeginDrawRoundabout"), NewObject<UDrawRoundaboutToolBuilder>());
#endif

	// enable realtime viewport override
	ConfigureRealTimeViewportsOverride(true);

	// Enable road-spline visualization/editing only while the mode is active, and hide the
	// previously generated _Gen meshes so they don't compete with the schematic/preview.
	// The mode-active flag makes unselected road authoring primitives visible (outside the
	// mode only selected ones render — see FRoadSplineSceneProxy::GetViewRelevance).
	FMetaRoadEditorModule::SetEditorModeActive(true);
	FMetaRoadEditorModule::Get().ActivateEditing();
	SetGeneratedActorsHidden(true);

	// Live mesh-preview driver for the Preview view mode (idle until SetViewMode(Preview)).
	PreviewManager = NewObject<UMetaRoadPreviewManager>(this);
	PreviewManager->Initialize(GetWorld());
	ViewMode = EMetaRoadViewMode::Schematic;
	FMetaRoadEditorModule::SetPreviewMode(false);

	GEditor->RedrawLevelEditingViewports(true);

	EditorClosedEventHandle = GEditor->OnEditorClose().AddUObject(this, &UMetaRoadEditorMode::OnEditorClosed);

	// Follow selection changes to refresh the live preview (reliable across actor/component picks).
	SelectionChangedHandle = USelection::SelectionChangedEvent.AddUObject(this, &UMetaRoadEditorMode::OnEditorSelectionChanged);
	PostUndoRedoHandle = FEditorDelegates::PostUndoRedo.AddUObject(this, &UMetaRoadEditorMode::OnPostUndoRedo);

	// Removing levels from the world garbage-collects any temporary actors we may have spawned for
	// visualization/gizmos, so exit to the default mode when that happens.
	FWorldDelegates::PreLevelRemovedFromWorld.AddWeakLambda(this, [this](ULevel*, UWorld*)
	{
		GetModeManager()->ActivateDefaultMode();
	});
}

void UMetaRoadEditorMode::Exit()
{
	FWorldDelegates::PreLevelRemovedFromWorld.RemoveAll(this);
	USelection::SelectionChangedEvent.Remove(SelectionChangedHandle);
	FEditorDelegates::PostUndoRedo.Remove(PostUndoRedoHandle);
	PostUndoRedoHandle.Reset();
	SelectionChangedHandle.Reset();

	// Symmetric with Enter(): drop the OnEditorClose binding so re-entering the mode does not stack
	// duplicate bindings (the handle is also removed in OnEditorClosed for the editor-shutdown path).
	if (EditorClosedEventHandle.IsValid() && GEditor)
	{
		GEditor->OnEditorClose().Remove(EditorClosedEventHandle);
	}
	EditorClosedEventHandle.Reset();

	// Tear down the live preview (destroys preview meshes) before leaving the mode.
	if (PreviewManager)
	{
		PreviewManager->ClearPreview();
		PreviewManager = nullptr;
	}
	ViewMode = EMetaRoadViewMode::Schematic;
	FMetaRoadEditorModule::SetPreviewMode(false);
	PreviewedActors.Reset();

	// Restore generated _Gen meshes and tear down road-spline visualization/editing.
	// Clearing the mode-active flag hides unselected road authoring primitives again.
	SetGeneratedActorsHidden(false);
	FMetaRoadEditorModule::SetEditorModeActive(false);
	FMetaRoadEditorModule::Get().DeactivateEditing();
	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports(true);
	}

	// exit any active tool with cancel
	GetToolManager()->DeactivateTool(EToolSide::Left, EToolShutdownType::Cancel);

	// deregister snapping manager and shut down level objects observer
	LevelObjectsObserver->Shutdown();		// do this first because it is going to fire events on the snapping manager
	LevelObjectsObserver.Reset();
	UE::Geometry::DeregisterSceneSnappingManager(GetInteractiveToolsContext());
	SceneSnappingManager = nullptr;

	// clear realtime viewport override
	ConfigureRealTimeViewportsOverride(false);

	// restore previous tool switching behavior
	GetInteractiveToolsContext()->ToolManager->SetToolSwitchMode(ToolSwitchModeToRestoreOnExit);

	// Call base Exit method to ensure proper cleanup
	UEdMode::Exit();
}

void UMetaRoadEditorMode::OnEditorClosed()
{
	// On editor close, Exit() should run to clean up, but this happens very late.
	// Close out any active Tools to mitigate any late-destruction issues.
	if (GetModeManager() != nullptr
		&& GetInteractiveToolsContext() != nullptr
		&& GetToolManager() != nullptr
		&& GetToolManager()->HasAnyActiveTool())
	{
		GetToolManager()->DeactivateTool(EToolSide::Mouse, EToolShutdownType::Cancel);
	}

	if (EditorClosedEventHandle.IsValid() && GEditor)
	{
		GEditor->OnEditorClose().Remove(EditorClosedEventHandle);
		EditorClosedEventHandle.Reset();
	}
}

bool UMetaRoadEditorMode::ShouldToolStartBeAllowed(const FString& ToolIdentifier) const
{
	if (UInteractiveToolManager* Manager = GetToolManager())
	{
		if (UInteractiveTool* Tool = Manager->GetActiveTool(EToolSide::Left))
		{
			IInteractiveToolExclusiveToolAPI* ExclusiveAPI = Cast<IInteractiveToolExclusiveToolAPI>(Tool);
			if (ExclusiveAPI)
			{
				return false;
			}
		}
	}
	return Super::ShouldToolStartBeAllowed(ToolIdentifier);
}

bool UMetaRoadEditorMode::BoxSelect(FBox& InBox, bool InSelect)
{
	// not handling yet
	return false;
}

bool UMetaRoadEditorMode::FrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, bool InSelect)
{
	// Disable FrustumSelect when a tool is active
	if (!Toolkit.IsValid() || StaticCast<FMetaRoadEditorModeToolkit*>(Toolkit.Get())->IsInActiveTool() )
	{
		return true;
	}

	// not handling yet
	return false;
}

void UMetaRoadEditorMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FMetaRoadEditorModeToolkit);
}

FBox UMetaRoadEditorMode::ComputeCustomViewportFocus() const
{
	// prefer a slightly farther-out focus
	auto ProcessFocusBoxFunc = [](FBox& FocusBoxInOut)
	{
		double MaxDimension = FocusBoxInOut.GetExtent().GetMax();
		FocusBoxInOut = FocusBoxInOut.ExpandBy(MaxDimension * 0.2);
	};

	FBox FocusBox = Super::ComputeCustomViewportFocus();
	if (FocusBox.IsValid)
	{
		ProcessFocusBoxFunc(FocusBox);
		return FocusBox;
	}

	// did not set a focus box, return a default (invalid) box
	return FBox();
}

bool UMetaRoadEditorMode::HasCustomViewportFocus() const
{
	if (Super::HasCustomViewportFocus())
	{
		return true;
	}

	// no mode-specific focus behavior
	return false;
}


bool UMetaRoadEditorMode::GetPivotForOrbit(FVector& OutPivot) const
{
	if (GCurrentLevelEditingViewportClient)
	{
		OutPivot = GCurrentLevelEditingViewportClient->GetViewTransform().GetLookAt();
		return true;
	}
	return false;
}


void UMetaRoadEditorMode::SetGeneratedActorsHidden(bool bHidden)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AMetaRoad> It(World); It; ++It)
	{
		AMetaRoad* RoadActor = *It;
		// Actors that were never baked have no _Gen actor (LastGeneratedActor is null) and are skipped.
		if (IsValid(RoadActor) && IsValid(RoadActor->LastGeneratedActor))
		{
			RoadActor->LastGeneratedActor->SetIsTemporarilyHiddenInEditor(bHidden);
		}
	}
}


void UMetaRoadEditorMode::ConfigureRealTimeViewportsOverride(bool bEnable)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
	if (LevelEditor.IsValid())
	{
		TArray<TSharedPtr<SLevelViewport>> Viewports = LevelEditor->GetViewports();
		for (const TSharedPtr<SLevelViewport>& ViewportWindow : Viewports)
		{
			if (ViewportWindow.IsValid())
			{
				FEditorViewportClient& Viewport = ViewportWindow->GetAssetViewportClient();
				const FText SystemDisplayName = LOCTEXT("RealtimeOverrideMessage_MetaRoad", "Meta Road");
				if (bEnable)
				{
					Viewport.AddRealtimeOverride(bEnable, SystemDisplayName);
				}
				else
				{
					Viewport.RemoveRealtimeOverride(SystemDisplayName, false);
				}
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE
