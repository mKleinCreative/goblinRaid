/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RoadSplineComponentVisualizer.h"
#include "RoadLaneAttribute.h"
#include "Templates/Requires.h"
#include "RoadSectionComponentVisualizer.generated.h"

class URoadLaneAttributeDescriptor;
class FRoadKeyOverlayController;

UENUM()
enum class ERoadSectionSelectionState : uint8
{
	None = 0,
	Component,
	/** The filled interior of a closed spline (FRoadLayout::LoopedRoadZone). A sibling of Section — needs a
	 *  valid spline that IsClosedLoop() with a valid LoopedRoadZone; carries no section/lane/key. Ordered
	 *  BELOW Section on purpose: the many "State >= Section" / ">= Lane" checks across the visualizer
	 *  (context menu CanExecute, the SOffset overlay, Delete/Add/Split Section, …) must treat Loop as NOT a
	 *  section/lane selection — otherwise they would run section handlers with SelectedSectionIndex == INDEX_NONE. */
	Loop,
	Section,
	Lane,
	Key,
	KeyTangent,
};

/** Identifies a single lane as (SectionIndex, LaneIndex). Used for the multi-lane selection set. */
USTRUCT()
struct FRoadSectionLaneRef
{
	GENERATED_BODY()

	UPROPERTY()
	int32 SectionIndex = INDEX_NONE;

	UPROPERTY()
	int32 LaneIndex = MetaRoad::ZeroLaneIndex;

	FRoadSectionLaneRef() = default;
	FRoadSectionLaneRef(int32 InSectionIndex, int32 InLaneIndex)
		: SectionIndex(InSectionIndex), LaneIndex(InLaneIndex) {}

	bool operator==(const FRoadSectionLaneRef& Other) const
	{
		return SectionIndex == Other.SectionIndex && LaneIndex == Other.LaneIndex;
	}
};

/** Selection state data that will be captured by scoped transactions.*/
UCLASS(Transient)
class URoadSectionComponentVisualizerSelectionState : public UObject
{
	GENERATED_BODY()

public:
	void ResetSelection(bool bSaveSplineSelection);
	void SetSelectedSpline(FComponentPropertyPath& SplinePropertyPath);
	void SetSelectedSection(int32 SectionIndex);
	void SetSelectedLane(int32 LaneIndex);
	/** Select the looped fill area of the (already-selected) closed spline. */
	void SetSelectedLoop();
	void SetSelectedAttributeDescriptor(const TSubclassOf<URoadLaneAttributeDescriptor>& AttributeDescriptor);
	void SetSelectedKeyIndex(int32 KeyIndex);
	void SetSelectedTangent(ESelectedTangentHandle TangentHandle);

	const FComponentPropertyPath GetSplinePropertyPath() const { return SplinePropertyPath; }
	URoadSplineComponent* GetSelectedSpline() const { return SplinePropertyPath.IsValid() ? Cast<URoadSplineComponent>(SplinePropertyPath.GetComponent()) : nullptr; }
	int32 GetSelectedSectionIndex() const { return SelectedSectionIndex; }
	int32 GetSelectedLaneIndex() const { return SelectedLaneIndex; }

	/** All currently selected lanes (multi-selection). When the state is >= Lane, the primary
	 *  (SelectedSectionIndex, SelectedLaneIndex) is always one of these. */
	const TArray<FRoadSectionLaneRef>& GetSelectedLanes() const { return SelectedLanes; }
	bool IsLaneInSelection(int32 SectionIndex, int32 LaneIndex) const { return SelectedLanes.Contains(FRoadSectionLaneRef(SectionIndex, LaneIndex)); }

	/** Ctrl+Click handler: add/remove a lane from the selection set. The toggled (or, on removal, a
	 *  remaining) lane becomes the primary; emptying the set drops to Section state. */
	void ToggleLane(int32 SectionIndex, int32 LaneIndex);
	const TSubclassOf<URoadLaneAttributeDescriptor>& GetSelectedAttributeDescriptor() const { return SelectedAttributeDescriptor; }
	int32 GetSelectedKeyIndex() const { return SelectedKeyIndex; }
	ESelectedTangentHandle GetSelectedTangent() const { return SelectedTangentHandleType; }

	void SetCashedData(const FVector& Position, const FQuat& Rotation, float SplineKey);
	void SetCashedDataAtSplineDistance(float S);
	void SetCashedDataAtSplineInputKey(float Key);
	void SetCashedDataAtLane(int SectionIndex, int LaneIndex, double SOffset, double Aplha);
	void ResetCahedData();

	FVector GetCashedPosition() const { return CahedPosition; }
	FQuat GetCachedRotation() const { return CachedRotation; }
	float GetCachedSplineKey() const { return CashedSplineKey; }

	ERoadSectionSelectionState GetState() const { return State; }
	ERoadSectionSelectionState GetStateVerified() const;

