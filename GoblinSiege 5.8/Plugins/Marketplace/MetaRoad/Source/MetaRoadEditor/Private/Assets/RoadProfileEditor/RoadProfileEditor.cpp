/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadProfileEditor.h"
#include "Assets/RoadProfile.h"
#include "RoadProfilePreviewBuilder.h"
#include "SRoadProfileViewport.h"
#include "SRoadProfileSelectionDetails.h"
#include "EditorMode/MetaRoadToolPresetManager.h"
#include "SSingleObjectDetailsPanel.h"
#include "PropertyEditorModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "UObject/UObjectGlobals.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/Application/SlateApplication.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "MetaRoadEditorStyle.h"

#define LOCTEXT_NAMESPACE "RoadProfileEditor"

namespace ERoadProfileEditorTabs
{
    // Tab identifiers
    static const FName DetailsID(TEXT("Details"));
    static const FName ViewportID(TEXT("Viewport"));
};

namespace
{
	/** Preset context backed by the road profile's transient preview build-settings holder (via the builder).
	 *  Operates on the same URoadBuildPreset assets as the editor mode: applying overwrites the preview holder
	 *  (NOT serialized into the URoadProfile asset), and Save As / Update capture the preview settings into an
	 *  asset. */
	class FRoadProfilePresetContext : public IMetaRoadPresetContext
	{
	public:
		explicit FRoadProfilePresetContext(URoadProfilePreviewBuilder* InBuilder) : Builder(InBuilder) {}

		virtual TArray<UMetaRoadBuildSettings*> GetPresetTargets() const override
		{
			TArray<UMetaRoadBuildSettings*> Targets;
			if (Builder.IsValid())
			{
				if (UMetaRoadBuildSettings* Holder = Builder->GetBuildSettings())
				{
					Targets.Add(Holder);
				}
			}
			return Targets;
		}
		// Same preset workflow as the editor mode: allow creating (Save As) and updating presets here too.
		virtual bool AllowsManagement() const override { return true; }

	private:
		TWeakObjectPtr<URoadProfilePreviewBuilder> Builder;
	};
}

class SRoadProfilePropertiesTabBody : public SSingleObjectDetailsPanel
{
public:
	SLATE_BEGIN_ARGS(SRoadProfilePropertiesTabBody) {}
	SLATE_END_ARGS()

private:
	// Pointer back to owning sprite editor instance (the keeper of state)
	TWeakPtr<class FRoadProfileEditor> RoadProfileEditorPtr;
public:
	void Construct(const FArguments& InArgs, TSharedPtr<FRoadProfileEditor> RoadProfileEditor)
	{
		RoadProfileEditorPtr = RoadProfileEditor;

		SSingleObjectDetailsPanel::Construct(SSingleObjectDetailsPanel::FArguments().HostCommandList(RoadProfileEditor->GetToolkitCommands()).HostTabManager(RoadProfileEditor->GetTabManager()), /*bAutomaticallyObserveViaGetObjectToObserve=*/ true, /*bAllowSearch=*/ true);
	}

	// SSingleObjectDetailsPanel interface
	virtual UObject* GetObjectToObserve() const override
	{
		return RoadProfileEditorPtr.Pin()->GetWorkingAsset();
	}

	virtual TSharedRef<SWidget> PopulateSlot(TSharedRef<SWidget> PropertyEditorWidget) override
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1)
			[
				PropertyEditorWidget
			];
	}
	// End of SSingleObjectDetailsPanel interface
};

FRoadProfileEditor::~FRoadProfileEditor()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnPropertyChangedHandle);
}

void FRoadProfileEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(WorkingAsset);
	Collector.AddReferencedObject(PreviewBuilder);
}

void FRoadProfileEditor::NotifyProfileEdited()
{
	if (!WorkingAsset)
	{
		return;
	}

	// Builder-based edits (lane add/delete/reverse, in-place struct details) bypass PostEditChangeProperty,
	// so UThumbnailManager never marks the asset's stored thumbnail dirty -> the stale on-disk icon reloads
	// after restart. Fire the property-changed event ourselves (the package is dirtied first): our own
	// OnObjectPropertyChanged handler rebuilds the preview, and UThumbnailManager re-renders the thumbnail on save.
	WorkingAsset->MarkPackageDirty();
	FPropertyChangedEvent EmptyEvent(nullptr);
	FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(WorkingAsset, EmptyEvent);
}

void FRoadProfileEditor::OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
{
	if (PreviewBuilder && Object == WorkingAsset)
	{
		PreviewBuilder->BuildFromProfile(WorkingAsset);
	}
}

