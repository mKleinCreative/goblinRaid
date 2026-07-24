/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "Templates/Casts.h"
#include "UObject/Interface.h"
#include "IRoadMeshBuildHost.generated.h"

class UWorld;
class UMaterialInterface;
class UMetaRoadBuildSettings;
class AMetaRoad;
class UActorComponent;

namespace MetaRoad { class FRoadComputePipeline; }

/**
 * Aggregated status of one TickPipelines() pass — consumed by hosts that show build status UI
 * (e.g. the editor mode's preview status button). Pure data, no UI semantics.
 */
struct FRoadMeshBuildTickResult
{
	bool bAnyRebuildStarted = false;
	int  TotalActiveTasks = 0;
	bool bHasFailed = false;
	bool bHasWarnings = false;
	bool bReportShown = false;
};

UINTERFACE(MinimalAPI)
class URoadMeshBuildHost : public UInterface
{
	GENERATED_BODY()
};

/**
 * IRoadMeshBuildHost
 *
 * Reusable road-mesh build engine + host contract. Owns the FRoadComputePipeline stack and drives
 * its lifecycle (build / tick / rebuild / cancel), so the async UMetaRoadBakeHost, the live
 * UMetaRoadPreviewManager, the URoadProfilePreviewBuilder and a future road ActorComponent share one
 * implementation instead of each re-driving the pipelines.
 *
 * Declared as a UE UInterface (URoadMeshBuildHost) so implementers are discoverable via Cast<>,
 * TScriptInterface and GetComponentByInterface(). It stays a non-UObject mixin (UObject hosts already
 * inherit UObject; only one UObject base is allowed), so its state (RoadComputePipelines) is plain
 * C++ — the UObjects inside each pipeline are GC-rooted by TStrongScriptInterface, not by this class.
 *
 * Contract methods (pure virtual) are implemented by the concrete host; engine methods are concrete
 * and operate on RoadComputePipelines.
 */
class METAROADEDITOR_API IRoadMeshBuildHost
{
	GENERATED_BODY()

public:
	// ---- Host contract (implemented by the concrete UObject host) ----

	/** World the preview meshes are placed in. */
	virtual UWorld* GetTargetWorld() const = 0;

	/** Invalidate any viewport showing the preview. */
	virtual void NotifyMeshUpdated() = 0;

	/** Material shown on the preview mesh while a background compute is running (may be null). */
	virtual UMaterialInterface* GetWorkingMaterial() = 0;

	/** Build settings holder an actor's preview/build sources all its property sets from (triangulation +
	 *  layers). Default is the actor's own per-actor UMetaRoadBuildSettings (null only for non-AMetaRoad
	 *  targets — consumers then fall back to CDO defaults). Every host ensures its target actors have a
	 *  holder on the game thread before building. Hosts may override to redirect: e.g. UMetaRoadPreviewManager
	 *  returns a transient working copy while editing in the Preset sub-mode, so edits preview without
	 *  touching the actor's real settings until Apply. */
	virtual UMetaRoadBuildSettings* GetBuildSettingsForActor(AMetaRoad* Actor) const;

	/** UObject used as NewObject<>() outer / GC root and as a weak validity guard. */
	UObject* GetHostObject() { return Cast<UObject>(this); }

	/** When true, GenerateAssets() materializes the layer meshes as TRANSIENT UStaticMeshes (in the transient
	 *  package, no disk asset / asset-registry / autosave) instead of persistent content assets. Used by the
	 *  FBX "From scratch" export host, which builds geometry in RAM only. Default false (normal bake/preview). */
	virtual bool WantsTransientMeshOutput() const { return false; }

	// ---- Reusable build engine (operates on RoadComputePipelines) ----

	/** Build the pipeline stack from the given AMetaRoad actors (one scope per distinct SubGroup). Splines
	 *  living on non-AMetaRoad actors are not accepted here, so they never enter the build pipeline. */
	void SetSplineActors(const TArray<TWeakObjectPtr<AMetaRoad>>& SplineActors);

	/** Initialize every built pipeline (wires the compute stack against this host). */
	void InitializePipelines();

	/** Queue a full rebuild of every pipeline on its next tick. */
	void RequestRebuildAll();

	/** Build every pipeline synchronously on the calling thread (no background threading, no ticks). */
	void BuildAllSynchronous();

	/** Mark the pipeline that owns the given component's actor dirty (deferred rebuild). */
	void MarkActorDirty(UActorComponent* Component);

	/** Drive one frame of every pipeline; fills Out with aggregate status. Returns bAnyRebuildStarted. */
	bool TickPipelines(float DeltaTime, FRoadMeshBuildTickResult& Out);

	/** Cancel all active background computes (does not clear the pipeline array). */
	void CancelAllPipelines();

	/** Release the pipeline stack. */
	void ResetPipelines() { RoadComputePipelines.Reset(); }

	const TArray<TSharedPtr<MetaRoad::FRoadComputePipeline>>& GetPipelines() const { return RoadComputePipelines; }

protected:
	/** Plain C++ state (not a UPROPERTY): pipelines hold GC-rooting TStrongScriptInterface computes. */
	TArray<TSharedPtr<MetaRoad::FRoadComputePipeline>> RoadComputePipelines;
};
