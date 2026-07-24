/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EditorMode/MetaRoadFbxExportSettings.h" // EMetaRoadFbx* enums (snapshotted)
#include "Misc/AsyncTaskNotification.h"
#include "MetaRoadFbxExportHost.generated.h"

class UWorld;
class AMetaRoad;
class UFbxExportOption;
class UMetaRoadFbxBuildHost;
class UMetaRoadFbxExportSettings;
class UPrimitiveComponent;

/**
 * UMetaRoadFbxExportHost
 *
 * Drives an FBX export of road meshes, ONE road per Tick (mirrors UMetaRoadBakeHost's lifecycle): the owner
 * calls BeginExportAsync() then ticks TickExport() each frame, which shows a cook-style FAsyncTaskNotification
 * with progress + Cancel and writes the .fbx files. Source meshes come either from the already-baked
 * LastGeneratedActor (GeneratedAssets) or are regenerated in RAM via UMetaRoadFbxBuildHost (FromScratch).
 * Layout (per-actor / combined / per-component) and pivot (centered / world) come from UMetaRoadFbxExportSettings.
 */
UCLASS(Transient)
class UMetaRoadFbxExportHost : public UObject
{
	GENERATED_BODY()

public:
	/** Begin exporting the given road actors. Drive with TickExport() until it returns false. */
	void BeginExportAsync(UWorld* InWorld, const TArray<TWeakObjectPtr<AMetaRoad>>& InRoads, UMetaRoadFbxExportSettings* InSettings);

	/** Advance the export one road. Returns true while still running; false when finished or cancelled
	 *  (the owner should then drop this host). */
	bool TickExport(float DeltaTime);

	bool IsExporting() const { return bExporting; }

private:
	void ProcessNextRoad();
	void FinalizeAndReport();
	void AbortAndReport(const FText& Title, const FText& Body);
	void Finish();

	// Source resolution / component gathering.
	AActor* ResolveGeneratedSourceActor(AMetaRoad* Road) const;
	void GatherExportComponents(AActor* Source, TArray<UPrimitiveComponent*>& OutComps) const;

	// Export primitives. Every export goes through a transient proxy actor that re-hosts the (filtered)
	// components positioned for the pivot — so the static/spline filter is always honoured and the pivot is
	// uniform. Combined forces world pivot (bForceWorldPivot) so the roads keep their relative layout.
	void EnsureExporterInitialized();
	AActor* BuildProxyActor(const TArray<UPrimitiveComponent*>& Comps, const FTransform& SourceXf, bool bForceWorldPivot);
	bool ExportToOwnFile(AActor* SourceActor, const TArray<UPrimitiveComponent*>& Comps, const FString& FilePath);
	void ExportIntoCombinedDoc(AActor* SourceActor, const TArray<UPrimitiveComponent*>& Comps);
	FString MakeFilePath(const FString& BaseName) const;

	// ---- Settings snapshot (the CDO could change between ticks) ----
	EMetaRoadFbxMeshSource  MeshSource = EMetaRoadFbxMeshSource::GeneratedAssets;
	EMetaRoadFbxExportLayout Layout    = EMetaRoadFbxExportLayout::PerActor;
	EMetaRoadFbxExportPivot  Pivot     = EMetaRoadFbxExportPivot::ActorOrigin;
	FString OutputDir;
	FString FilePrefix;
	FString CombinedFileName;
	bool bOverwrite     = true;
	bool bExportStatic  = true;
	bool bExportSpline  = true;
	bool bOpenFolder    = true;

	UPROPERTY()
	TObjectPtr<UFbxExportOption> Options;

	UPROPERTY()
	TObjectPtr<UMetaRoadFbxBuildHost> BuildHost;

	TWeakObjectPtr<UWorld> World;
	TArray<TWeakObjectPtr<AMetaRoad>> Roads;
	int32 CurrentIndex = 0;
	bool bExporting = false;

	// FFbxExporter singleton / combined-document state.
	bool bExporterInitialized = false;
	bool bCombinedDocOpen = false;
	FString CombinedPath;

	// Result counters (surfaced in the completion notification).
	int32 NumExported = 0;        // .fbx files written
	int32 NumCombinedRoads = 0;   // roads added to the single Combined file
	int32 NumSkippedNotBaked = 0;
	int32 NumSkippedOverwrite = 0;
	int32 NumFailed = 0;

	TUniquePtr<FAsyncTaskNotification> Notification;
};