void FRoadProfileEditor::ExtendToolbar()
{
	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP(this, &FRoadProfileEditor::FillToolbar));
	AddToolbarExtender(ToolbarExtender);
	RegenerateMenusAndToolbars();
}

void FRoadProfileEditor::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("PreviewMode");
	{
		ToolbarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateSP(this, &FRoadProfileEditor::GeneratePreviewModeMenu),
			TAttribute<FText>::CreateSP(this, &FRoadProfileEditor::GetPreviewModeLabel),
			LOCTEXT("PreviewModeTooltip", "Choose what the preview viewport displays"),
			TAttribute<FSlateIcon>::CreateSP(this, &FRoadProfileEditor::GetPreviewModeIcon));
	}
	ToolbarBuilder.EndSection();

	// Preset panel — only shown in Preview mode (presets affect the generated mesh shown there).
	ToolbarBuilder.BeginSection("Presets");
	{
		if (PresetManager.IsValid())
		{
			if (TSharedPtr<SWidget> PresetPanel = PresetManager->MakePresetPanel())
			{
				ToolbarBuilder.AddWidget(
					SNew(SBox)
					.Visibility_Lambda([this]()
					{
						return CurrentPreviewMode == ERoadProfilePreviewMode::GeneratedMesh
							? EVisibility::Visible : EVisibility::Collapsed;
					})
					[
						PresetPanel.ToSharedRef()
					]);
			}
		}
	}
	ToolbarBuilder.EndSection();
}

void FRoadProfileEditor::OnPresetApplied()
{
	// A preset overwrote the (transient) build property sets — rebuild the preview. Not saved to the asset.
	if (PreviewBuilder && WorkingAsset)
	{
		PreviewBuilder->BuildFromProfile(WorkingAsset);
	}
}

TSharedRef<SWidget> FRoadProfileEditor::GeneratePreviewModeMenu()
{
	FMenuBuilder MenuBuilder(/*bShouldCloseWindowAfterMenuSelection=*/true, GetToolkitCommands());

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Mode_RoadGraph", "Edit"),
		LOCTEXT("Mode_RoadGraph_Tip", "Edit mode: show only the road spline (lane selection / editing), no generated mesh"),
		FSlateIcon(FMetaRoadEditorStyle::Get().GetStyleSetName(), "RoadProfileEditor.EditMode"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FRoadProfileEditor::SetPreviewMode, ERoadProfilePreviewMode::RoadGraph),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &FRoadProfileEditor::IsPreviewModeActive, ERoadProfilePreviewMode::RoadGraph)),
		NAME_None,
		EUserInterfaceActionType::RadioButton);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Mode_GeneratedMesh", "Preview"),
		LOCTEXT("Mode_GeneratedMesh_Tip", "Preview mode: show only the generated road mesh"),
		FSlateIcon(FMetaRoadEditorStyle::Get().GetStyleSetName(), "RoadProfileEditor.PreviewMode"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FRoadProfileEditor::SetPreviewMode, ERoadProfilePreviewMode::GeneratedMesh),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &FRoadProfileEditor::IsPreviewModeActive, ERoadProfilePreviewMode::GeneratedMesh)),
		NAME_None,
		EUserInterfaceActionType::RadioButton);

	return MenuBuilder.MakeWidget();
}

FText FRoadProfileEditor::GetPreviewModeLabel() const
{
	return CurrentPreviewMode == ERoadProfilePreviewMode::RoadGraph
		? LOCTEXT("Mode_RoadGraph", "Edit")
		: LOCTEXT("Mode_GeneratedMesh", "Preview");
}

FSlateIcon FRoadProfileEditor::GetPreviewModeIcon() const
{
	const FName StyleSet = FMetaRoadEditorStyle::Get().GetStyleSetName();
	return CurrentPreviewMode == ERoadProfilePreviewMode::RoadGraph
		? FSlateIcon(StyleSet, "RoadProfileEditor.EditMode")
		: FSlateIcon(StyleSet, "RoadProfileEditor.PreviewMode");
}

void FRoadProfileEditor::SetPreviewMode(ERoadProfilePreviewMode Mode)
{
	CurrentPreviewMode = Mode;
	if (PreviewBuilder)
	{
		PreviewBuilder->SetPreviewMode(Mode);
	}
}

bool FRoadProfileEditor::IsPreviewModeActive(ERoadProfilePreviewMode Mode) const
{
	return CurrentPreviewMode == Mode;
}

