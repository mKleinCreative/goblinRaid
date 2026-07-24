/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "EditorMode/MetaRoadEditorModeToolkit.h"
#include "EditorMode/MetaRoadEditorMode.h"
#include "EditorMode/MetaRoadPreviewManager.h" // EMetaRoadPreviewStatus
#include "EditorMode/IToolShutdownOverlayWidget.h"
#include "EditorMode/MetaRoadToolPresetManager.h"
#include "EditorMode/Widgets/SMetaRoadAttributeTree.h"
#include "EditorMode/Widgets/SMetaRoadBakePanel.h"
#include "EditorMode/Widgets/SMetaRoadFbxExportPanel.h"
#include "EditorMode/Widgets/SMetaRoadPresetPanel.h"
#include "EditorMode/Widgets/SMetaRoadVisibilityPanel.h"
#include "EditorMode/Widgets/MetaRoadWidgetUtils.h"
#include "RoadEditorCommands.h"
#include "MetaRoadEditorModule.h"
#include "MetaRoadEditorStyle.h"
#include "Widgets/Input/SButton.h"
#include "Animation/CurveSequence.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Images/SImage.h"
#include "Styling/StyleColors.h" // FStyleColors::AccentRed (Cancel/stop icon tint)

#include "Editor.h"
#include "Engine/Selection.h"
#include "RoadSplineComponent.h"
#include "MetaRoadActor.h"
#include "Editor/EditorEngine.h"
#include "EditorModeManager.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "Tools/UEdMode.h"
#include "InteractiveToolManager.h"
#include "IDetailsView.h"
#include "DetailCustomizations/RoadSelectionEmbeddedDetails.h"
#include "Widgets/Layout/SSplitter.h"
#include "Toolkits/AssetEditorModeUILayer.h"
#include "LevelEditorViewport.h"
#include "IAssetViewport.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/ISlateStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FMetaRoadEditorModeToolkit"

// Integrated tool-palette categories shown as segmented tabs in the Meta Road mode panel.
// "Edit" = spline editing sub-modes; "Create" = draw/generate tools.
static const FName MetaRoadPalette_Edit("Edit");
static const FName MetaRoadPalette_Create("Create");
static const FName MetaRoadPalette_Bake("Bake");
static const FName MetaRoadPalette_Misc("Misc");

// Slots of the main content SWidgetSwitcher (the panel shown below the palette for the active sub-mode).
// Order must match the order the slots are added in Init().
namespace
{
	enum EMetaRoadContentSlot : int32
	{
		ContentSlot_EditSubMode = 0, // attribute tree + selection editor
		ContentSlot_ToolProps   = 1, // active interactive tool's properties
		ContentSlot_Preset      = 2,
		ContentSlot_Bake        = 3,
		ContentSlot_Visibility  = 4,
		ContentSlot_FbxExport   = 5,
	};
}


FMetaRoadEditorModeToolkit::FMetaRoadEditorModeToolkit()
{
}

FMetaRoadEditorModeToolkit::~FMetaRoadEditorModeToolkit()
{
	GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->OnToolNotificationMessage.RemoveAll(this);
	GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->OnToolWarningMessage.RemoveAll(this);
	FMetaRoadSelectionController::Get().OnSelectedSplineChanged().RemoveAll(this);
}


TSharedPtr<SWidget> FMetaRoadEditorModeToolkit::GetInlineContent() const
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.VAlign(VAlign_Fill)
		[
			ToolkitWidget.ToSharedRef()
		];
}


void FMetaRoadEditorModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	// Use the standard FModeToolkit palette path (integrated "PaletteToolBar" tiles).
	bUsesToolkitBuilder = false;

	// Have to create the ToolkitWidget here because FModeToolkit::Init() is going to ask for it and add
	// it to the Mode panel, and not ask again afterwards. However we have to call Init() to get the
	// ModeDetailsView created, that we need to add to the ToolkitWidget. So, we will create the Widget
	// here but only add the rows to it after we call Init()
	const TSharedPtr<SVerticalBox> ToolkitWidgetVBox = SNew(SVerticalBox);

	SAssignNew(ToolkitWidget, SBorder)
		.HAlign(HAlign_Fill)
		.Padding(0)
		.BorderImage(FAppStyle::Get().GetBrush("NoBorder"))
		[
			ToolkitWidgetVBox->AsShared()
		];

	FModeToolkit::Init(InitToolkitHost, InOwningMode);

	GetToolkitHost()->OnActiveViewportChanged().AddSP(this, &FMetaRoadEditorModeToolkit::OnActiveViewportChanged);

	ModeWarningArea = SNew(STextBlock)
		.AutoWrapText(true)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
		.ColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.15f, 0.15f)));
	ModeWarningArea->SetText(FText::GetEmpty());
	ModeWarningArea->SetVisibility(EVisibility::Collapsed);

	ToolWarningArea = SNew(STextBlock)
		.AutoWrapText(true)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
		.ColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.15f, 0.15f)));
	ToolWarningArea->SetText(FText::GetEmpty());

	// Embedded details view that hosts the road-spline "Selection" editor (formerly the right Details
	// "Selection" category). FRoadSplineComponentDetails recognizes this view (via the module) and renders
	// the active edit sub-mode into it; RefreshSelectionDetailsView re-points/rebuilds it.
	{
		SelectionDetailsView = MetaRoadWidgetUtils::MakeCompactDetailsView(/*bAllowSearch=*/true);
		// This view shows the selected URoadSplineComponent, but with its OWN per-view layout that renders
		// only the active edit sub-mode's "Selection" editor and hides all other categories — so it doesn't
		// drag in the component's Transform/Landscape/etc. The main Details panel keeps the global layout.
		SelectionDetailsView->RegisterInstancedCustomPropertyLayout(
			URoadSplineComponent::StaticClass(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FRoadSelectionEmbeddedDetails::MakeInstance));
	}

	// The edit sub-mode radio commands (and utility commands) are bound on the module command list
	// (FMetaRoadEditorModule::BindCommands). Append it so the "Edit" palette category tiles resolve.
	GetToolkitCommands()->Append(FMetaRoadEditorModule::Get().GetCommandList().ToSharedRef());

	// add the various sections to the mode toolbox
	// Global display toggle: editable lane Schematic vs live generated-mesh Preview. The preview status +
	// Update icon buttons sit to the right, aligned to the panel's right edge (BuildPreviewControls).
	ToolkitWidgetVBox->AddSlot().AutoHeight().HAlign(HAlign_Fill).Padding(FMargin(5.f, 5.f, 5.f, 2.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.25f)[SNew(SBox)] // Just for aling
			+ SHorizontalBox::Slot().FillWidth(0.7f).VAlign(VAlign_Center).HAlign(HAlign_Center)
			[
				SNew(SSegmentedControl<EMetaRoadViewMode>)
				.Value_Lambda([this]() -> EMetaRoadViewMode
				{
					UMetaRoadEditorMode* Mode = GetMode();
					return Mode ? Mode->GetViewMode() : EMetaRoadViewMode::Schematic;
				})
				.OnValueChanged_Lambda([this](EMetaRoadViewMode NewMode)
				{
					if (UMetaRoadEditorMode* Mode = GetMode())
					{
						Mode->SetViewMode(NewMode);
					}
				})
				+ SSegmentedControl<EMetaRoadViewMode>::Slot(EMetaRoadViewMode::Schematic).Text(LOCTEXT("ViewModeSchematic", "Schematic"))
					.ToolTip(LOCTEXT("ViewModeSchematicTooltip", "Show the editable lane graph drawn by the visualizers."))
				+ SSegmentedControl<EMetaRoadViewMode>::Slot(EMetaRoadViewMode::Preview).Text(LOCTEXT("ViewModePreview", "Preview"))
					.ToolTip(LOCTEXT("ViewModePreviewTooltip", "Show a live preview of the generated road mesh."))
			]
			+ SHorizontalBox::Slot().FillWidth(0.25f).HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(FMargin(6.f, 0.f, 0.f, 0.f))
			[
				BuildPreviewControls()
			]
		];
	// The preset combo lives inside the Preset sub-mode panel (SMetaRoadPresetPanel), not here.
	ToolkitWidgetVBox->AddSlot().AutoHeight().HAlign(HAlign_Fill).Padding(5)
		[
			ModeWarningArea->AsShared()
		];
	ToolkitWidgetVBox->AddSlot().AutoHeight().HAlign(HAlign_Fill).Padding(5)
		[
			ToolWarningArea->AsShared()
		];
	// Main content area, filling the available height: either the attribute tree (Edit > Attribute)
	// or the active tool's properties — they are mutually exclusive, so a switcher lets each use the
	// full height.
	ToolkitWidgetVBox->AddSlot().FillHeight(1.f).HAlign(HAlign_Fill).Padding(FMargin(0.f, 4.f, 0.f, 0.f))
		[
			SNew(SWidgetSwitcher)
			.WidgetIndex_Lambda([this]() -> int32
			{
				if (IsPresetSubModeActive()) { return ContentSlot_Preset; }
				if (IsBakeSubModeActive()) { return ContentSlot_Bake; }
				if (IsFbxExportSubModeActive()) { return ContentSlot_FbxExport; }
				if (IsVisibilitySubModeActive()) { return ContentSlot_Visibility; }
				if (IsEditSubModeActive()) { return ContentSlot_EditSubMode; }
				return ContentSlot_ToolProps;
			})
			// Index 0 — edit sub-modes: the attribute tree (Attribute mode only) above the selection editor,
			// split by a draggable handle. In non-Attribute modes the tree slot is collapsed, so the splitter
			// gives the whole area to the selection editor (no handle).
			+ SWidgetSwitcher::Slot()
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)
				+ SSplitter::Slot()
				.Value(0.5f)
				[
					SNew(SBox)
					.Visibility_Lambda([this]() { return IsAttributeSubModeActive() ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SAssignNew(AttributeTree, SMetaRoadAttributeTree)
					]
				]
				+ SSplitter::Slot()
				.Value(0.5f)
				[
					// Show the selection editor when a single road spline is selected; otherwise a hint.
					SNew(SWidgetSwitcher)
					.WidgetIndex_Lambda([this]() { return GetSelectedRoadSplineForPanel() != nullptr ? 0 : 1; })
					+ SWidgetSwitcher::Slot()
					[
						SelectionDetailsView->AsShared()
					]
					+ SWidgetSwitcher::Slot()
					[
						SNew(SBox)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Padding(8.f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("NoSplineSelected", "Select a road spline to edit."))
							.Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							.Justification(ETextJustify::Center)
							.AutoWrapText(true)
						]
					]
				]
			]
			// Index 1 — active interactive tool's properties.
			+ SWidgetSwitcher::Slot()
			[
				ModeDetailsView->AsShared()
			]
			// Index 2 — Preset sub-mode panel.
			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(PresetPanel, SMetaRoadPresetPanel).EditorMode(GetMode())
			]
			// Index 3 — Bake sub-mode panel (Bake/Clear Selected/All).
			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(BakePanel, SMetaRoadBakePanel).EditorMode(GetMode())
			]
			// Index 4 — Visibility sub-mode panel (Misc): UMetaRoadVisibilitySettings details.
			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(VisibilityPanel, SMetaRoadVisibilityPanel)
			]
			// Index 5 — FBX Export sub-mode panel (Assets): export settings + Export Selected/All.
			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(FbxExportPanel, SMetaRoadFbxExportPanel).EditorMode(GetMode())
			]
		];
	// (Bake/Clear → the Bake-palette Bake tile; visibility settings → the Misc > Visibility tile — both in
	// the content switcher above, not pinned at the bottom anymore.)

	ClearNotification();
	ClearWarning();

	ActiveToolName = FText::GetEmpty();
	ActiveToolMessage = FText::GetEmpty();

	GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->OnToolNotificationMessage.AddSP(this, &FMetaRoadEditorModeToolkit::PostNotification);
	GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->OnToolWarningMessage.AddSP(this, &FMetaRoadEditorModeToolkit::PostWarning);

	// Re-point the Selection panel whenever the active visualizer's selected spline changes (a lane click on a
	// different spline of the same multi-spline AMetaRoad fires no actor-selection event).
	FMetaRoadSelectionController::Get().OnSelectedSplineChanged().AddSP(this, &FMetaRoadEditorModeToolkit::RefreshSelectionDetailsView);
}


