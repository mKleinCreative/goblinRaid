/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Tools/LegacyEdModeWidgetHelpers.h"
#include "InteractiveToolManager.h"
#include "MetaRoadEditorMode.generated.h"

class FLevelObjectsObserver;
class UModelingSceneSnappingManager;
class UMetaRoadPreviewManager;
class UMetaRoadBakeHost;
class UMetaRoadFbxExportHost;
class AMetaRoad;
enum class EMetaRoadPreviewStatus : uint8; // defined in MetaRoadPreviewManager.h

/** What the mode displays: the editable lane schematic, or a live generated-mesh preview. */
enum class EMetaRoadViewMode : uint8
{
	Schematic,
	Preview
};

UCLASS(Transient, MinimalAPI)
class UMetaRoadEditorMode : public UBaseLegacyWidgetEdMode, public ILegacyEdModeSelectInterface
{
	GENERATED_BODY()
public:
	METAROADEDITOR_API const static FEditorModeID EM_MetaRoadEditorModeId;

	EMetaRoadViewMode GetViewMode() const { return ViewMode; }
	METAROADEDITOR_API void SetViewMode(EMetaRoadViewMode InViewMode);

	// Preview status / actions surfaced by the toolkit's preview controls (status icon + Update button,
	// next to the Schematic/Preview toggle).
	METAROADEDITOR_API EMetaRoadPreviewStatus GetPreviewStatus() const;
	METAROADEDITOR_API bool HasActivePreview() const;
	METAROADEDITOR_API void RequestPreviewRebuild();
	/** Cancel the in-progress preview build (background compute) without dropping the preview meshes. */
	METAROADEDITOR_API void CancelPreviewRebuild();

	// Preset sub-mode: the transient working-copy holders edited in the Preset panel, and the commit action.
	METAROADEDITOR_API TArray<UObject*> GetPresetWorkingObjects() const;
	METAROADEDITOR_API void ApplyPresetToSelected();

	// Bake: generate road mesh assets/actors (replaces UTriangulateRoadTool's Accept). Bake Selected
	// targets the selected road actors; Bake All targets every AMetaRoad in the world.
	METAROADEDITOR_API void BakeSelected();
	METAROADEDITOR_API void BakeAll();
	METAROADEDITOR_API bool HasSelectedRoad() const;

	// FBX Export: write the baked (or freshly generated) road meshes to .fbx files. Export Selected targets the
	// selected road actors; Export All targets every AMetaRoad in the world. Driven by UMetaRoadFbxExportSettings.
	METAROADEDITOR_API void ExportSelectedToFBX();
	METAROADEDITOR_API void ExportAllToFBX();

	// AMetaRoad actors (owning a URoadSplineComponent) in the current selection. Splines on non-AMetaRoad
	// actors are intentionally not gathered (ignored by the pipeline). Used by the toolkit's Selection panel.
	METAROADEDITOR_API TArray<TWeakObjectPtr<AMetaRoad>> GatherSelectedRoadActors() const;

	// Clear: delete the generated _Gen actors + mesh assets for the selected / all road actors (the cleanup
	// Bake does up front), without generating anything.
	METAROADEDITOR_API void ClearSelected();
	METAROADEDITOR_API void ClearAll();

	// True while an asynchronous Bake is running (used to disable Bake/Clear and block re-entry).
	METAROADEDITOR_API bool IsBaking() const { return BakeHost != nullptr; }

	// True while an FBX export is running (used to disable the FBX Export buttons and block re-entry).
	METAROADEDITOR_API bool IsExporting() const { return FbxExportHost != nullptr; }

	UMetaRoadEditorMode();
	UMetaRoadEditorMode(FVTableHelper& Helper);
	~UMetaRoadEditorMode();

	////////////////
	// UEdMode interface
	////////////////

	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;

	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;

	virtual void ActorSelectionChangeNotify() override;

	virtual bool ShouldDrawWidget() const override;
	virtual bool ProcessEditDelete() override;
	virtual bool ProcessEditCut() override;

	virtual bool CanAutoSave() const override;

	virtual bool GetPivotForOrbit(FVector& OutPivot) const override;

	// called when we "start"/"end" this editor mode (ie switch tabs)
	virtual void Enter() override;
	virtual void Exit() override;

	virtual bool ShouldToolStartBeAllowed(const FString& ToolIdentifier) const override;

