/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RoadSectionComponentVisualizer.h"
#include "RoadHitProxies.h"


class  FRoadAttributeComponentVisualizer : public FRoadSectionComponentVisualizer
{
public:
	FRoadAttributeComponentVisualizer();
	virtual ~FRoadAttributeComponentVisualizer();

	virtual void OnRegister() override;
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	virtual FString GetReferencerName() const override { return GetReferencerNameStatic(); }
	static FString GetReferencerNameStatic() { return TEXT("FRoadAttributeComponentVisualizer"); }

	// Switch the active attribute being edited; clears any selection that referred to the previous attribute.
	void SetActiveAttributeDescriptor(const TSubclassOf<URoadLaneAttributeDescriptor>& AttributeDescriptor);

	// Programmatically select an attribute key (used by the details attribute list double-click).
	// Sets the selection state (spline -> section -> [lane] -> descriptor -> key) and caches the gizmo
	// position. Pass KeyIndex == INDEX_NONE to select only down to the descriptor (no key).
	void SelectAttributeKey(URoadSplineComponent* Spline, int32 SectionIndex, int32 LaneIndex,
		const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor, int32 KeyIndex);

	// --- Pin column support (SMetaRoadAttributeTree) ---
	// True if the attribute descriptor is present on the given (section, lane). LaneIndex == ZeroLaneIndex => section centre.
	bool HasAttributeOnLane(int32 SectionIndex, int32 LaneIndex, const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor);
	// Add the attribute to every selected lane (or the primary lane / section centre when the set is empty)
	// that does NOT already have it. Lanes that already have it keep their existing keys untouched.
	void PinAttributeToSelectedLanes(const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor);
	// Remove the attribute from every selected lane (or the primary lane / section centre when the set is empty).
	void UnpinAttributeFromSelectedLanes(const TSubclassOf<URoadLaneAttributeDescriptor>& Descriptor);

protected:
	virtual void GenerateChildContextMenuSections(FMenuBuilder& InMenuBuilder) const;
	virtual void ConfigureKeyOverlay() override;

protected:
	void OnCreateAttribute();
	void OnDeleteAttribute();
	void OnAddKey();
	void OnDeleteKey();
};