void FMetaRoadEditorModeToolkit::UpdateActiveToolProperties()
{
	UInteractiveTool* CurTool = GetScriptableEditorMode()->GetToolManager(EToolsContextScope::EdMode)->GetActiveTool(EToolSide::Left);
	if (CurTool == nullptr)
	{
		return;
	}

	// Before actually changing the detail panel, we need to see where the current keyboard focus is, because
	// if it's inside the detail panel, we'll need to reset it to the detail panel as a whole, else we might
	// lose it entirely when that detail panel element gets destroyed (which would make us unable to receive any
	// hotkey presses until the user clicks somewhere).
	TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
	if (FocusedWidget != ModeDetailsView)
	{
		// Search upward from the currently focused widget
		TSharedPtr<SWidget> CurrentWidget = FocusedWidget;
		while (CurrentWidget.IsValid())
		{
			if (CurrentWidget == ModeDetailsView)
			{
				// Reset focus to the detail panel as a whole to avoid losing it when the inner elements change.
				FSlateApplication::Get().SetKeyboardFocus(ModeDetailsView);
				break;
			}

			CurrentWidget = CurrentWidget->GetParentWidget();
		}
	}

	ModeDetailsView->SetObjects(CurTool->GetToolProperties(true));
}

void FMetaRoadEditorModeToolkit::InvalidateCachedDetailPanelState(UObject* ChangedObject)
{
	ModeDetailsView->InvalidateCachedState();
}


