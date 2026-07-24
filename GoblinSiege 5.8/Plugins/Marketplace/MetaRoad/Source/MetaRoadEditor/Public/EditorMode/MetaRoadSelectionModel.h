/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MetaRoadSelectionModel.generated.h"

class URoadSectionComponentVisualizerSelectionState;
class URoadOffsetComponentVisualizerSelectionState;
class URoadSplineComponentVisualizerSelectionState;

/**
 * UMetaRoadSelectionModel
 *
 * Module-owned owner of the road-spline visualizer selection state. Instead of each FComponentVisualizer
 * NewObject-ing + GC-rooting its own per-mode selection-state object (which is destroyed on every sub-mode
 * swap), the model owns the three state objects as sub-objects and hands them out. The active visualizer
 * borrows the matching state; because the state now outlives the visualizer swap, the mid-swap "teardown
 * push" hazard disappears.
 *
 * Owned by FMetaRoadEditorModule via a TStrongObjectPtr (created lazily by GetSelectionModel(), released in
 * DeactivateEditing). The Section state is shared by the Section/Width/Attribute sub-modes (their visualizers
 * all use URoadSectionComponentVisualizerSelectionState), matching the pre-refactor behaviour.
 */
UCLASS(Transient)
class METAROADEDITOR_API UMetaRoadSelectionModel : public UObject
{
	GENERATED_BODY()

public:
	// Lazily create-or-return each per-mode selection state (RF_Transactional, outered to this model).
	URoadSectionComponentVisualizerSelectionState* GetSectionState();
	URoadOffsetComponentVisualizerSelectionState* GetOffsetState();
	URoadSplineComponentVisualizerSelectionState* GetSplineState();

private:
	UPROPERTY()
	TObjectPtr<URoadSectionComponentVisualizerSelectionState> SectionState;

	UPROPERTY()
	TObjectPtr<URoadOffsetComponentVisualizerSelectionState> OffsetState;

	UPROPERTY()
	TObjectPtr<URoadSplineComponentVisualizerSelectionState> SplineState;
};