	inline bool IsSelected(const URoadSplineComponent* Spline, int SectionIndex) const { return Spline == GetSelectedSpline() && SectionIndex == SelectedSectionIndex; }
	inline bool IsSelected(const URoadSplineComponent* Spline, int SectionIndex, int LaneIndex) const { return Spline == GetSelectedSpline() && SectionIndex == SelectedSectionIndex && LaneIndex == SelectedLaneIndex; }
	inline bool IsSelected(const URoadSplineComponent* Spline, int SectionIndex, int LaneIndex, int KeyIndex) const { return Spline == GetSelectedSpline() && SectionIndex == SelectedSectionIndex && LaneIndex == SelectedLaneIndex && KeyIndex == SelectedKeyIndex; }
	inline bool IsSelected(const URoadSplineComponent* Spline, int SectionIndex, int LaneIndex, const TSubclassOf<URoadLaneAttributeDescriptor>& AttributeDescriptor, int KeyIndex) const { return Spline == GetSelectedSpline() && SectionIndex == SelectedSectionIndex && LaneIndex == SelectedLaneIndex && AttributeDescriptor == SelectedAttributeDescriptor && KeyIndex == SelectedKeyIndex;  }

	void FixState();

	void UpdateSplineSelection() const;

	/** Removes selection-set entries whose section/lane no longer exist in the current layout. */
	void PruneSelectedLanes();

	DECLARE_DELEGATE_RetVal(bool, FIsKeyValid);

	// Used in GetStateVerified() to check validation of the selected key
	FIsKeyValid IsKeyValid;

protected:
	/** Property path from the parent actor to the component */
	UPROPERTY()
	FComponentPropertyPath SplinePropertyPath;

	UPROPERTY()
	int32 SelectedSectionIndex = INDEX_NONE;

	UPROPERTY()
	int32 SelectedLaneIndex = MetaRoad::ZeroLaneIndex;

	/** Authoritative multi-lane selection (may span sections). Captured by undo transactions. */
	UPROPERTY()
	TArray<FRoadSectionLaneRef> SelectedLanes;

	UPROPERTY()
	TSubclassOf<URoadLaneAttributeDescriptor> SelectedAttributeDescriptor;

	UPROPERTY()
	int32 SelectedKeyIndex = INDEX_NONE;

	UPROPERTY()
	ESelectedTangentHandle SelectedTangentHandleType = ESelectedTangentHandle::None;

	/** Position on spline we have selected */
	UPROPERTY()
	FVector CahedPosition;

	/** Cached rotation for this point */
	UPROPERTY()
	FQuat CachedRotation;

	UPROPERTY()
	float CashedSplineKey = 0;

	UPROPERTY()
	ERoadSectionSelectionState State;
};

/** SplineComponent visualizer/edit functionality */
class  FRoadSectionComponentVisualizer : public FComponentVisualizer, public FGCObject
{
public:
	FRoadSectionComponentVisualizer();
	virtual ~FRoadSectionComponentVisualizer();