void FMetaRoadEditorModeToolkit::PostNotification(const FText& Message)
{
	ClearNotification();

	ActiveToolMessage = Message;

	if (ModeUILayer.IsValid())
	{
		TSharedPtr<FAssetEditorModeUILayer> ModeUILayerPtr = ModeUILayer.Pin();
		ActiveToolMessageHandle = GEditor->GetEditorSubsystem<UStatusBarSubsystem>()->PushStatusBarMessage(ModeUILayerPtr->GetStatusBarName(), ActiveToolMessage);
	}
}

void FMetaRoadEditorModeToolkit::ClearNotification()
{
	ActiveToolMessage = FText::GetEmpty();

	if (ModeUILayer.IsValid())
	{
		TSharedPtr<FAssetEditorModeUILayer> ModeUILayerPtr = ModeUILayer.Pin();
		GEditor->GetEditorSubsystem<UStatusBarSubsystem>()->PopStatusBarMessage(ModeUILayerPtr->GetStatusBarName(), ActiveToolMessageHandle);
	}
	ActiveToolMessageHandle.Reset();
}


void FMetaRoadEditorModeToolkit::PostWarning(const FText& Message)
{
	ToolWarningArea->SetText(Message);
	ToolWarningArea->SetVisibility(EVisibility::Visible);
}

void FMetaRoadEditorModeToolkit::ClearWarning()
{
	ToolWarningArea->SetText(FText());
	ToolWarningArea->SetVisibility(EVisibility::Collapsed);
}

FName FMetaRoadEditorModeToolkit::GetToolkitFName() const
{
	return FName("MetaRoadEditorMode");
}

FText FMetaRoadEditorModeToolkit::GetBaseToolkitName() const
{
	return NSLOCTEXT("MetaRoadEditorModeToolkit", "DisplayName", "MetaRoadEditorMode Tool");
}


FText FMetaRoadEditorModeToolkit::GetToolPaletteDisplayName(FName Palette) const
{
	return FText::FromName(Palette);
}

void FMetaRoadEditorModeToolkit::GetToolPaletteNames(TArray<FName>& PaletteNames) const
{
	PaletteNames.Add(MetaRoadPalette_Create);
	PaletteNames.Add(MetaRoadPalette_Edit);
	PaletteNames.Add(MetaRoadPalette_Bake);
	PaletteNames.Add(MetaRoadPalette_Misc);
}

