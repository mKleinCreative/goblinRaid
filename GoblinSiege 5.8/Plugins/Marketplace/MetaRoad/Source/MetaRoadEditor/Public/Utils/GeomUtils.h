/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RoadSplineComponent.h"
#include "Math/InterpCurve.h"
#include "Intersection/IntrSegment2Segment2.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"

namespace GeomUtils
{
	using namespace UE::Geometry;

	constexpr double DefSnapDistance = 0.1;

	template<typename RealType>
	bool FindPolylineSelfIntersection(
		const TArray<UE::Math::TVector2<RealType>>& Polyline,
		UE::Math::TVector2<RealType>& IntersectionPointOut,
		FIndex2i& IntersectionIndexOut,
		bool bParallel = true)
	{
		std::atomic<bool> bSelfIntersects(false);
		bool IsLoop = (Polyline[0] - Polyline.Last()).IsNearlyZero(UE_KINDA_SMALL_NUMBER);
		int32 N = Polyline.Num() - (int)IsLoop;

		if (N == 0)
		{
			return false;
		}

		ParallelFor(N - 1, [&](int32 i)
		{
			TSegment2<RealType> SegA(Polyline[i], Polyline[i + 1]);
			for (int32 j = i + 2; j < N - 1 && bSelfIntersects == false; ++j)
			{
				TSegment2<RealType> SegB(Polyline[j], Polyline[j + 1]);
				if (SegA.Intersects(SegB) && bSelfIntersects == false)
				{
					bool ExpectedValue = false;
					if (std::atomic_compare_exchange_strong(&bSelfIntersects, &ExpectedValue, true))
					{
						UE::Geometry::TIntrSegment2Segment2<RealType> Intersection(SegA, SegB);
						Intersection.Find();
						IntersectionPointOut = Intersection.Point0;
						IntersectionIndexOut = FIndex2i(i, j);
						return;
					}
				}
			}
		}, (bParallel) ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread);

		return bSelfIntersects;
	}

	METAROADEDITOR_API void RemovedPolylineSelfIntersection(TArray<FVector2D>& Polyline, bool bParallel = true);

	/**
	 * Collapse consecutive vertices closer than MinDistance. Treats the polyline as closed when
	 * bClosed (also collapses the wrap last<->first). Never reduces below 3 vertices. Prevents
	 * degenerate sliver triangles from near-coincident vertices.
	 */
	METAROADEDITOR_API void RemoveClosePolylineVertices(TArray<FVector2D>& Polyline, double MinDistance = DefSnapDistance, bool bClosed = true);

	/**
	 * Collapse near-coincident vertices of a closed 3D ring, comparing positions in the XY plane (Z is
	 * kept on the retained vertices). Also collapses the closing wrap (last<->first). Use before feeding
	 * a polygon to an XY triangulator: coincident XY vertices create zero-length edges that make the
	 * constrained Delaunay return no triangles. Returns true when at least 3 distinct vertices remain.
	 */
	METAROADEDITOR_API bool RemoveCloseRingVerticesXY(TArray<FVector>& Vertices, double MinDistance = DefSnapDistance);

	/**
	 * Convert an FInterpCurveVector2D to a polyline using adaptive binary subdivision.
	 * The result is denser where the curve bends sharply and sparser where it is nearly linear.
	 * Handles closed polygons (the closing segment uses LoopKeyOffset).
	 * @param ChordTolerance  Linear chord tolerance: a segment is subdivided further when
	 *                        the midpoint deviates more than this distance from the chord.
	 *                        Floored to 0.001 to avoid pathological full-depth subdivision.
	 */
	METAROADEDITOR_API TArray<FVector2D> ConvertInterpCurveToPolyline(const FInterpCurveVector2D& Curve, float ChordTolerance);

	template<class TRoadPosition = FRoadPosition>
	void NormalizePolyline(TArray<TRoadPosition>& Polyline, double SnapToDistance = DefSnapDistance)
	{
		Polyline.Sort([](auto& A, auto& B) { return A.SOffset < B.SOffset; });

		for (auto It = Polyline.CreateIterator() + 1; It; ++It)
		{
			if (It->SOffset - (It - 1)->SOffset < SnapToDistance)
			{
				It.RemoveCurrent();
			}
		}
	}