void FRoadProfileEditor::OnLaneContextMenuRequested(int32 LaneIndex)
{
	if (!ViewportWidget.IsValid())
	{
		return;
	}

	const bool bIsLane = (LaneIndex != URoadProfilePreviewBuilder::NoSelection && LaneIndex != MetaRoad::ZeroLaneIndex);

	FMenuBuilder MenuBuilder(/*bShouldCloseWindowAfterMenuSelection=*/true, nullptr);
	MenuBuilder.BeginSection("RoadLane", LOCTEXT("ContextMenuRoadLane", "Road Lane"));

	const FName StyleSet = FMetaRoadEditorStyle::Get().GetStyleSetName();

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddLaneToLeft", "Add Lane to Left"),
		LOCTEXT("AddLaneToLeft_Tip", "Add a new road lane to the left of the selected lane"),
		FSlateIcon(StyleSet, "RoadSectionComponentVisualizer.AddLaneToLeft"),
		FUIAction(FExecuteAction::CreateSP(this, &FRoadProfileEditor::DoAddLane, true)));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddLaneToRight", "Add Lane to Right"),
		LOCTEXT("AddLaneToRight_Tip", "Add a new road lane to the right of the selected lane"),
		FSlateIcon(StyleSet, "RoadSectionComponentVisualizer.AddLaneToRight"),
		FUIAction(FExecuteAction::CreateSP(this, &FRoadProfileEditor::DoAddLane, false)));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("DeleteLane", "Delete Lane"),
		LOCTEXT("DeleteLane_Tip", "Delete the selected lane"),
		FSlateIcon(StyleSet, "RoadSectionComponentVisualizer.DeleteLane"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FRoadProfileEditor::DoDeleteLane),
			FCanExecuteAction::CreateLambda([bIsLane]() { return bIsLane; })));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ReverseLane", "Reverse Direction"),
		LOCTEXT("ReverseLane_Tip", "Reverse the travel direction of the selected lane"),
		FSlateIcon(StyleSet, "RoadSectionComponentVisualizer.ReverseLane"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FRoadProfileEditor::DoReverseLane),
			FCanExecuteAction::CreateLambda([bIsLane]() { return bIsLane; }),
			FIsActionChecked::CreateSP(this, &FRoadProfileEditor::IsLaneReversed)),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);

	MenuBuilder.EndSection();

	FSlateApplication::Get().PushMenu(
		ViewportWidget.ToSharedRef(),
		FWidgetPath(),
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
}

void FRoadProfileEditor::DoAddLane(bool bOnLeft)
{
	if (!WorkingAsset || !PreviewBuilder)
	{
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("AddLane", "Add Road Lane"));
	WorkingAsset->Modify();
	const int32 NewIdx = WorkingAsset->AddLane(PreviewBuilder->GetSelectedLane(), bOnLeft);
	NotifyProfileEdited(); // dirty + OnObjectPropertyChanged -> rebuild preview + thumbnail regen on save
	PreviewBuilder->SetSelectedLane(NewIdx);
	if (SelectionDetails.IsValid())
	{
		SelectionDetails->Refresh();
	}
}

void FRoadProfileEditor::DoDeleteLane()
{
	if (!WorkingAsset || !PreviewBuilder)
	{
		return;
	}
	const int32 Sel = PreviewBuilder->GetSelectedLane();
	if (Sel == URoadProfilePreviewBuilder::NoSelection || Sel == MetaRoad::ZeroLaneIndex)
	{
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("DeleteLane", "Delete Road Lane"));
	WorkingAsset->Modify();
	const int32 NewIdx = WorkingAsset->DeleteLane(Sel);
	NotifyProfileEdited(); // dirty + OnObjectPropertyChanged -> rebuild preview + thumbnail regen on save
	PreviewBuilder->SetSelectedLane(NewIdx);
	if (SelectionDetails.IsValid())
	{
		SelectionDetails->Refresh();
	}
}

void FRoadProfileEditor::DoReverseLane()
{
	if (!WorkingAsset || !PreviewBuilder)
	{
		return;
	}
	const int32 Sel = PreviewBuilder->GetSelectedLane();
	if (Sel == URoadProfilePreviewBuilder::NoSelection || Sel == MetaRoad::ZeroLaneIndex)
	{
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("ReverseLane", "Reverse Road Lane"));
	WorkingAsset->Modify();
	WorkingAsset->ReverseLane(Sel);
	NotifyProfileEdited(); // dirty + OnObjectPropertyChanged -> rebuild preview + thumbnail regen on save
	if (SelectionDetails.IsValid())
	{
		SelectionDetails->Refresh();
	}
}