void FMetaRoadEditorModeToolkit::BuildToolPalette(FName PaletteIndex, class FToolBarBuilder& ToolbarBuilder)
{
	const FRoadEditorCommands& Commands = FRoadEditorCommands::Get();

	if (PaletteIndex == MetaRoadPalette_Edit)
	{
		// Spline editing sub-modes (radio). Bound on the module command list, which is appended
		// to the toolkit command list in Init() so these palette buttons resolve.
		ToolbarBuilder.AddToolBarButton(Commands.RoadSplineMode);
		ToolbarBuilder.AddToolBarButton(Commands.RoadSectionMode);
		ToolbarBuilder.AddToolBarButton(Commands.RoadOffsetMode);
		ToolbarBuilder.AddToolBarButton(Commands.RoadLaneWidthMode);
		ToolbarBuilder.AddToolBarButton(Commands.RoadAttributeMode);
		ToolbarBuilder.AddToolBarButton(Commands.RoadPresetMode);
	}
	else if (PaletteIndex == MetaRoadPalette_Bake)
	{
		// Bake tile (radio sub-mode); its panel hosts the Bake/Clear Selected/All buttons.
		ToolbarBuilder.AddToolBarButton(Commands.RoadBakeMode);
		// FBX Export tile (radio sub-mode); its panel hosts the export settings + Export Selected/All.
		ToolbarBuilder.AddToolBarButton(Commands.RoadFbxExportMode);
	}
	else if (PaletteIndex == MetaRoadPalette_Misc)
	{
		// Visibility tile (radio sub-mode); its panel hosts UMetaRoadVisibilitySettings.
		ToolbarBuilder.AddToolBarButton(Commands.RoadVisibilityMode);
	}
	else // MetaRoadPalette_Create
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginDrawNewRoad);
#if METAROAD_PRO
		ToolbarBuilder.AddToolBarButton(Commands.BeginDrawRoundabout);
		ToolbarBuilder.AddToolBarButton(Commands.BeginDrawIntersection);
		ToolbarBuilder.AddToolBarButton(Commands.BeginDrawCrosswalk);
		ToolbarBuilder.AddToolBarButton(Commands.BeginDrawChevronMarking);
#endif
	}
}


bool FMetaRoadEditorModeToolkit::IsAttributeSubModeActive() const
{
	// The attribute tree is shown when the Attribute edit sub-mode is active and no interactive
	// tool is running (a draw tool takes over the viewport and its own overlay UI).
	return !bInActiveTool && FMetaRoadSelectionController::Get().GetRoadSelectionMode() == ERoadSelectionMode::Attribute;
}

bool FMetaRoadEditorModeToolkit::IsPresetSubModeActive() const
{
	return !bInActiveTool && FMetaRoadSelectionController::Get().GetRoadSelectionMode() == ERoadSelectionMode::Preset;
}

bool FMetaRoadEditorModeToolkit::IsBakeSubModeActive() const
{
	return !bInActiveTool && FMetaRoadSelectionController::Get().GetRoadSelectionMode() == ERoadSelectionMode::Bake;
}

bool FMetaRoadEditorModeToolkit::IsFbxExportSubModeActive() const
{
	return !bInActiveTool && FMetaRoadSelectionController::Get().GetRoadSelectionMode() == ERoadSelectionMode::FbxExport;
}

bool FMetaRoadEditorModeToolkit::IsVisibilitySubModeActive() const
{
	return !bInActiveTool && FMetaRoadSelectionController::Get().GetRoadSelectionMode() == ERoadSelectionMode::Visibility;
}

bool FMetaRoadEditorModeToolkit::IsEditSubModeActive() const
{
	if (bInActiveTool)
	{
		return false;
	}
	switch (FMetaRoadSelectionController::Get().GetRoadSelectionMode())
	{
	case ERoadSelectionMode::Spline:
	case ERoadSelectionMode::Section:
	case ERoadSelectionMode::Offset:
	case ERoadSelectionMode::Width:
	case ERoadSelectionMode::Attribute:
		return true;
	default:
		return false;
	}
}

void FMetaRoadEditorModeToolkit::RefreshSelectionDetailsView()
{
	if (!SelectionDetailsView.IsValid())
	{
		return;
	}

	// bForceRefresh re-runs CustomizeDetails so a sub-mode change (same spline) re-dispatches the builder.
	// When no single spline is selected the view is emptied; a placeholder message is shown in its place
	// (see the SWidgetSwitcher around SelectionDetailsView in Init).
	SelectionDetailsView->SetObject(GetSelectedRoadSplineForPanel(), /*bForceRefresh=*/true);

	// Keep the attribute tree's selection hard-bound to the active attribute editor mode (e.g. when the
	// Attribute sub-mode is (re)entered or the selection changes).
	if (AttributeTree.IsValid())
	{
		AttributeTree->UpdateSelection();
	}
}

