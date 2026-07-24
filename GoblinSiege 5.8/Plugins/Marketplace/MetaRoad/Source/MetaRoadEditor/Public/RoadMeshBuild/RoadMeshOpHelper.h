
/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Copyright Epic Games, Inc. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "BackgroundModelingComputeSource.h"
#include "MeshOpPreviewHelpers.h"
#include "IRoadOpCompute.h"
#include "InteractiveTool.h"
#include "UObject/WeakInterfacePtr.h"
#include "RoadMeshBuild/IRoadMeshBuildHost.h"
#include "RoadMeshOpHelper.generated.h"

namespace MetaRoad
{
	class FRoadComputePipeline;
}

class UMetaRoadBuildSettingsBase;
class UTriangulateRoadToolProperties; // used by GetEffectiveTriangulationProperties()


/**
 * URoadMeshOpPreviewWithBackgroundCompute
 *
 * UObject wrapper around UMeshOpPreviewWithBackgroundCompute that implements IRoadOpCompute,
 * enabling polymorphic ownership inside FRoadComputePipeline::LayerComputes[].
 *
 * Owns an FLambdaOperatorFactory internally — callers provide a MakeOp closure and never
 * interact with IDynamicMeshOperatorFactory directly.
 */
UCLASS(Transient)
class METAROADEDITOR_API URoadMeshOpPreviewWithBackgroundCompute
	: public UObject
	, public IRoadOpCompute
{
	GENERATED_BODY()

public:
	/**
	 * Initialize the compute. Creates an internal FLambdaOperatorFactory that calls MakeOp
	 * on a background thread each time a recompute is needed.
	 *
	 * @param Host            The build host. Used as outer for NewObject calls and for world access.
	 * @param RoadComputePipeline Weak pointer to the scope; passed to completion callbacks.
	 * @param MakeOp          Factory closure — called on background thread, must be thread-safe.
	 *                        Return MakeUnique<FDynamicMeshOperatorDummy>() to produce an empty result.
	 */
	void Setup(IRoadMeshBuildHost* Host, TWeakPtr<MetaRoad::FRoadComputePipeline> RoadComputePipeline,
	           TFunction<TUniquePtr<UE::Geometry::FDynamicMeshOperator>()> MakeOp);
	virtual bool ShutdownAndGenerateAssets(AActor* TargetActor, const FTransform3d& ActorToWorld) override;
	virtual void InvalidateResult() override { BackgroundCompute->InvalidateResult(); }
	virtual void CancelCompute() override { BackgroundCompute->CancelCompute(); }
	virtual void SetVisibility(bool bVisible) override { BackgroundCompute->SetVisibility(bVisible); }
	virtual void Tick(float DeltaTime) override { BackgroundCompute->Tick(DeltaTime); };
	virtual void ComputeSynchronous() override;
	virtual void EnableWireframe(bool bEnable) override { BackgroundCompute->PreviewMesh->EnableWireframe(bEnable); }
	virtual void Cancel() override { BackgroundCompute->Cancel(); }
	virtual bool HaveValidNonEmptyResult() const override { return bSyncResultValid || BackgroundCompute->HaveValidNonEmptyResult(); }
	virtual UE::Geometry::EBackgroundComputeTaskStatus GetLastComputeStatus() const override;
	virtual bool IsRoadAttribute() const { return bIsRoadAttribute; }
	virtual int GetNumVertices() const;
	virtual int GetNumTriangles() const;
	virtual UObject* AsObject() override { return Cast<UObject>(this); }
	virtual TSet<FName>& GetRebuildTags() override { return RebuildTags; }
	virtual const TSet<FName>& GetRebuildTags() const { return RebuildTags; }

	/** When true, GetRebuildTags() only triggers on attribute-rebuild tags, not full rebuilds. */
	UPROPERTY()
	bool bIsRoadAttribute = false;

	/** Set by ComputeSynchronous() when a non-empty mesh was produced on the calling thread (Bake path):
	 *  the inner async result flag is never set there, so HaveValidNonEmptyResult()/asset generation rely on this. */
	bool bSyncResultValid = false;

	/** Inner UE compute object that owns the preview mesh and background threading. */
	UPROPERTY()
	TObjectPtr<UMeshOpPreviewWithBackgroundCompute> BackgroundCompute;

	/** Base name for the generated static mesh asset (without SubGroup suffix). */
	UPROPERTY()
	FString BaseAssetName;

	/** The build property set this compute reads (drive surface / decals / ... settings). */
	UPROPERTY()
	TObjectPtr<UMetaRoadBuildSettingsBase> PropertySet;

	/** Material slots produced by the last completed operator, forwarded to the static mesh on Accept. */
	TArray<TPair<FName, TWeakObjectPtr<UMaterialInterface>>> ResultMaterialSlots;

	/** Weak interface reference to the build host (tool / preview builder). Weak by design: the host
	 *  owns this compute through the pipeline, so a strong reference would form a GC cycle. */
	TWeakInterfacePtr<IRoadMeshBuildHost> Host;

	/** Tags that trigger recompute of this specific operator (matched against Property meta tags). */
	TSet<FName> RebuildTags;

private:
	/** Apply a finished operator's result info + resolved materials. Shared by the async OnOpCompleted
	 *  callback and the synchronous ComputeSynchronous() path. */
	void ApplyOperatorResult(const UE::Geometry::FDynamicMeshOperator* Op);

	/** Triangulation properties to use: the scope target actor's per-actor build settings when present
	 *  (level preview/bake), otherwise the host's (profile preview). Game-thread only — never null while
	 *  a host is valid. */
	UTriangulateRoadToolProperties* GetEffectiveTriangulationProperties() const;

	/** Wraps the MakeOp closure passed to Setup(). Owned here so BackgroundCompute's raw pointer stays valid. */
	TUniquePtr<UE::Geometry::IDynamicMeshOperatorFactory> OwnedFactory;

	/** Owning pipeline (set in Setup) — used to aggregate result info. */
	TWeakPtr<MetaRoad::FRoadComputePipeline> OwningPipeline;
};