	//~ Begin FComponentVisualizer Interface
	virtual void OnRegister() override;
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	virtual void EndEditing() override;
	virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;
	virtual bool GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const override;
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	virtual bool HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	/** Handle click modified by Alt, Ctrl and/or Shift. The input HitProxy may not be on this component. */
	virtual bool HandleModifiedClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	/** Return whether focus on selection should focus on bounding box defined by active visualizer */
	virtual bool HasFocusOnSelectionBoundingBox(FBox& OutBoundingBox) override;
	/** Pass snap input to active visualizer */
	virtual bool HandleSnapTo(const bool bInAlign, const bool bInUseLineTrace, const bool bInUseBounds, const bool bInUsePivot, AActor* InDestination) override;
	/** Gets called when the mouse tracking has stopped (dragging behavior) */
	virtual void TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove) override;
	/** Get currently edited component, this is needed to reset the active visualizer after undo/redo */
	virtual UActorComponent* GetEditedComponent() const override;
	virtual TSharedPtr<SWidget> GenerateContextMenu() const override;
	virtual bool IsVisualizingArchetype() const override;
	//~ End FComponentVisualizer Interface

	//~ FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return GetReferencerNameStatic(); }
	//~ End of FGCObject interface

	URoadSplineComponent* GetEditedSplineComponent() const;
	URoadSectionComponentVisualizerSelectionState* GetSelectionState() const { return SelectionState; }

	/**
	 * Add (or replace) an attribute key on the currently selected section/lane.
	 * Assumes a transaction is already open. Generalizes OnCreateAttribute() so it can be reused by
	 * the viewport drag-and-drop handler.
	 *  - Value: optional pre-filled key value; if invalid, the descriptor's template is used.
	 *  - bReplaceExisting: true  -> Reset() the attribute first (single-key/replace semantics);
	 *                      false -> add/update a key alongside existing keys.
	 * Returns the resulting (post-sort) key index, or INDEX_NONE on failure.
	 */
	int32 AddAttributeKeyToSelection(
		const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor,
		TConstStructView<FRoadLaneAttributeValue> Value,
		double SOffset, bool bReplaceExisting);

	/** Convenience overload: pass a concrete FRoadLaneAttributeValue subclass by value. */
	template <typename T UE_REQUIRES(std::is_base_of_v<FRoadLaneAttributeValue, T>)>
	int32 AddAttributeKeyToSelection(
		const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor,
		const T& Value, double SOffset, bool bReplaceExisting)
	{
		return AddAttributeKeyToSelection(Descriptor, TConstStructView<FRoadLaneAttributeValue>(Value), SOffset, bReplaceExisting);
	}

	/**
	 * Like AddAttributeKeyToSelection, but applies the key to EVERY selected lane (multi-selection),
	 * using the same SOffset/Value. Lanes that reject the descriptor (CanBeAddedTo) are skipped.
	 * When only the section centerline is selected, behaves like AddAttributeKeyToSelection.
	 * Returns the resulting key index for the primary lane, or INDEX_NONE.
	 */
	int32 AddAttributeKeyToAllSelectedLanes(
		const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor,
		TConstStructView<FRoadLaneAttributeValue> Value,
		double SOffset, bool bReplaceExisting);

	/** Convenience overload: pass a concrete FRoadLaneAttributeValue subclass by value. */
	template <typename T UE_REQUIRES(std::is_base_of_v<FRoadLaneAttributeValue, T>)>
	int32 AddAttributeKeyToAllSelectedLanes(
		const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor,
		const T& Value, double SOffset, bool bReplaceExisting)
	{
		return AddAttributeKeyToAllSelectedLanes(Descriptor, TConstStructView<FRoadLaneAttributeValue>(Value), SOffset, bReplaceExisting);
	}

	/**
	 * Adds an attribute key to a single, explicitly addressed lane (used by positional operations such as
	 * a Polygon drop, which must target only the lane under the cursor). Assumes a transaction is already
	 * open. Returns the resulting key index or INDEX_NONE.
	 */
	int32 AddAttributeKeyToLane(
		int32 SectionIndex, int32 LaneIndex,
		const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor,
		TConstStructView<FRoadLaneAttributeValue> Value,
		double SOffset, bool bReplaceExisting);

	/** Convenience overload: pass a concrete FRoadLaneAttributeValue subclass by value. */
	template <typename T UE_REQUIRES(std::is_base_of_v<FRoadLaneAttributeValue, T>)>
	int32 AddAttributeKeyToLane(
		int32 SectionIndex, int32 LaneIndex,
		const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor,
		const T& Value, double SOffset, bool bReplaceExisting)
	{
		return AddAttributeKeyToLane(SectionIndex, LaneIndex, Descriptor, TConstStructView<FRoadLaneAttributeValue>(Value), SOffset, bReplaceExisting);
	}

	static FString GetReferencerNameStatic() { return TEXT("FRoadSectionComponentVisualizer"); }


protected:
	virtual void GenerateContextMenuSections(FMenuBuilder& InMenuBuilder) const;
	virtual void GenerateChildContextMenuSections(FMenuBuilder& InMenuBuilder) const {}
	const URoadSplineComponent* UpdateSelectedComponentAndSectionAndLane(HComponentVisProxy* VisProxy);

	/** Per-lane attribute mutation WITHOUT the layout refresh/redraw (so bulk adds can refresh once at the
	 *  end). Assumes a transaction is already open; calls SplineComp->Modify(). Returns key index or INDEX_NONE. */
	int32 AddAttributeKeyToLaneNoRefresh(
		int32 SectionIndex, int32 LaneIndex,
		const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor,
		TConstStructView<FRoadLaneAttributeValue> Value,
		double SOffset, bool bReplaceExisting);

	bool ShouldDraw(const UActorComponent* Component) const;

	/** Build the per-key numeric overlay (fields differ per visualizer; overridden by subclasses). */
	virtual void ConfigureKeyOverlay();

protected:
	void OnSplitSection(bool bFull);
	void OnDeleteSection();
	void OnCretaeProfile();
	void OnAddLane(bool bLeft);
	void OnDeleteLane();
	void OnReverseLane();
	bool IsLaneReverse();

	/** All selected real lanes (multi-selection minus the centerline), or the primary lane if the set is empty. */
	TArray<FRoadSectionLaneRef> GatherSelectedRealLanes() const;

protected:
	/** Output log commands */
	TSharedPtr<FUICommandList> RoadScetionComponentVisualizerActions;

	/** Current selection state */
	TObjectPtr<URoadSectionComponentVisualizerSelectionState> SelectionState;

	/** Floating numeric overlay for the selected key (created in ConfigureKeyOverlay). */
	TSharedPtr<FRoadKeyOverlayController> OverlayController;
};