URoadSplineComponent* FMetaRoadEditorModeToolkit::GetSelectedRoadSplineForPanel() const
{
	// The embedded selection editor handles a single object, so return the spline only when exactly one
	// AMetaRoad is selected (reuses the mode's gather — splines on other actors are ignored).
	UMetaRoadEditorMode* Mode = GetMode();
	if (!Mode)
	{
		return nullptr;
	}
	const TArray<TWeakObjectPtr<AMetaRoad>> Roads = Mode->GatherSelectedRoadActors();
	if (Roads.Num() != 1 || !Roads[0].IsValid())
	{
		return nullptr;
	}
	AMetaRoad* Actor = Roads[0].Get();

	// An AMetaRoad can own several URoadSplineComponents (SubGroups, Attach To, intersections). Use the
	// live selected spline from the module-owned model (correct across sub-mode swaps + undo) so the Selection
	// panel customizes the SAME component the visualizer's state points at. The owner check rejects a spline from
	// a different actor (e.g. a persisted selection while a different actor is now selected).
	URoadSplineComponent* Selected = FMetaRoadSelectionController::Get().GetSelectedSplineForActiveMode();
	if (Selected && Selected->GetOwner() == Actor)
	{
		return Selected;
	}

	// Spline sub-mode (and before any element is clicked): fall back to the actor's first road spline.
	return Actor->FindComponentByClass<URoadSplineComponent>();
}

void FMetaRoadEditorModeToolkit::RefreshPresetPanel()
{
	if (PresetPanel.IsValid())
	{
		PresetPanel->RefreshWorkingObjects();
	}
}

void FMetaRoadEditorModeToolkit::InvokeUI()
{
	FModeToolkit::InvokeUI();

	// FModeToolkit::UpdatePrimaryModePanel() wrapped our GetInlineContent() output in a SScrollBar widget,
	// however this doesn't make sense as we want to dock panels to the "top" and "bottom" of our mode panel area,
	// and the details panel in the middle has it's own scrollbar already. The SScrollBar is hardcoded as the content
	// of FModeToolkit::InlineContentHolder so we can just replace it here
	InlineContentHolder->SetContent(GetInlineContent().ToSharedRef());

	// Default to the Create category (the first tab) so it is highlighted on first open (otherwise the
	// segmented control starts with no active palette).
	SetCurrentPalette(MetaRoadPalette_Create);
}

void FMetaRoadEditorModeToolkit::ShutdownUI()
{
	FModeToolkit::ShutdownUI();

	ClearNotification();
}


void FMetaRoadEditorModeToolkit::OnToolPaletteChanged(FName PaletteName)
{
}


void FMetaRoadEditorModeToolkit::ShowRealtimeAndModeWarnings(bool bShowRealtimeWarning)
{
	FText WarningText{};
	if (GEditor->bIsSimulatingInEditor)
	{
		WarningText = LOCTEXT("MetaRoadToolkitSimulatingWarning", "Cannot use Meta Road tools while simulating.");
	}
	else if (GEditor->PlayWorld != NULL)
	{
		WarningText = LOCTEXT("MetaRoadToolkitPIEWarning", "Cannot use Meta Road tools in PIE.");
	}
	else if (bShowRealtimeWarning)
	{
		WarningText = LOCTEXT("MetaRoadToolkitRealtimeWarning", "Realtime Mode is required for Meta Road tools to work correctly. Please enable Realtime Mode in the Viewport Options or with the Ctrl+r hotkey.");
	}
	if (!WarningText.IdenticalTo(ActiveWarning))
	{
		ActiveWarning = WarningText;
		ModeWarningArea->SetVisibility(ActiveWarning.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible);
		ModeWarningArea->SetText(ActiveWarning);
	}
}