	template<class TRoadPosition = FRoadPosition>
	TArray<TRoadPosition> GetSubPolyline(const TArray<TRoadPosition>& Vertices, double S0, double S1, double SnapToDistance = DefSnapDistance)
	{
		int32 StartKey = 0;
		int32 EndKey = Vertices.Num() - 1;

		for (int32 KeyIndex = 0; KeyIndex < Vertices.Num(); ++KeyIndex)
		{
			const double CurrentS = Vertices[KeyIndex].SOffset;
			if (CurrentS < S0)
			{
				StartKey = KeyIndex;
			}
			if (CurrentS > S1)
			{
				EndKey = KeyIndex;
				break;
			}
		}

		TArray<TRoadPosition> SubLane(&Vertices[StartKey], EndKey - StartKey + 1);

		if (SubLane.Num())
		{
			if (SubLane[0].SOffset < S0)
			{
				if (SubLane.Num() > 1)
				{
					double Alpha = (S0 - SubLane[0].SOffset) / (SubLane[1].SOffset - SubLane[0].SOffset);
					SubLane[0].Location = FMath::Lerp(SubLane[0].Location, SubLane[1].Location, Alpha);
					if ((SubLane[0].Location - SubLane[1].Location).Size() < SnapToDistance)
					{
						SubLane.RemoveAt(0, EAllowShrinking::No);
					}
				}
				SubLane[0].SOffset = S0;
			}

			if (SubLane[SubLane.Num() - 1].SOffset > S1)
			{
				if (SubLane.Num() > 1)
				{
					double Alpha = (S1 - SubLane[SubLane.Num() - 2].SOffset) / (SubLane[SubLane.Num() - 1].SOffset - SubLane[SubLane.Num() - 2].SOffset);
					SubLane[SubLane.Num() - 1].Location = FMath::Lerp(SubLane[SubLane.Num() - 2].Location, SubLane[SubLane.Num() - 1].Location, Alpha);
					if ((SubLane[SubLane.Num() - 1].Location - SubLane[SubLane.Num() - 2].Location).Size() < SnapToDistance)
					{
						SubLane.RemoveAt(SubLane.Num() - 1, EAllowShrinking::No);
					}
				}
				SubLane[SubLane.Num() - 1].SOffset = S1;
			}
		}

		NormalizePolyline(SubLane);

		return MoveTemp(SubLane);
	}

	template<class TRoadPosition = FRoadPosition>
	void RemoveSegmant(TArray<TRoadPosition>& Vertices, double S0, double S1)
	{
		int IndexStart = INDEX_NONE;
		int IndexEnd = INDEX_NONE;

		for (int i = 0; i < Vertices.Num(); ++i)
		{
			if (Vertices[i].SOffset >= S0 && Vertices[i].SOffset <= S1)
			{
				if (IndexStart == INDEX_NONE)
				{
					IndexStart = i;
				}
				IndexEnd = i;
			}
		}

		if (IndexStart != INDEX_NONE)
		{
			Vertices.RemoveAt(IndexStart, IndexEnd - IndexStart + 1, EAllowShrinking::No);
		}
	}

	template<class TRoadPosition = FRoadPosition>
	void InsertPolyline(TArray<TRoadPosition>& DstVertices, const TArray<TRoadPosition>& SrcVertices, double SnapToDistance = DefSnapDistance)
	{
		if (SrcVertices.Num() == 0)
		{
			return;
		}

		if (SrcVertices.Num() == 1)
		{
			DstVertices.Add(SrcVertices[0]);
		}
		else
		{
			const double S0 = SrcVertices[0].SOffset;
			const double S1 = SrcVertices.Last().SOffset;

			RemoveSegmant(DstVertices, S0, S1);

			DstVertices.Append(SrcVertices);
		}

		NormalizePolyline(DstVertices);
	}

	struct METAROADEDITOR_API FPolygonIntersectApproximation
	{
		FPolygonIntersectApproximation() = default;
		FPolygonIntersectApproximation(const TArray<FVector>& Vertices)
		{
			UpdateAABBTree(Vertices);
		}
		bool UpdateAABBTree(const TArray<FVector>& Vertices);
		bool Intersect(const FVector2D& Point, FVector& OutPoint, FVector& OutNormal) const;

	private:
		UE::Geometry::FDynamicMesh3 DynamicMesh2d; 
		UE::Geometry::FDynamicMesh3 DynamicMesh3d;
		UE::Geometry::FDynamicMeshAABBTree3 AABBTree2d;
		double MaxZ;
	};

} // GeomUtils