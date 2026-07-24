/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RoadSplineComponent.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailCustomNodeBuilder.h"
#include "ComponentVisualizers/RoadSectionComponentVisualizer.h"
#include "Widgets/SWidget.h"

class IDetailLayoutBuilder;

class IStructureDataProvider;

namespace MetaRoad
{
	class FInstancedStructProvider;
	class FRoadLaneStructProvider;
}

class URoadLaneAttributeDescriptor;

/** One row in the lane/section attribute list. */
struct FRoadAttrListItem
{
	TSoftClassPtr<URoadLaneAttributeDescriptor> SoftClass;
	TSubclassOf<URoadLaneAttributeDescriptor> ResolvedClass;
	bool bValid = false;
};

/** Which per-lane struct a multi-instance property row edits across the selection. */
enum class ERoadLaneStructScope : uint8
{
	Lane,     // fields of FRoadLane
	RoadZone, // fields of the lane's RoadZone struct
};

class FRoadSelectionDetails : public IDetailCustomNodeBuilder, public TSharedFromThis<FRoadSelectionDetails>
{
public:
	FRoadSelectionDetails(URoadSplineComponent* InOwningSplineComponent, URoadSectionComponentVisualizerSelectionState* SelectionState, IDetailLayoutBuilder& DetailBuilder);

	//~ Begin IDetailCustomNodeBuilder interface
	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override;
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool RequiresTick() const override { return true; }
	virtual bool InitiallyCollapsed() const override { return false; }
	virtual FName GetName() const override;
	//~ End IDetailCustomNodeBuilder interface

private:
	URoadSplineComponent* RoadSplineComp;
	URoadSplineComponent* RoadSplineCompArchetype;
	TSharedPtr<IPropertyHandle> SectionsProperty;

	int SelectedSectiontIndex = INDEX_NONE;
	int SelectedLaneIndex = 0;
	TSubclassOf<URoadLaneAttributeDescriptor> SelectedAttributeDescriptor = nullptr;
	int SelectedKeyIndex = INDEX_NONE;

	/** Hash of the multi-selection set composition; used by Tick() to rebuild when it changes
	 *  (e.g. Ctrl+Click adds/removes a lane while the primary stays the same). */
	uint32 SelectedLanesSignature = 0;

	TObjectPtr<URoadSectionComponentVisualizerSelectionState> SelectionState;
	FSimpleDelegate OnRegenerateChildren;

	TSharedPtr<class SRoadLaneLanePicker> RoadLaneLanePicker;

	TSharedPtr<MetaRoad::FInstancedStructProvider> RoadLaneAttributeStruct;

	/** Multi-instance struct providers (one FStructOnScope per selected lane) backing the lane/RoadZone
	 *  property rows. Editing a field writes to every selected lane and differing values render as
	 *  "Multiple Values" natively. Kept alive between rebuilds; reset in Tick() on selection change. */
	TSharedPtr<MetaRoad::FRoadLaneStructProvider> LaneStructProvider;
	TSharedPtr<MetaRoad::FRoadLaneStructProvider> RoadZoneStructProvider;

	/** Looped fill area (closed spline): provider over FRoadLayout::LoopedRoadZone, its type picker, and the
	 *  RoadLayout child handles for LoopedRoadZone / TexAngle / TexScale. Cached loop-selected flag for Tick. */
	TSharedPtr<MetaRoad::FInstancedStructProvider> LoopedZoneProvider;
	TSharedPtr<class SRoadLaneLanePicker> LoopedZonePicker;
	TSharedPtr<IPropertyHandle> LoopedRoadZoneProperty;
	TSharedPtr<IPropertyHandle> LoopedRoadZoneTexAngleProperty;
	TSharedPtr<IPropertyHandle> LoopedRoadZoneTexScaleProperty;
	bool bSelectedLoopCached = false;

	/** Builds the "Attributes" list row for the given lane (LaneIndex == 0 → section centre line). */
	void AddAttributeList(IDetailChildrenBuilder& ChildrenBuilder, int32 SectionIndex, int32 LaneIndex);

	/** Adds one flat property row spanning all selected lanes via the given multi-instance provider, and
	 *  wires undo + refresh. Field-name agnostic, so new simple UPROPERTYs need no special-casing. */
	void AddMultiLaneStructRow(IDetailChildrenBuilder& ChildrenBuilder, const TSharedPtr<IStructureDataProvider>& Provider, FName PropertyName);

	/** Builds the details rows for the looped fill area of a closed spline: the LoopedRoadZone
	 *  (TInstancedStruct<FRoadZone>) with a type picker + flattened fields, and the two tex scalars. */
	void AddLoopedRoadZoneRows(IDetailChildrenBuilder& ChildrenBuilder);

	/** Hash of the current selection-set composition (order-independent). */
	uint32 ComputeSelectedLanesSignature() const;

	/** Backing source for the attribute SListView (kept alive between rebuilds). */
	TArray<TSharedPtr<FRoadAttrListItem>> AttributeListItems;
};