void FMetaRoadEditorModeToolkit::OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	bInActiveTool = true;
	// While a Create-category tool runs, suppress the Edit sub-mode tiles' checked state so only
	// one palette tile (the active tool) appears selected across both tabs.
	FMetaRoadSelectionController::Get().SetInteractiveToolActive(true);
	// Activating any tool resets the edit sub-mode to Spline, so the mode returns to Spline editing
	// once the tool ends.
	FMetaRoadSelectionController::Get().SetSplineEditorMode();

	// Clear any stale overlay left over from a previous tool: if the new tool does not provide its
	// own shutdown overlay, the pointer must be null so we don't re-add a phantom overlay below.
	ToolShutdownViewportOverlayWidget.Reset();

	if (auto ShutdownOverlayWidget = Cast<IToolShutdownOverlayWidget>(Tool))
	{
		ToolShutdownViewportOverlayWidget = ShutdownOverlayWidget->MakeShutdownOverlayWidget(this->AsWeak());
	}

	UpdateActiveToolProperties();

	UInteractiveTool* CurTool = GetScriptableEditorMode()->GetToolManager(EToolsContextScope::EdMode)->GetActiveTool(EToolSide::Left);
	// This can happen due to out of order mode Enter/Exit as a result of Exit being deferred to Tick.
	if (!CurTool)
	{
		return;
	}

	CurTool->OnPropertySetsModified.AddSP(this, &FMetaRoadEditorModeToolkit::UpdateActiveToolProperties);
	CurTool->OnPropertyModifiedDirectlyByTool.AddSP(this, &FMetaRoadEditorModeToolkit::InvalidateCachedDetailPanelState);

	ActiveToolName = CurTool->GetToolInfo().ToolDisplayName;

	if (ToolShutdownViewportOverlayWidget)
	{
		GetToolkitHost()->AddViewportOverlayWidget(ToolShutdownViewportOverlayWidget.ToSharedRef());
	}

	// Invalidate all the level viewports so that e.g. hitproxy buffers are cleared
	// (fixes the editor gizmo still being clickable despite not being visible)
	if (GIsEditor)
	{
		for (FLevelEditorViewportClient* Viewport : GEditor->GetLevelViewportClients())
		{
			Viewport->Invalidate();
		}
	}

}

void FMetaRoadEditorModeToolkit::OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	bInActiveTool = false;
	FMetaRoadSelectionController::Get().SetInteractiveToolActive(false);

	if (IsHosted() && ToolShutdownViewportOverlayWidget)
	{
		GetToolkitHost()->RemoveViewportOverlayWidget(ToolShutdownViewportOverlayWidget.ToSharedRef());
	}
	// Drop the reference so the next tool that lacks an overlay does not re-add this one.
	ToolShutdownViewportOverlayWidget.Reset();

	ModeDetailsView->SetObject(nullptr);
	ActiveToolName = FText::GetEmpty();

	ClearNotification();
	ClearWarning();
	UInteractiveTool* CurTool = GetScriptableEditorMode()->GetToolManager(EToolsContextScope::EdMode)->GetActiveTool(EToolSide::Left);
	if ( CurTool )
	{
		CurTool->OnPropertySetsModified.RemoveAll(this);
		CurTool->OnPropertyModifiedDirectlyByTool.RemoveAll(this);
	}

}

void FMetaRoadEditorModeToolkit::OnActiveViewportChanged(TSharedPtr<IAssetViewport> OldViewport, TSharedPtr<IAssetViewport> NewViewport)
{
	// Only worry about handling this notification if there is an active tool
	if (!ActiveToolName.IsEmpty())
	{
		// Check first to see if this changed because the old viewport was deleted and if not, remove our hud
		if (OldViewport)
		{
			if (ToolShutdownViewportOverlayWidget)
			{
				GetToolkitHost()->RemoveViewportOverlayWidget(ToolShutdownViewportOverlayWidget.ToSharedRef(), OldViewport);
			}
		}

		// Add the hud to the new viewport
		if (ToolShutdownViewportOverlayWidget)
		{
			GetToolkitHost()->AddViewportOverlayWidget(ToolShutdownViewportOverlayWidget.ToSharedRef(), NewViewport);
		}
	}
}

UMetaRoadEditorMode* FMetaRoadEditorModeToolkit::GetMode() const
{
	return Cast<UMetaRoadEditorMode>(GetScriptableEditorMode().Get());
}

