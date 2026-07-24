/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "EditorMode/Widgets/SMetaRoadAttributeTree.h"
#include "MetaRoadEditorModule.h"
#include "Utils/AssetUtils.h"
#include "Assets/RoadLaneAttributeDescriptor.h"
#include "RoadSplineComponent.h"
#include "ComponentVisualizers/RoadSectionComponentVisualizer.h"
#include "ComponentVisualizers/RoadAttributeComponentVisualizer.h"

#include "Editor.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SExpanderArrow.h"

#define LOCTEXT_NAMESPACE "SMetaRoadAttributeTree"

namespace MetaRoadAttributeTreeColumns
{
	static const FName Pin("Pin");
	static const FName Name("Name");
	static const FName Type("Type");
}

/**
 * SMetaRoadAttributeTableRow — Outliner-style multi-column row for the attribute tree.
 * Pin column (leaf rows only): a tri-state pushpin toggle over the selected lane(s); Name column hosts the
 * expander + icon + label (tinted yellow when the attribute is present on all selected lanes); Type column
 * shows the dimmed native-parent-class label.
 */
class SMetaRoadAttributeTableRow : public SMultiColumnTableRow<TSharedPtr<FMetaRoadAttributeTreeNode>>
{
public:
	SLATE_BEGIN_ARGS(SMetaRoadAttributeTableRow) {}
		SLATE_ARGUMENT(TSharedPtr<FMetaRoadAttributeTreeNode>, Node)
		SLATE_ARGUMENT(TWeakPtr<SMetaRoadAttributeTree>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable)
	{
		Node = InArgs._Node;
		OwnerTree = InArgs._Owner;
		SMultiColumnTableRow<TSharedPtr<FMetaRoadAttributeTreeNode>>::Construct(FSuperRowType::FArguments(), OwnerTable);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == MetaRoadAttributeTreeColumns::Pin)
		{
			// Category rows have no descriptor, so no pin.
			if (!IsLeaf())
			{
				return SNullWidget::NullWidget;
			}
			return SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(2.f))
				.ToolTipText(LOCTEXT("PinRowTooltip", "Set/remove this attribute on the selected lane(s)"))
				.IsEnabled(this, &SMetaRoadAttributeTableRow::IsPinEnabled)
				.OnClicked(this, &SMetaRoadAttributeTableRow::OnPinClicked)
				[
					SNew(SImage)
					.Image(this, &SMetaRoadAttributeTableRow::GetPinBrush)
					.ColorAndOpacity(this, &SMetaRoadAttributeTableRow::GetPinColor)
				];
		}
		if (ColumnName == MetaRoadAttributeTreeColumns::Name)
		{
			return SNew(SHorizontalBox)
				// Only this column hosts the tree indentation/expander.
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SExpanderArrow, SharedThis(this)).IndentAmount(12)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SImage)
					.Image(Node->Icon.GetIcon())
					.Visibility(Node->Icon.IsSet() ? EVisibility::Visible : EVisibility::Collapsed)
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Node->Label)
					.ColorAndOpacity(this, &SMetaRoadAttributeTableRow::GetLabelColor)
				];
		}
		if (ColumnName == MetaRoadAttributeTreeColumns::Type)
		{
			return SNew(SBox).VAlign(VAlign_Center).Padding(4.f, 0.f, 6.f, 0.f)
			[
				SNew(STextBlock)
				.Text(Node->NativeClassLabel)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Visibility(Node->NativeClassLabel.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
			];
		}
		return SNullWidget::NullWidget;
	}

private:
	bool IsLeaf() const { return Node.IsValid() && IsValid(Node->DescriptorClass.Get()); }

	ECheckBoxState PinState() const
	{
		TSharedPtr<SMetaRoadAttributeTree> Tree = OwnerTree.Pin();
		return (Tree.IsValid() && Node.IsValid()) ? Tree->GetAttributePinState(Node->DescriptorClass) : ECheckBoxState::Unchecked;
	}

	bool IsPinEnabled() const
	{
		TSharedPtr<SMetaRoadAttributeTree> Tree = OwnerTree.Pin();
		return Tree.IsValid() && Tree->CanEditAttributeOnSelectedLane();
	}

	const FSlateBrush* GetPinBrush() const
	{
		switch (PinState())
		{
		case ECheckBoxState::Checked:      return FAppStyle::Get().GetBrush("Icons.Pinned");
		case ECheckBoxState::Undetermined: return FAppStyle::Get().GetBrush("Icons.FilledCircle");
		default:                           return FAppStyle::Get().GetBrush("Icons.Unpinned");
		}
	}

	FSlateColor GetPinColor() const
	{
		// Outliner behavior: an unset pin is invisible until the row is hovered; set/mixed pins stay visible.
		if (PinState() == ECheckBoxState::Unchecked && !IsHovered())
		{
			return FLinearColor::Transparent;
		}
		return IsHovered() ? FSlateColor::UseForeground() : FSlateColor::UseSubduedForeground();
	}

	FSlateColor GetLabelColor() const
	{
		// Tint yellow when present on all selected lanes (like the Actor Outliner highlights).
		return PinState() == ECheckBoxState::Checked ? FStyleColors::AccentYellow : FSlateColor::UseForeground();
	}

	FReply OnPinClicked()
	{
		TSharedPtr<SMetaRoadAttributeTree> Tree = OwnerTree.Pin();
		if (Tree.IsValid() && Node.IsValid())
		{
			Tree->TogglePinForDescriptor(Node->DescriptorClass);
		}
		return FReply::Handled();
	}

	TSharedPtr<FMetaRoadAttributeTreeNode> Node;
	TWeakPtr<SMetaRoadAttributeTree> OwnerTree;
};