bool FRoadProfileEditor::IsLaneReversed() const
{
	if (WorkingAsset && PreviewBuilder)
	{
		if (const FRoadLaneProfile* Lane = WorkingAsset->GetLaneByIndex(PreviewBuilder->GetSelectedLane()))
		{
			return Lane->Direction == ERoadLaneDirection::Invert;
		}
	}
	return false;
}

void FRoadProfileEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) 
{
    WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_MarkProfileEditor", "Mark Profile Editor"));
    auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

    FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

    InTabManager->RegisterTabSpawner(ERoadProfileEditorTabs::ViewportID, FOnSpawnTab::CreateSP(this, &FRoadProfileEditor::SpawnTab_Viewport))
        .SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
        .SetGroup(WorkspaceMenuCategoryRef)
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

    InTabManager->RegisterTabSpawner(ERoadProfileEditorTabs::DetailsID, FOnSpawnTab::CreateSP(this, &FRoadProfileEditor::SpawnTab_Details))
        .SetDisplayName(LOCTEXT("DetailsTabLabel", "Details"))
        .SetGroup(WorkspaceMenuCategoryRef)
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FRoadProfileEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
    FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

    InTabManager->UnregisterTabSpawner(ERoadProfileEditorTabs::ViewportID);
    InTabManager->UnregisterTabSpawner(ERoadProfileEditorTabs::DetailsID);
}

void FRoadProfileEditor::InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, UObject* InObject) 
{
	TArray<UObject*> ObjectsToEdit;
    ObjectsToEdit.Add(InObject);

    WorkingAsset = Cast<URoadProfile>(InObject);

    // Live preview rebuild whenever the profile is edited in the Details panel.
    OnPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FRoadProfileEditor::OnObjectPropertyChanged);

    // Create the builder before tabs spawn so both the viewport and the details tab can reference it.
    PreviewBuilder = NewObject<URoadProfilePreviewBuilder>(GetTransientPackage());

    // Preset panel (preview-only): applies preset values to the builder's transient property sets, then
    // rebuilds the preview. Presets are NOT serialized into the URoadProfile asset.
    PresetManager = MakeShared<FMetaRoadToolPresetManager>(MakeShared<FRoadProfilePresetContext>(PreviewBuilder));
    PresetManager->OnPresetApplied.AddSP(this, &FRoadProfileEditor::OnPresetApplied);

	// Default layout
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_RoadProfileEditor_Layout_v4")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(0.9f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.8f)
					->SetHideTabWell(true)
					->AddTab(ERoadProfileEditorTabs::ViewportID, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)
					->AddTab(ERoadProfileEditorTabs::DetailsID, ETabState::OpenedTab)
				)
			)
		);

	InitAssetEditor(Mode, InitToolkitHost, TEXT("RoadProfileApp"), StandaloneDefaultLayout, /*bCreateDefaultStandaloneMenu=*/ true, /*bCreateDefaultToolbar=*/ true, ObjectsToEdit);

	ExtendToolbar();
}

TSharedRef<SDockTab> FRoadProfileEditor::SpawnTab_Viewport(const FSpawnTabArgs& Args)
{
	TSharedRef<SRoadProfileViewport> Viewport = SNew(SRoadProfileViewport);
	ViewportWidget = Viewport;

	PreviewBuilder->Initialize(Viewport->GetPreviewWorld());
	Viewport->SetPreviewBuilder(PreviewBuilder);
	Viewport->SetContextMenuHandler(FOnLaneContextMenu::CreateSP(this, &FRoadProfileEditor::OnLaneContextMenuRequested));
	PreviewBuilder->BuildFromProfile(WorkingAsset);
	PreviewBuilder->SetPreviewMode(CurrentPreviewMode);

	return SNew(SDockTab)
		.Label(LOCTEXT("ViewportTab_Title", "Viewport"))
		[
			Viewport
		];
}

TSharedRef<SDockTab> FRoadProfileEditor::SpawnTab_Details(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTab_Title", "Details"))
		[
			SAssignNew(SelectionDetails, SRoadProfileSelectionDetails, WorkingAsset.Get(), PreviewBuilder.Get())
		];
}

FName FRoadProfileEditor::GetToolkitFName() const
{ 
    return FName(TEXT("RoadProfileEditor")); 
}

FText FRoadProfileEditor::GetBaseToolkitName() const 
{ 
    return LOCTEXT("FRoadProfileEditorLable", "Road Mark Profile Editor");
}

FString FRoadProfileEditor::GetWorldCentricTabPrefix() const 
{ 
    return TEXT("RoadProfileEditor"); 
}

FLinearColor FRoadProfileEditor::GetWorldCentricTabColorScale() const
{ 
    return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f); 
}

#undef LOCTEXT_NAMESPACE