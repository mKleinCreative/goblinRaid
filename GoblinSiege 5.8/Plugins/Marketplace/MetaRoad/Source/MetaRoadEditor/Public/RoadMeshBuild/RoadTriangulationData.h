/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "IndexTypes.h"
#include "ModelingOperators.h"
#include "Geometry/Arrangement2d.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Engine/HitResult.h"
#include "RoadSplineComponent.h"
#include "IRoadPolygon.h"
#include "RoadMeshBuild/RoadSplineEditorComponent.h"
#include "RoadMeshBuild/ProceduralPolygon.h"

namespace MetaRoad
{
	// ---------------------------------------------------------------------------

	/**
	 * FRoadTriangulationParams
	 *
	 * Pass-through parameters produced by FRoadTriangulationOp and consumed by downstream layer
	 * operators (UV scaling, section splitting, spline-tessellation tolerances). Set once.
	 */
	struct FRoadTriangulationParams
	{
		double UV0ScaleFactor = 0;
		double UV1ScaleFactor = 0;
		double UV2ScaleFactor = 0;
		int    UVMaxSize = 13;
		bool   bSplitBySections = false;
		double MergeSectionsAreaThreshold = 0; // cm², already multiplied by 100*100
		double ChordToleranceSq = 0;           // squared chord-height tolerance for spline tessellation
		double MinSegmentLength = 0;
	};

	// ---------------------------------------------------------------------------

	/**
	 * FDebugLineBuffer
	 *
	 * Thread-safe collection of debug line batches. Written by operators on background threads
	 * (e.g. FSplineMeshOp when bDrawRefSplines), drained by UMetaRoadPreviewManager::RenderDebugLines()
	 * on the game thread. Encapsulates the lock so callers never touch a raw FCriticalSection.
	 */
	struct FDebugLineBuffer
	{
		struct FBatch
		{
			TArray<TPair<FVector, FVector>> Lines;
			FColor Color;
			float Thickness;
		};

		void Add(FBatch Batch)
		{
			FScopeLock Lock(&Mutex);
			Batches.Add(MoveTemp(Batch));
		}

		template <typename FFunc>
		void ForEach(FFunc&& Func) const
		{
			FScopeLock Lock(&Mutex);
			for (const FBatch& Batch : Batches)
			{
				Func(Batch);
			}
		}

	private:
		TArray<FBatch> Batches;
		mutable FCriticalSection Mutex;
	};

	// ---------------------------------------------------------------------------

	/**
	 * FSpatialQuery
	 *
	 * Owns the 2D/3D triangle meshes and their AABB trees used for ray/point queries against the
	 * triangulated road surface. Build() enforces the invariant that the backing FDynamicMesh3
	 * outlives the AABB tree that references it.
	 */
	struct FSpatialQuery
	{
		void Build(const TArray<FArrangementVertex3d>& Vertices3d, const TArray<FIndex3i>& Triangles);
		bool FindRayIntersection(const FVector2D& Point, double TopZ, FHitResult& HitOut) const;

	private:
		FDynamicMesh3 Mesh3d;            // backs Tree3d
		FDynamicMesh3 Mesh2d;            // Z=0 copy, backs Tree2d
		UE::Geometry::FDynamicMeshAABBTree3 Tree3d;
		UE::Geometry::FDynamicMeshAABBTree3 Tree2d;
	};

	// ---------------------------------------------------------------------------

	/**
	 * FRoadTriangulationData
	 *
	 * Shared triangulation result produced by FRoadTriangulationOp and consumed by all layer
	 * operators. Grouped concerns: Params (downstream parameters), DebugDraw (thread-safe debug
	 * lines), Spatial (mesh/AABB query). Arrangement/Vertices3d/Triangles/Polygons/Boundaries are
	 * the core geometry.
	 */
	struct METAROADEDITOR_API FRoadTriangulationData
	{
		~FRoadTriangulationData();

		// Inputs:

		FRoadTriangulationParams Params;
		FTransform ActorTransform;
		TArray<TStrongObjectPtr<URoadSplineEditorComponent>> Splines;
		TArray<FRoadPolygonData> SimplePolygones;

		// Outputs:

		FGeometryResult ResultInfo;
		FAxisAlignedBox3d Bounds;
		TUniquePtr<MetaRoad::FArrangement2d> Arrangement;
		TArray<FArrangementVertex3d> Vertices3d; // Vertices matched with Arrangement by ID
		TArray<TArray<FIndex2i>> Boundaries;
		TArray<FIndex3i> Triangles;
		TArray<TUniquePtr<FProceduralPolygon>> Polygons;
		FDebugLineBuffer DebugDraw;
		FSpatialQuery Spatial;

		void AddDebugLines(const TArray<FIndex2i>& InBoundaries, const FColor& Color, float Thickness);
		void AddDebugLines(int GID, const FColor& Color, float Thickness);
		bool IsBoundaryVertex(int VID) const;
		bool FindRayIntersection(const FVector2D& Point, FHitResult& HitOut) const;

		/**
		 * Rebuilds vertex normals from the current Vertices3d + Triangles (optionally CotanSmoothing the
		 * Z first), then (optionally) rebuilds the Spatial AABB tree. Pure geometry — never touches UWorld,
		 * so it is safe both on the op's background thread and on the game thread (e.g. after the
		 * Snap to Ground line-trace pass overrides per-vertex Z). Behaviour matches the inline code that
		 * previously lived at the end of FRoadTriangulationOp::CalculateResult().
		 */
		void FinalizeVertexGeometry(bool bCotanSmoothZ, float SmoothSpeed, float Smoothness,
		                            bool bRebuildSpatial, FProgressCancel* Progress = nullptr);
	};
}