void SMetaRoadAttributeTree::Construct(const FArguments& InArgs)
{
	RebuildTree();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
		.Padding(4.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
			[
				SAssignNew(SearchBox, SSearchBox)
				.HintText(LOCTEXT("AttributeSearchHint", "Search attributes..."))
				.OnTextChanged(this, &SMetaRoadAttributeTree::OnSearchTextChanged)
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SAssignNew(TreeView, STreeView<TSharedPtr<FMetaRoadAttributeTreeNode>>)
				.TreeItemsSource(&FilteredRoots)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SMetaRoadAttributeTree::OnGenerateRow)
				.OnGetChildren(this, &SMetaRoadAttributeTree::OnGetChildren)
				.OnSelectionChanged(this, &SMetaRoadAttributeTree::OnSelectionChanged)
				.OnMouseButtonDoubleClick(this, &SMetaRoadAttributeTree::OnDoubleClick)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column(MetaRoadAttributeTreeColumns::Pin)
					.FixedWidth(24.f)
					.HAlignHeader(HAlign_Center).VAlignHeader(VAlign_Center)
					.HAlignCell(HAlign_Center).VAlignCell(VAlign_Center)
					.DefaultTooltip(LOCTEXT("PinColumnTooltip", "Set/remove the attribute on the selected lane(s)"))
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FAppStyle::Get().GetBrush("Icons.Unpinned"))
					]
					+ SHeaderRow::Column(MetaRoadAttributeTreeColumns::Name)
					.DefaultLabel(LOCTEXT("NameColumn", "Attribute"))
					.FillWidth(0.6f)
					+ SHeaderRow::Column(MetaRoadAttributeTreeColumns::Type)
					.DefaultLabel(LOCTEXT("TypeColumn", "Type"))
					.FillWidth(0.4f)
				)
			]
		]
	];

	// Now that the tree widget exists, build the filtered view + expand categories.
	RefreshFilter();
}

void SMetaRoadAttributeTree::RebuildTree()
{
	TreeRoots.Reset();

	auto MakeLeaf = [](URoadLaneAttributeDescriptor* DefaultObject) -> TSharedPtr<FMetaRoadAttributeTreeNode>
	{
		auto Node = MakeShared<FMetaRoadAttributeTreeNode>();
		Node->Label = DefaultObject->GetDisplayName();
		Node->Icon = DefaultObject->GetIcon();
		UClass* DescriptorClass = DefaultObject->GetClass();
		Node->DescriptorClass = DescriptorClass;

		// For a Blueprint-derived descriptor, walk up to the nearest native parent class and take
		// its CDO's GetDisplayName() to show the underlying C++ attribute type on the right.
		if (DescriptorClass->ClassGeneratedBy != nullptr)
		{
			UClass* NativeClass = DescriptorClass;
			while (NativeClass && NativeClass->ClassGeneratedBy != nullptr)
			{
				NativeClass = NativeClass->GetSuperClass();
			}
			if (NativeClass)
			{
				if (auto* NativeCDO = NativeClass->GetDefaultObject<URoadLaneAttributeDescriptor>())
				{
					Node->NativeClassLabel = NativeCDO->GetDisplayName();
				}
			}
		}
		return Node;
	};

	TMap<FName, TArray<URoadLaneAttributeDescriptor*>> PerCategory;
	for (UClass* Class : AssetUtils::GetAllClassesOfSubClass(URoadLaneAttributeDescriptor::StaticClass()))
	{
		if (!Class)
		{
			continue;
		}
		if (auto* DefaultObject = Class->GetDefaultObject<URoadLaneAttributeDescriptor>())
		{
			PerCategory.FindOrAdd(DefaultObject->Category).Add(DefaultObject);
		}
	}

	// Default-category descriptors are shown as top-level leaves.
	if (auto* DefaultCategory = PerCategory.Find(NAME_None))
	{
		for (auto* DefaultObject : *DefaultCategory)
		{
			if (IsValid(DefaultObject))
			{
				TreeRoots.Add(MakeLeaf(DefaultObject));
			}
		}
	}

	// Other categories become expandable parent nodes.
	for (auto& [Category, Descriptors] : PerCategory)
	{
		if (Category == NAME_None)
		{
			continue;
		}
		auto CategoryNode = MakeShared<FMetaRoadAttributeTreeNode>();
		CategoryNode->Label = FText::FromName(Category);
		// Category rows have no icon (leaf descriptor rows keep theirs).
		for (auto* DefaultObject : Descriptors)
		{
			if (IsValid(DefaultObject))
			{
				CategoryNode->Children.Add(MakeLeaf(DefaultObject));
			}
		}
		TreeRoots.Add(CategoryNode);
	}
	// Construct() calls RefreshFilter() after the TreeView exists (which also rebuilds FilteredRoots), so
	// RebuildTree() itself does not need to filter here — RebuildTree is only ever invoked from Construct().
}

