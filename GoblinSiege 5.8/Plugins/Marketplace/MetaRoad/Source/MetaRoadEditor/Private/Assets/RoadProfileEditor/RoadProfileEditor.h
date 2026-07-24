/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Toolkits/AssetEditorToolkit.h"
#include "Textures/SlateIcon.h"
#include "RoadProfilePreviewBuilder.h"

class SRoadProfileViewport;
class SRoadProfileSelectionDetails;
class FMetaRoadToolPresetManager;
class FToolBarBuilder;

class FRoadProfileEditor
    : public FAssetEditorToolkit
    , public FGCObject
{
public:
    virtual ~FRoadProfileEditor() override;
    // IToolkit interface
    virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
    virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
    // End of IToolkit interface

    // FAssetEditorToolkit
    virtual FName GetToolkitFName() const override;
    virtual FText GetBaseToolkitName() const override;
    virtual FString GetWorldCentricTabPrefix() const override;
    virtual FLinearColor GetWorldCentricTabColorScale() const override;
    // End of FAssetEditorToolkit

    // FSerializableObject interface
    virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
    virtual FString GetReferencerName() const override { return TEXT("FRoadProfileEditor"); }
    // End of FSerializableObject interface

public:
    void InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, UObject* InObject);
    class URoadProfile* GetWorkingAsset() { return WorkingAsset; }

private:
    TObjectPtr<URoadProfile> WorkingAsset = nullptr;

    /** Drives the preview-scene mesh pipeline from the edited profile. GC-rooted via AddReferencedObjects. */
    TObjectPtr<URoadProfilePreviewBuilder> PreviewBuilder = nullptr;

    TSharedPtr<SRoadProfileViewport> ViewportWidget;

    /** Selection-driven details (selected lane / center / whole profile). */
    TSharedPtr<SRoadProfileSelectionDetails> SelectionDetails;

    /** Preset panel (preview-only: applies preset values to the transient build property sets). */
    TSharedPtr<FMetaRoadToolPresetManager> PresetManager;

    FDelegateHandle OnPropertyChangedHandle;

    ERoadProfilePreviewMode CurrentPreviewMode = ERoadProfilePreviewMode::RoadGraph;

    void OnObjectPropertyChanged(UObject* Object, struct FPropertyChangedEvent& Event);

    // Preview-mode dropdown added to the asset toolbar (Road Graph / Generated Mesh).
    void ExtendToolbar();
    void FillToolbar(FToolBarBuilder& ToolbarBuilder);

    /** Rebuild the preview after a preset's values are applied to the transient property sets. */
    void OnPresetApplied();
    TSharedRef<SWidget> GeneratePreviewModeMenu();
    FText GetPreviewModeLabel() const;
    FSlateIcon GetPreviewModeIcon() const;
    void SetPreviewMode(ERoadProfilePreviewMode Mode);
    bool IsPreviewModeActive(ERoadProfilePreviewMode Mode) const;

    // Lane editing context menu (built on right-click in the preview viewport).
    void OnLaneContextMenuRequested(int32 LaneIndex);
    void DoAddLane(bool bOnLeft);
    void DoDeleteLane();
    void DoReverseLane();
    bool IsLaneReversed() const;

    /** Mark the asset dirty + fire OnObjectPropertyChanged so the preview rebuilds AND UThumbnailManager
     *  re-renders the stored thumbnail on save (builder-based edits skip PostEditChangeProperty otherwise). */
    void NotifyProfileEdited();

    TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args);
};
