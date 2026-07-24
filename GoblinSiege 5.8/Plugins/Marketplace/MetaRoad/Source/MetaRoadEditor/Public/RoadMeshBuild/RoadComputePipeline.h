/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "ModelingOperators.h"
#include "MeshOpPreviewHelpers.h"
#include "Utils/StrongScriptInterface.h"
#include "RoadMeshBuild/IRoadOpCompute.h"
#include "RoadMeshBuild/RoadTriangulationData.h"

class IRoadMeshBuildHost;
class AMetaRoad;

namespace MetaRoad
{
	class FRoadComputePipeline;

	// ---------------------------------------------------------------------------

	/**
	 * FRoadComputePipeline
	 *
	 * Per-actor (or per-SubGroup) container that owns AND drives the full compute stack for one road
	 * actor: the base triangulation (FRoadTriangulationOp via TriangulationCompute) plus the layer
	 * operators (LayerComputes) that consume the shared FRoadTriangulationData in parallel.
	 *
	 * Owns its lifecycle: Initialize() builds the stack, Tick() drives rebuilds + background computes.
	 * Rebuild requests from tool events are deferred (MarkDirty / RequestRebuildAll /
	 * HandlePropertyModified) and flushed on the next Tick(). Held via TSharedPtr; derives
	 * TSharedFromThis so it can hand a weak self-reference to the op factory and result callback.
	 *
	 * One scope per distinct SubGroup on the actor's splines (NAME_None = all splines, legacy path).
	 */
	class METAROADEDITOR_API FRoadComputePipeline : public TSharedFromThis<FRoadComputePipeline>
	{
	public:
		FRoadComputePipeline(TWeakObjectPtr<AMetaRoad> InTargetActor, FName InSubGroup,
		                       TArray<TWeakObjectPtr<URoadSplineComponent>> InGroupSplines);

		/** Build the compute stack (triangulation factory/compute + layer operators) and wire callbacks. */
		void Initialize(IRoadMeshBuildHost& Host);

		/** Drive one frame: flush dirty/pending rebuilds, tick the background computes. Returns true if a rebuild was started this frame. */
		bool Tick(float DeltaTime);

		/** Build the whole stack synchronously on the calling thread (triangulation + every layer), without
		 *  background threading. Requires Initialize() to have been called. Used by synchronous hosts (thumbnails). */
		void BuildSynchronous();

		// Rebuild scheduling — called from tool event handlers, executed on the next Tick().
		void MarkDirty() { bIsDirty = true; }
		void RequestRebuildAll() { bRebuildAllPending = true; }
		void HandlePropertyModified(const FProperty& Property);
		void SetWireframe(bool bEnable);

		// Finalization / queries.
		void CancelAll();
		/** Cancel any in-flight background compute (triangulation + layers) WITHOUT destroying the preview
		 *  meshes. Unlike CancelAll() — which calls Cancel() on each layer, nulling its PreviewMesh and thus
		 *  requiring an immediate ResetPipelines() — this uses CancelCompute(), so the last-displayed meshes
		 *  survive and the pipeline stays tickable. Pending rebuild requests are dropped so the next Tick()
		 *  idles instead of restarting the build. */
		void CancelActiveComputes();
		/** Write every layer's asset(s)/component(s) onto OutputActor. @return false if any layer's write failed. */
		bool GenerateAssets(AActor* OutputActor, const FTransform3d& ActorToWorld);
		bool CanAccept() const;
		int  CountActiveTasks() const;
		bool TryFlushReport();

		/** Aggregate a layer operator's result info into this scope's status (called on op completion). */
		void AppendResultInfo(const UE::Geometry::FGeometryResult& Result);

		AMetaRoad* GetTargetActor() const { return TargetActor.Get(); }
		FName   GetSubGroup() const { return SubGroup; }
		const TArray<TWeakObjectPtr<URoadSplineComponent>>& GetGroupSplines() const { return GroupSplines; }
		const TSharedPtr<MetaRoad::FRoadTriangulationData>& GetTriangulationResult() const { return TriangulationResult; }
		const UE::Geometry::FGeometryResult& GetResultInfo() const { return GenerationResultInfo; }

	private:
		void DoRebuildAll();
		void DoRebuildOne(IRoadOpCompute& LayerCompute);
		void ShowReport() const;

		/**
		 * Game-thread "Snap to Ground" pass: when the actor's OverlapStrategy is SnapToGround, line-trace
		 * straight down under every triangulation vertex and pull its Z onto the hit point, then re-finalize
		 * normals + AABB. No-op for other strategies or when no target world is available. Must run after the
		 * triangulation result is set and BEFORE layer ops consume it.
		 */
		void ApplyGroundSnap(MetaRoad::FRoadTriangulationData& Data);

		// Raw (not weak) on purpose: this pipeline is owned by the host, so OwningHost always outlives it.
		IRoadMeshBuildHost* OwningHost = nullptr;
		TWeakObjectPtr<AMetaRoad> TargetActor;
		FName SubGroup = NAME_None;
		TArray<TWeakObjectPtr<URoadSplineComponent>> GroupSplines;

		/** Shared triangulation result produced by TriangulationCompute and consumed by all LayerComputes. */
		TSharedPtr<MetaRoad::FRoadTriangulationData> TriangulationResult;

		/** Owned lambda factory for the base operator. Must outlive TriangulationCompute. */
		TUniquePtr<UE::Geometry::IGenericDataOperatorFactory<MetaRoad::FRoadTriangulationData>> TriangulationOpFactory;

		/** Background compute that drives FRoadTriangulationOp. Notified when TriangulationResult changes. */
		TUniquePtr<TGenericDataBackgroundCompute<MetaRoad::FRoadTriangulationData>> TriangulationCompute;

		/** Mesh and attribute compute wrappers — one per registered operator. */
		TArray<TStrongScriptInterface<IRoadOpCompute>> LayerComputes;

		UE::Geometry::FGeometryResult GenerationResultInfo = {};
		bool bNeedGenerateReport = false;
		bool bIsDirty = true; // start dirty so the first Tick builds

		bool bRebuildAllPending = false;
		TSet<IRoadOpCompute*> PendingRebuildOps;
	};
}