void SMetaRoadAttributeTree::RefreshFilter()
{
	const FString Search = SearchText.ToString().TrimStartAndEnd();
	const bool bNoFilter = Search.IsEmpty();

	FilteredRoots.Reset();
	for (const auto& Root : TreeRoots)
	{
		if (Root->Children.Num() == 0)
		{
			// Top-level leaf (default-category descriptor).
			if (bNoFilter || Root->Label.ToString().Contains(Search))
			{
				FilteredRoots.Add(Root);
			}
		}
		else if (bNoFilter)
		{
			FilteredRoots.Add(Root);
		}
		else
		{
			// Category: keep matching children (all children if the category name itself matches).
			const bool bCategoryMatches = Root->Label.ToString().Contains(Search);
			TArray<TSharedPtr<FMetaRoadAttributeTreeNode>> MatchingChildren;
			for (const auto& Child : Root->Children)
			{
				if (bCategoryMatches || Child->Label.ToString().Contains(Search))
				{
					MatchingChildren.Add(Child);
				}
			}
			if (MatchingChildren.Num() > 0)
			{
				TSharedPtr<FMetaRoadAttributeTreeNode> FilteredCategory = MakeShared<FMetaRoadAttributeTreeNode>(*Root);
				FilteredCategory->Children = MoveTemp(MatchingChildren);
				FilteredRoots.Add(FilteredCategory);
			}
		}
	}

	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
		// Expand category nodes (auto-expand while filtering so matches are visible).
		for (const auto& Root : FilteredRoots)
		{
			if (Root->Children.Num() > 0)
			{
				TreeView->SetItemExpansion(Root, true);
			}
		}
		// Keep the tree selection in sync with the active attribute editor mode.
		UpdateSelection();
	}
}

void SMetaRoadAttributeTree::OnSearchTextChanged(const FText& NewText)
{
	SearchText = NewText;
	RefreshFilter();
}

void SMetaRoadAttributeTree::OnGetChildren(TSharedPtr<FMetaRoadAttributeTreeNode> Node, TArray<TSharedPtr<FMetaRoadAttributeTreeNode>>& OutChildren)
{
	if (Node.IsValid())
	{
		OutChildren = Node->Children;
	}
}

TSharedRef<ITableRow> SMetaRoadAttributeTree::OnGenerateRow(TSharedPtr<FMetaRoadAttributeTreeNode> Node, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SMetaRoadAttributeTableRow, OwnerTable)
		.Node(Node)
		.Owner(SharedThis(this));
}

void SMetaRoadAttributeTree::OnSelectionChanged(TSharedPtr<FMetaRoadAttributeTreeNode> Node, ESelectInfo::Type SelectInfo)
{
	// Ignore programmatic selection; only react to user clicks/keyboard.
	if (SelectInfo == ESelectInfo::Direct)
	{
		return;
	}
	if (Node.IsValid() && IsValid(Node->DescriptorClass.Get()))
	{
		FMetaRoadSelectionController::Get().SetAttributeEditorMode(Node->DescriptorClass);
	}
	else
	{
		// Selection is hard-bound to the active mode: don't allow the user to leave it deselected.
		UpdateSelection();
	}
}

void SMetaRoadAttributeTree::UpdateSelection()
{
	if (!TreeView.IsValid())
	{
		return;
	}

	const TSubclassOf<URoadLaneAttributeDescriptor>& Active = FMetaRoadSelectionController::Get().GetSelectedAttributeDescriptor();

	TSharedPtr<FMetaRoadAttributeTreeNode> Match;
	TSharedPtr<FMetaRoadAttributeTreeNode> ParentCategory;
	for (const TSharedPtr<FMetaRoadAttributeTreeNode>& Root : FilteredRoots)
	{
		if (Root->DescriptorClass == Active)
		{
			Match = Root;
			break;
		}
		for (const TSharedPtr<FMetaRoadAttributeTreeNode>& Child : Root->Children)
		{
			if (Child->DescriptorClass == Active)
			{
				Match = Child;
				ParentCategory = Root;
				break;
			}
		}
		if (Match.IsValid())
		{
			break;
		}
	}

	if (Match.IsValid())
	{
		if (ParentCategory.IsValid())
		{
			TreeView->SetItemExpansion(ParentCategory, true);
		}
		// ESelectInfo::Direct so OnSelectionChanged treats it as programmatic (no re-entry).
		TreeView->SetSelection(Match, ESelectInfo::Direct);
	}
	else
	{
		TreeView->ClearSelection();
	}
}