	//////////////////
	// End of UEdMode interface
	//////////////////

	// ILegacyEdModeSelectInterface
	virtual bool BoxSelect(FBox& InBox, bool InSelect = true) override;
	virtual bool FrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, bool InSelect) override;

	// Manage viewport focus
	virtual bool HasCustomViewportFocus() const override;
	virtual FBox ComputeCustomViewportFocus() const override;

protected:
	virtual void CreateToolkit() override;

	FDelegateHandle EditorClosedEventHandle;
	void OnEditorClosed();

	// Reliable selection-change hook (USelection::SelectionChangedEvent) to refresh the Preview.
	FDelegateHandle SelectionChangedHandle;
	void OnEditorSelectionChanged(UObject* NewSelection);

	// Undo/redo restores the visualizer selection state (SplinePropertyPath etc.) byte-level, bypassing the
	// selection setters — so re-point the selection panel afterward (it reads the live selection from the model).
	FDelegateHandle PostUndoRedoHandle;
	void OnPostUndoRedo();

	TSharedPtr<FLevelObjectsObserver> LevelObjectsObserver;

	UPROPERTY()
	TObjectPtr<UModelingSceneSnappingManager> SceneSnappingManager;

	void ConfigureRealTimeViewportsOverride(bool bEnable);

	// Show/hide every AMetaRoad's previously generated _Gen actor while the mode is active,
	// so the schematic/preview is not obscured by baked geometry. Uses temporary editor
	// visibility (not serialized, reset on reload) and AMetaRoad::LastGeneratedActor as the
	// source of truth (more reliable than matching the _Gen name suffix).
	void SetGeneratedActorsHidden(bool bHidden);

	// In Preview view mode, (re)build the live mesh preview for the currently selected road actors.
	// No-op in Schematic mode. Deduplicated so repeated selection notifications don't thrash.
	void RebuildPreviewForSelection();

	// Re-point/rebuild the toolkit's embedded road-selection details view (left panel). Called on selection
	// change and when the edit sub-mode changes (so the correct Spline/Section/Offset/Width/Attribute editor
	// shows for the selected road).
	void RefreshSelectionPanel();

	// Asynchronously bake the given road actors (via a transient UMetaRoadBakeHost, ticked from Tick()) and
	// keep their new _Gen output hidden while the mode stays active.
	void BakeActors(const TArray<TWeakObjectPtr<AMetaRoad>>& RoadActors);

	// Export the given road actors to FBX via FRoadFbxExporter (driven by UMetaRoadFbxExportSettings).
	void ExportActorsToFBX(const TArray<TWeakObjectPtr<AMetaRoad>>& RoadActors);

	// All AMetaRoad actors (owning a URoadSplineComponent) in the world. (GatherSelectedRoadActors is public.)
	TArray<TWeakObjectPtr<AMetaRoad>> GatherAllRoadActors() const;

private:
	// we restore previous switch tool behavior when exiting this mode
	EToolManagerToolSwitchMode ToolSwitchModeToRestoreOnExit;

	EMetaRoadViewMode ViewMode = EMetaRoadViewMode::Schematic;

	// Drives live road-mesh preview for selected actors while in Preview view mode.
	UPROPERTY()
	TObjectPtr<UMetaRoadPreviewManager> PreviewManager;

	// Road actors currently fed to the preview (to skip redundant rebuilds on selection churn).
	TArray<TWeakObjectPtr<AMetaRoad>> PreviewedActors;

	// Drives the asynchronous Bake while it runs (null when idle). Ticked from Tick(); see IsBaking().
	UPROPERTY()
	TObjectPtr<UMetaRoadBakeHost> BakeHost;

	// Drives the FBX export while it runs (null when idle). Ticked from Tick(); see IsExporting().
	UPROPERTY()
	TObjectPtr<UMetaRoadFbxExportHost> FbxExportHost;

	// Last ERoadSelectionMode (as int32, to avoid including the module header here) seen in Tick — used to
	// refresh the selection panel when the edit sub-mode changes. -1 = uninitialized.
	int32 LastSelectionModeForPanel = -1;

	// Last applied value of UMetaRoadVisibilitySettings::bShowWireframe — drives live wireframe toggling
	// of the preview from Tick (see UMetaRoadEditorMode::Tick).
	bool bLastAppliedWireframe = false;
};
