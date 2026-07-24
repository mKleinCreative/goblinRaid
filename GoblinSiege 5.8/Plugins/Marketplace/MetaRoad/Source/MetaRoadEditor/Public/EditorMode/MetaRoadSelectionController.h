/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/StrongObjectPtr.h"
#include "EditorMode/MetaRoadSelectionModel.h" // UMetaRoadSelectionModel (complete type for TStrongObjectPtr member)

class FComponentVisualizer;
class URoadSplineComponent;
class URoadLaneAttributeDescriptor;

// Which road-editing sub-mode is active (drives the active component visualizer + the toolkit palette/panel).
enum class ERoadSelectionMode
{
	None,
	Spline,
	Section,
	Offset,
	Width,
	Attribute,
	Preset,
	Bake,
	FbxExport,
	Visibility
};

/**
 * FMetaRoadSelectionController
 *
 * Owns the road-editing "selection / sub-mode state machine" that used to live on FMetaRoadEditorModule: the
 * active sub-mode + attribute descriptor, the registered road-spline FComponentVisualizer, and the single
 * source of truth for the currently-selected road spline (cache + OnSelectedSplineChanged delegate +
 * UMetaRoadSelectionModel owning the per-mode selection states). The editor module stays a thin coordinator
 * and delegates its selection lifecycle here (ActivateVisualizers / EndSelection).
 *
 * Process-wide singleton (Get()); its selection state is created/reset around the editor mode's Enter/Exit.
 */
class METAROADEDITOR_API FMetaRoadSelectionController
{
public:
	static FMetaRoadSelectionController& Get();

	// Sub-mode setters — set RoadSelectionMode and (re)register the matching road-spline visualizer.
	void SetSplineEditorMode();
	void SetSectionEditorMode();
	void SetOffsetEditorMode();
	void SetWidthEditorMode();
	void SetAttributeEditorMode(const TSubclassOf<URoadLaneAttributeDescriptor>& AttributeDescriptor);
	void SetPresetEditorMode();
	void SetBakeEditorMode();
	void SetFbxExportEditorMode();
	void SetVisibilityEditorMode();

	ERoadSelectionMode GetRoadSelectionMode() const { return RoadSelectionMode; }
	const TSubclassOf<URoadLaneAttributeDescriptor>& GetSelectedAttributeDescriptor() const { return SelectionAttributeDescriptor; }

	// While an interactive tool (Create category) runs, the edit sub-mode tiles show unchecked.
	bool IsInteractiveToolActive() const { return bIsInteractiveToolActive; }
	void SetInteractiveToolActive(bool bInActive) { bIsInteractiveToolActive = bInActive; }

	TSharedPtr<FComponentVisualizer> GetComponentVisualizer() const { return ComponentVisualizer; }

	// Single source of truth for "the road spline the active visualizer currently has selected".
	DECLARE_MULTICAST_DELEGATE(FOnSelectedSplineChanged);
	FOnSelectedSplineChanged& OnSelectedSplineChanged() { return OnSelectedSplineChangedDelegate; }
	void SetCurrentSelectedSpline(URoadSplineComponent* InSpline);
	URoadSplineComponent* GetCurrentSelectedSpline() const { return CurrentSelectedSpline.Get(); }
	// The road spline the active sub-mode's selection state currently points at, read live from the model
	// (always correct, incl. after undo/redo and across sub-mode swaps). Null when nothing/non-spline is selected.
	URoadSplineComponent* GetSelectedSplineForActiveMode();

	// Owner of the visualizer selection state (created lazily; released in EndSelection). Visualizers borrow
	// their per-mode state from it instead of owning their own, so selection outlives the sub-mode swap.
	UMetaRoadSelectionModel* GetSelectionModel();

	// Register the default road-spline visualizer (FRoadSplineComponentVisualizer). It stays registered even
	// OUTSIDE the MetaRoad editor mode (from module startup on), so a selected road spline is always editable at
	// the spline-point level; inside the mode the sub-mode setters swap it, and mode exit restores it here.
	void RegisterDefaultVisualizer();
	// Full teardown (module shutdown): unregister the road-spline visualizer and release the selection model.
	void EndSelection();

	// Cancel any active interactive tool in the Meta Road editor mode (so picking an edit sub-mode tile exits
	// the current Create tool instead of leaving both selected).
	void DeactivateActiveTool();

private:
	void SetComponentVisualizer(TSharedRef<FComponentVisualizer> Visualizer);

	ERoadSelectionMode RoadSelectionMode = ERoadSelectionMode::None;
	TSubclassOf<URoadLaneAttributeDescriptor> SelectionAttributeDescriptor;

	TSharedPtr<FComponentVisualizer> ComponentVisualizer;

	// Cached "currently selected road spline" (single source of truth). Weak: the controller is not a UObject.
	TWeakObjectPtr<URoadSplineComponent> CurrentSelectedSpline;
	FOnSelectedSplineChanged OnSelectedSplineChangedDelegate;
	// Set only while swapping/releasing the component visualizer — suppresses OnSelectedSplineChanged so a
	// teardown push doesn't re-enter the Selection panel while RoadSelectionMode and ComponentVisualizer are
	// momentarily out of sync (see SetComponentVisualizer / EndSelection).
	bool bUpdatingComponentVisualizer = false;

	// Owns the visualizer selection-state objects (see GetSelectionModel / UMetaRoadSelectionModel).
	TStrongObjectPtr<UMetaRoadSelectionModel> SelectionModel;

	bool bIsInteractiveToolActive = false;
};
