/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Textures/SlateIcon.h"
#include "Templates/SubclassOf.h"
#include "Widgets/Views/STreeView.h"

class URoadLaneAttributeDescriptor;
class FRoadAttributeComponentVisualizer;
class SSearchBox;
class ITableRow;
class STableViewBase;
enum class ECheckBoxState : uint8;

// One node of the Attribute tree. Category nodes have no DescriptorClass and own Children;
// leaf nodes carry a concrete URoadLaneAttributeDescriptor subclass to activate on click.
struct FMetaRoadAttributeTreeNode
{
	FText Label;
	FSlateIcon Icon;
	TSubclassOf<URoadLaneAttributeDescriptor> DescriptorClass;
	// For Blueprint-derived descriptors: the GetDisplayName() of the nearest native (C++) parent
	// class's CDO, shown dimmed on the right of the row. Empty for native descriptors.
	FText NativeClassLabel;
	TArray<TSharedPtr<FMetaRoadAttributeTreeNode>> Children;
};

/**
 * SMetaRoadAttributeTree
 *
 * The Edit > Attribute sub-mode tree: a searchable, Outliner-style multi-column table (Pin | Attribute |
 * Type) of all URoadLaneAttributeDescriptor types (categories -> descriptors). Self-contained — talks to
 * FMetaRoadSelectionController for the active attribute editor mode + the selected lane(s):
 *  - leaf click -> FMetaRoadSelectionController::SetAttributeEditorMode(descriptor)
 *  - selection is hard-bound to the active descriptor (UpdateSelection)
 *  - the Pin column sets/removes the row's attribute on the selected lane(s); it is tri-state across a
 *    multi-lane selection (present on all / none / some), and pinning a mixed state fills the lanes that
 *    lack it (see GetAttributePinState / TogglePinForDescriptor)
 *  - double-click a Blueprint-derived descriptor reveals its asset in the Content Browser
 */
class SMetaRoadAttributeTree : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaRoadAttributeTree) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Force the tree selection to match the module's active attribute editor mode descriptor (hard-bound).
	 *  Call when the selection or the active attribute sub-mode changes. */
	void UpdateSelection();

	// --- Pin column (called from the table row) ---
	/** True when a lane/section is selected in Attribute mode, so the Pin column can act. */
	bool CanEditAttributeOnSelectedLane() const;
	/** Tri-state presence of the attribute across the selected lane(s): present on all (Checked) / none
	 *  (Unchecked) / some (Undetermined). Unchecked when nothing editable is selected. */
	ECheckBoxState GetAttributePinState(const TSubclassOf<URoadLaneAttributeDescriptor>& DescriptorClass) const;
	/** Toggle the attribute on the selected lane(s): present on all -> remove from all; otherwise add to
	 *  the lanes that lack it (lanes that already have it keep their keys). */
	void TogglePinForDescriptor(const TSubclassOf<URoadLaneAttributeDescriptor>& DescriptorClass);

private:
	// Rebuild the full tree (all descriptors) from the registered URoadLaneAttributeDescriptor classes.
	void RebuildTree();
	// Rebuild FilteredRoots from TreeRoots using the current search text.
	void RefreshFilter();
	void OnSearchTextChanged(const FText& NewText);
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FMetaRoadAttributeTreeNode> Node, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(TSharedPtr<FMetaRoadAttributeTreeNode> Node, TArray<TSharedPtr<FMetaRoadAttributeTreeNode>>& OutChildren);
	void OnSelectionChanged(TSharedPtr<FMetaRoadAttributeTreeNode> Node, ESelectInfo::Type SelectInfo);
	void OnDoubleClick(TSharedPtr<FMetaRoadAttributeTreeNode> Node);
	// The active attribute visualizer, or null when not in Attribute mode. Backs the Pin column helpers.
	TSharedPtr<FRoadAttributeComponentVisualizer> GetActiveAttributeVisualizer() const;

	// Full tree (all descriptors) and the search-filtered view actually shown by the tree widget.
	TArray<TSharedPtr<FMetaRoadAttributeTreeNode>> TreeRoots;
	TArray<TSharedPtr<FMetaRoadAttributeTreeNode>> FilteredRoots;
	FText SearchText;
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<STreeView<TSharedPtr<FMetaRoadAttributeTreeNode>>> TreeView;
};
