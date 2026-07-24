/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Toolkits/AssetEditorToolkit.h"

class FRoadMarkProfileEditor 
    : public FAssetEditorToolkit
    , public FGCObject
{
public: 
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
    virtual FString GetReferencerName() const override { return TEXT("FRoadMarkProfileEditor"); }
    // End of FSerializableObject interface

public:
    void InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, UObject* InObject);
    class URoadMarkProfile* GetWorkingAsset() { return WorkingAsset; }

private:
    TObjectPtr<URoadMarkProfile> WorkingAsset = nullptr;

    TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args);
};