TSharedPtr<FRoadAttributeComponentVisualizer> SMetaRoadAttributeTree::GetActiveAttributeVisualizer() const
{
	// Only valid in Attribute mode, where the active component visualizer is the attribute visualizer.
	if (FMetaRoadSelectionController::Get().GetRoadSelectionMode() != ERoadSelectionMode::Attribute)
	{
		return nullptr;
	}
	return StaticCastSharedPtr<FRoadAttributeComponentVisualizer>(FMetaRoadSelectionController::Get().GetComponentVisualizer());
}

bool SMetaRoadAttributeTree::CanEditAttributeOnSelectedLane() const
{
	TSharedPtr<FRoadAttributeComponentVisualizer> Visualizer = GetActiveAttributeVisualizer();
	// Accept Section too: selecting just a section means the zero lane (section centre, ZeroLaneIndex).
	URoadSectionComponentVisualizerSelectionState* SelectionState = Visualizer.IsValid() ? Visualizer->GetSelectionState() : nullptr;
	return SelectionState && SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Section;
}

ECheckBoxState SMetaRoadAttributeTree::GetAttributePinState(const TSubclassOf<URoadLaneAttributeDescriptor>& DescriptorClass) const
{
	TSharedPtr<FRoadAttributeComponentVisualizer> Visualizer = GetActiveAttributeVisualizer();
	if (!Visualizer.IsValid() || !IsValid(DescriptorClass.Get()))
	{
		return ECheckBoxState::Unchecked;
	}

	URoadSectionComponentVisualizerSelectionState* SelectionState = Visualizer->GetSelectionState();
	if (!SelectionState || SelectionState->GetStateVerified() < ERoadSectionSelectionState::Section)
	{
		return ECheckBoxState::Unchecked;
	}

	// Target lanes: the multi-lane selection, or the primary (section, lane) / section centre when empty.
	TArray<FRoadSectionLaneRef> Targets = SelectionState->GetSelectedLanes();
	if (Targets.Num() == 0)
	{
		Targets.Emplace(SelectionState->GetSelectedSectionIndex(), SelectionState->GetSelectedLaneIndex());
	}

	int32 Present = 0;
	for (const FRoadSectionLaneRef& Ref : Targets)
	{
		if (Visualizer->HasAttributeOnLane(Ref.SectionIndex, Ref.LaneIndex, DescriptorClass))
		{
			++Present;
		}
	}

	if (Present == 0)
	{
		return ECheckBoxState::Unchecked;
	}
	if (Present == Targets.Num())
	{
		return ECheckBoxState::Checked;
	}
	return ECheckBoxState::Undetermined;
}

void SMetaRoadAttributeTree::TogglePinForDescriptor(const TSubclassOf<URoadLaneAttributeDescriptor>& DescriptorClass)
{
	TSharedPtr<FRoadAttributeComponentVisualizer> Visualizer = GetActiveAttributeVisualizer();
	if (!Visualizer.IsValid() || !IsValid(DescriptorClass.Get()) || !CanEditAttributeOnSelectedLane())
	{
		return;
	}

	if (GetAttributePinState(DescriptorClass) == ECheckBoxState::Checked)
	{
		// Present on all selected lanes -> remove from all.
		Visualizer->UnpinAttributeFromSelectedLanes(DescriptorClass);
	}
	else
	{
		// None / some -> add to every selected lane that lacks it (existing lanes keep their keys).
		Visualizer->PinAttributeToSelectedLanes(DescriptorClass);
	}
}

void SMetaRoadAttributeTree::OnDoubleClick(TSharedPtr<FMetaRoadAttributeTreeNode> Node)
{
	if (!Node.IsValid() || !IsValid(Node->DescriptorClass.Get()) || !GEditor)
	{
		return;
	}

	// Blueprint-defined descriptors have a generating UBlueprint asset; reveal it in the Content Browser.
	if (UObject* GeneratedBy = Node->DescriptorClass->ClassGeneratedBy)
	{
		TArray<UObject*> ObjectsToSync{ GeneratedBy };
		GEditor->SyncBrowserToObjects(ObjectsToSync);
	}
}

#undef LOCTEXT_NAMESPACE
