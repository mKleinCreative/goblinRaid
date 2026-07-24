/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Toolkits/BaseToolkit.h"
#include "StatusBarSubsystem.h"

class IAssetViewport;
class IDetailsView;
class STextBlock;
struct FCurveSequence;
class URoadSplineComponent;
class UMetaRoadEditorMode;
class SMetaRoadAttributeTree;
class SMetaRoadBakePanel;
class SMetaRoadFbxExportPanel;
class SMetaRoadPresetPanel;
class SMetaRoadVisibilityPanel;

class FMetaRoadEditorModeToolkit : public FModeToolkit
{
public:

	FMetaRoadEditorModeToolkit();
	~FMetaRoadEditorModeToolkit();

	/** FModeToolkit interface */
	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual TSharedPtr<class SWidget> GetInlineContent() const override;

	// set/clear notification message area
	virtual void PostNotification(const FText& Message);
	virtual void ClearNotification();

	// set/clear warning message area
	virtual void PostWarning(const FText& Message);
	virtual void ClearWarning();

	/** Returns the Mode specific tabs in the mode toolbar **/
	virtual FText GetToolPaletteDisplayName(FName PaletteName) const override;
	virtual void GetToolPaletteNames(TArray<FName>& PaletteNames) const override;
	virtual void BuildToolPalette(FName PaletteName, class FToolBarBuilder& ToolbarBuilder) override;
	virtual void OnToolPaletteChanged(FName PaletteName) override;
	virtual bool HasIntegratedToolPalettes() const { return true; }
	virtual bool HasExclusiveToolPalettes() const { return false; }

	virtual FText GetActiveToolDisplayName() const override { return ActiveToolName; }
	virtual FText GetActiveToolMessage() const override { return ActiveToolMessage; }

	virtual void ShowRealtimeAndModeWarnings(bool bShowRealtimeWarning);

	virtual void OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual void OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;

	void OnActiveViewportChanged(TSharedPtr<IAssetViewport>, TSharedPtr<IAssetViewport> );

	virtual void InvokeUI() override;
	virtual void ShutdownUI() override;

	// @return true if we are currently in a tool, used to enable/disable UI things
	bool IsInActiveTool() const { return bInActiveTool; }

	// Re-point the Preset panel's working-copy details view (called by the mode when the working copies change).
	void RefreshPresetPanel();
	// Re-point the embedded selection details view (left panel) at the currently selected road spline,
	// and force a refresh so FRoadSplineComponentDetails re-runs for the current edit sub-mode.
	// Subscribed to FMetaRoadEditorModule::OnSelectedSplineChanged: re-points the view when the active
	// visualizer's selected spline changes (e.g. a lane click on a different spline of the same AMetaRoad).
	void RefreshSelectionDetailsView();
	// The single selected road actor's spline component, or null if zero / more than one is selected
	// (the embedded selection editor handles a single object). Drives the "select a spline" placeholder.
	URoadSplineComponent* GetSelectedRoadSplineForPanel() const;

private:
	// Resolve the owning editor mode (DRY helper for the many lambdas/handlers that need it).
	UMetaRoadEditorMode* GetMode() const;

	// Live-preview status + Update icon buttons shown to the right of the Schematic/Preview toggle (replaces
	// the old viewport overlay): the status icon opens the Output Log on click; Update forces a preview rebuild.
	TSharedRef<SWidget> BuildPreviewControls();
	TSharedPtr<FCurveSequence> PreviewThrobberAnim;

	// Sub-mode predicates — drive the content switcher (which panel is shown) + tile checked state.
	bool IsVisibilitySubModeActive() const;
	bool IsAttributeSubModeActive() const;
	bool IsPresetSubModeActive() const;
	bool IsBakeSubModeActive() const;
	bool IsFbxExportSubModeActive() const;
	// True for any spline-editing sub-mode (Spline/Section/Offset/Width/Attribute) with no active tool.
	bool IsEditSubModeActive() const;

	// Extracted sub-mode panels (each owns its own state; the toolkit just hosts them in the content switcher).
	TSharedPtr<SMetaRoadAttributeTree>   AttributeTree;     // Edit > Attribute tree
	TSharedPtr<SMetaRoadBakePanel>       BakePanel;          // Assets > Bake
	TSharedPtr<SMetaRoadFbxExportPanel>  FbxExportPanel;     // Assets > FBX Export
	TSharedPtr<SMetaRoadPresetPanel>     PresetPanel;        // Preset
	TSharedPtr<SMetaRoadVisibilityPanel> VisibilityPanel;    // Misc > Visibility

	// Embedded view that hosts the road-spline "Selection" editor (was the right Details "Selection" category).
	TSharedPtr<IDetailsView> SelectionDetailsView;

	bool bInActiveTool = false;
	FText ActiveToolName;
	FText ActiveToolMessage;
	FStatusBarMessageHandle ActiveToolMessageHandle;

	TSharedPtr<SWidget> ToolkitWidget;
	void UpdateActiveToolProperties();
	void InvalidateCachedDetailPanelState(UObject* ChangedObject);

	TSharedPtr<SWidget> ToolShutdownViewportOverlayWidget;

	TSharedPtr<STextBlock> ModeWarningArea;
	TSharedPtr<STextBlock> ToolWarningArea;

	FText ActiveWarning{};
};