TSharedRef<SWidget> FMetaRoadEditorModeToolkit::BuildPreviewControls()
{
	auto GetStatus = [this]() -> EMetaRoadPreviewStatus
	{
		UMetaRoadEditorMode* Mode = GetMode();
		return Mode ? Mode->GetPreviewStatus() : EMetaRoadPreviewStatus::Idle;
	};

	// Rotating throbber for the InProgress state (FCurveSequence owned by the toolkit).
	PreviewThrobberAnim = MakeShared<FCurveSequence>(0.f, 1.f);
	TSharedPtr<SImage> ThrobberImage;
	SAssignNew(ThrobberImage, SImage)
		.Image(FAppStyle::Get().GetBrush("NotificationList.Throbber"))
		.RenderTransformPivot(FVector2D(0.5f, 0.5f))
		.RenderTransform(MakeAttributeLambda([Anim = PreviewThrobberAnim]() -> TOptional<FSlateRenderTransform>
		{
			return FSlateRenderTransform(FQuat2D(Anim->GetLerp() * 2.f * PI));
		}));
	PreviewThrobberAnim->Play(ThrobberImage.ToSharedRef(), /*bPlayLooped=*/true);

	return SNew(SHorizontalBox)
		// Only relevant while in Preview view mode.
		.Visibility_Lambda([this]()
		{
			UMetaRoadEditorMode* Mode = GetMode();
			return (Mode && Mode->GetViewMode() == EMetaRoadViewMode::Preview) ? EVisibility::Visible : EVisibility::Hidden;
		})
		// Status button (icon-only): the live build status icon; click opens the Output Log. Hidden when Idle.
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 4.f, 0.f))
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(2.f))
			.ToolTipText(LOCTEXT("PreviewStatusTooltip", "Live preview build status — click to open the Output Log"))
			.Visibility_Lambda([GetStatus]() { return GetStatus() != EMetaRoadPreviewStatus::Idle ? EVisibility::Visible : EVisibility::Collapsed; })
			.OnClicked_Lambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("OutputLog"))); return FReply::Handled(); })
			[
				SNew(SBox)
				.WidthOverride(16.f)
				.HeightOverride(16.f)
				[
					SNew(SWidgetSwitcher)
					.WidgetIndex_Lambda([GetStatus]() -> int32
					{
						switch (GetStatus())
						{
						case EMetaRoadPreviewStatus::InProgress: return 0;
						case EMetaRoadPreviewStatus::Error:      return 1;
						case EMetaRoadPreviewStatus::Warning:    return 2;
						case EMetaRoadPreviewStatus::Success:    return 3;
						default:                                 return 0;
						}
					})
					+ SWidgetSwitcher::Slot()[ ThrobberImage.ToSharedRef() ]
					+ SWidgetSwitcher::Slot()[ SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.ErrorWithColor")) ]
					+ SWidgetSwitcher::Slot()[ SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.WarningWithColor")) ]
					+ SWidgetSwitcher::Slot()[ SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.SuccessWithColor")) ]
				]
			]
		]
		// Update / Cancel button (icon-only): while a preview build is InProgress this turns into a Cancel
		// button (stop icon) that aborts the current mesh compute; otherwise it forces a preview rebuild.
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(2.f))
			.ToolTipText_Lambda([GetStatus]()
			{
				return GetStatus() == EMetaRoadPreviewStatus::InProgress
					? LOCTEXT("PreviewCancelTooltip", "Stop the in-progress road-mesh preview build")
					: LOCTEXT("PreviewUpdateTooltip", "Rebuild the road-mesh preview from the current spline data and settings");
			})
			.OnClicked_Lambda([this, GetStatus]()
			{
				if (UMetaRoadEditorMode* Mode = GetMode())
				{
					if (GetStatus() == EMetaRoadPreviewStatus::InProgress)
					{
						Mode->CancelPreviewRebuild();
					}
					else
					{
						Mode->RequestPreviewRebuild();
					}
				}
				return FReply::Handled();
			})
			.IsEnabled_Lambda([this]()
			{
				UMetaRoadEditorMode* Mode = GetMode();
				return Mode && Mode->HasActivePreview();
			})
			[
				SNew(SBox)
				.WidthOverride(16.f)
				.HeightOverride(16.f)
				[
					SNew(SWidgetSwitcher)
					.WidgetIndex_Lambda([GetStatus]() -> int32
					{
						return GetStatus() == EMetaRoadPreviewStatus::InProgress ? 1 : 0;
					})
					// 0 = Update (rebuild the preview)
					+ SWidgetSwitcher::Slot()
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.Refresh"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
					// 1 = Cancel (stop the in-progress build) — red tint to read as a stop/abort action.
					+ SWidgetSwitcher::Slot()
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.Toolbar.Stop"))
						.ColorAndOpacity(FStyleColors::AccentRed)
					]
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
