/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Templates/SubclassOf.h"
#include "EditorMode/MetaRoadSelectionController.h" // FMetaRoadSelectionController + ERoadSelectionMode (used by Activate/DeactivateEditing; re-exported to module-header includers)

#if METAROAD_PRO
#include "Utils/ZoneGraphBuilder.h"
#endif

class FUICommandList;
struct FZoneGraphBuildData;

class METAROADEDITOR_API FMetaRoadEditorModule : public IModuleInterface
{
public:
		
	static inline FMetaRoadEditorModule& Get() { return FModuleManager::LoadModuleChecked< FMetaRoadEditorModule >("MetaRoadEditor"); }

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// Editing activation is driven by UMetaRoadEditorMode::Enter()/Exit(). These delegate to
	// FMetaRoadSelectionController: the default road-spline visualizer stays registered even outside
	// the mode (registered at startup, restored on Exit), while the crosswalk visualizer (Pro) is
	// mode-gated here. Both delegate the road-spline selection lifecycle to the controller.
	void ActivateEditing();
	void DeactivateEditing();

	TSharedPtr<FUICommandList> GetCommandList() const { return CommandList; }

	static bool IsTileRendersVisibleInEditor() { return bIsTileRendersVisibleInEditor; }
	static void SetIsTileRendersVisibleInEditor(bool bInbIsTileRendersVisibleInEditor) { bIsTileRendersVisibleInEditor = bInbIsTileRendersVisibleInEditor; }

	// True while the Meta Road editor mode is active. Drives editor visibility of authoring primitives
	// (road splines, crosswalk/chevron markings): outside the mode only the SELECTED ones are shown and
	// inside the mode all of them are shown. This restriction is applied by the scene proxies only in
	// the main level-editor world (they gate on EWorldType::Editor), so that in asset & Blueprint editor
	// preview scenes, PIE, game and thumbnails the primitives always render. Set by
	// UMetaRoadEditorMode::Enter/Exit.
	static bool IsEditorModeActive() { return bIsEditorModeActive; }
	static void SetEditorModeActive(bool bInActive) { bIsEditorModeActive = bInActive; }

	// True while the editor mode is in Preview display mode: road-spline scene proxies draw only the
	// lane/section boundary lines (no filled lane polygons), so the generated preview mesh is visible
	// underneath. Set by UMetaRoadEditorMode::SetViewMode/Exit.
	static bool IsPreviewMode() { return bIsPreviewMode; }
	static void SetPreviewMode(bool bInPreview) { bIsPreviewMode = bInPreview; }

	// True while at least one URoadSplineComponent is selected in the editor. Read on the render thread by
	// FRoadSplineSceneProxy to dim the unselected roads so the selected one stands out. Self-managed:
	// recomputed from USelection::SelectionChangedEvent (bound for the module lifetime in OnPostEngineInit).
	static bool IsAnyRoadSplineSelected() { return bAnyRoadSplineSelected; }

	static const FName AssetCategoryName;

protected:
	void BindCommands();
	void OnPreExit();
	void OnPostEngineInit();

	// Recomputes bAnyRoadSplineSelected from the current editor component selection. Bound to
	// USelection::SelectionChangedEvent for the module lifetime (OnPostEngineInit -> OnPreExit).
	void OnEditorSelectionChanged(UObject* NewSelection);

	// Startup/shutdown helpers — keep each Register*/Bind* paired with its Unregister*/Unbind*.
	void RegisterDetailCustomizations();
	void UnregisterDetailCustomizations();
	void RegisterThumbnailRenderers();
	void BindRuntimeDelegates();
	void UnbindRuntimeDelegates();

#if METAROAD_PRO
	void BindZoneGraphDelegates();
	void UnbindZoneGraphDelegates();
	void OnZoneGraphRequestRebuild();
	void OnZoneGraphDataBuildDone(const FZoneGraphBuildData& /*BuildData*/);
#endif

	TSharedPtr<FUICommandList> CommandList;

	static bool bIsTileRendersVisibleInEditor;
	static bool bIsEditorModeActive;
	static bool bIsPreviewMode;
	static bool bAnyRoadSplineSelected;

#if METAROAD_PRO
	MetaRoad::ZoneGraph::FZoneGraphBuilder ZoneGraphBuilder;
	FDelegateHandle OnZoneGraphRequestRebuildHandle;
	FDelegateHandle OnZoneGraphDataBuildDoneHandle;
	FDelegateHandle OnRoadGraphRegistredHandle;
	FDelegateHandle OnRoadGraphUnregistredHandle;
	bool bPreventGraphDataBuildDone = false;
#endif
	FDelegateHandle OnAttributeBeginDestroydHandle;
	FDelegateHandle SelectionChangedHandle;

	bool bIsExiting = false;
};
