/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "Utils/GeomUtils.h"
#include "CompGeom/Delaunay2.h"

namespace GeomUtils
{
using namespace UE::Geometry;

// ─── ConvertInterpCurveToPolyline ─────────────────────────────────────────────

namespace
{
	void SubdivideSegment(
		const FInterpCurveVector2D& Curve,
		float T0, float T1,
		FVector2D P0, FVector2D P1,
		float MaxSqDist,
		TArray<FVector2D>& Out,
		int32 Depth)
	{
		if (Depth <= 0) { Out.Add(P1); return; }

		const float    TMid = (T0 + T1) * 0.5f;
		const FVector2D PMid(Curve.Eval(TMid, FVector2D::ZeroVector));

		const float SqDist = FMath::PointDistToSegmentSquared(
			FVector(PMid.X, PMid.Y, 0.f),
			FVector(P0.X,   P0.Y,   0.f),
			FVector(P1.X,   P1.Y,   0.f));

		if (SqDist <= MaxSqDist)
		{
			Out.Add(P1);
		}
		else
		{
			SubdivideSegment(Curve, T0,   TMid, P0,   PMid, MaxSqDist, Out, Depth - 1);
			SubdivideSegment(Curve, TMid, T1,   PMid, P1,   MaxSqDist, Out, Depth - 1);
		}
	}
} // anonymous namespace

TArray<FVector2D> ConvertInterpCurveToPolyline(const FInterpCurveVector2D& Curve, float ChordTolerance)
{
	TArray<FVector2D> Result;
	int32 N = Curve.Points.Num();
	if (N < 2) return Result;

	// Floor the tolerance: a near-zero value forces pathological full-depth subdivision.
	const float MaxDist = FMath::Max(ChordTolerance, 0.001f);
	const float MaxSqDist = MaxDist * MaxDist;   // SubdivideSegment compares squared distance

	// A looped curve may carry a trailing control point coincident with the first
	// (e.g. SVG paths that return to start before closing). It produces a zero-length
	// seam segment -> degenerate slivers / missing triangles downstream. Ignore it here
	// so the closing is handled solely by the implicit loop wrap below.
	if (Curve.bIsLooped && N > 2 &&
		FVector2D::DistSquared(FVector2D(Curve.Points[N - 1].OutVal), FVector2D(Curve.Points[0].OutVal)) < 1e-6f)
	{
		--N;
	}

	const float LoopDT = Curve.LoopKeyOffset > 1e-6f ? Curve.LoopKeyOffset : 1.f;

	Result.Add(FVector2D(Curve.Points[0].OutVal));

	constexpr int32 MaxDepth = 16;
	for (int32 i = 0; i < N; ++i)
	{
		const int32   j  = (i + 1) % N;
		const float   T0 = Curve.Points[i].InVal;
		const float   T1 = (j == 0) ? T0 + LoopDT : Curve.Points[j].InVal;
		const FVector2D P0(Curve.Points[i].OutVal);
		const FVector2D P1(Curve.Points[j].OutVal);

		SubdivideSegment(Curve, T0, T1, P0, P1, MaxSqDist, Result, MaxDepth);
	}

	// Closed polygon: last point equals first — remove the duplicate
	if (Result.Num() > 1 && FVector2D::DistSquared(Result.Last(), Result[0]) < 1e-6f)
		Result.Pop();

	return Result;
}

// ─────────────────────────────────────────────────────────────────────────────

void RemoveClosePolylineVertices(TArray<FVector2D>& Polyline, double MinDistance, bool bClosed)
{
	if (Polyline.Num() < 4)
	{
		return;
	}

	const double MinDistSq = MinDistance * MinDistance;

	TArray<FVector2D> Out;
	Out.Reserve(Polyline.Num());
	Out.Add(Polyline[0]);

	for (int32 i = 1; i < Polyline.Num(); ++i)
	{
		if (FVector2D::DistSquared(Polyline[i], Out.Last()) > MinDistSq)
		{
			Out.Add(Polyline[i]);
		}
	}

	// Collapse the closing wrap (last coincident with first).
	if (bClosed)
	{
		while (Out.Num() > 3 && FVector2D::DistSquared(Out.Last(), Out[0]) <= MinDistSq)
		{
			Out.Pop(EAllowShrinking::No);
		}
	}

	// Never reduce below a valid polygon; leave degenerate input untouched for downstream checks.
	if (Out.Num() >= 3)
	{
		Polyline = MoveTemp(Out);
	}
}

bool RemoveCloseRingVerticesXY(TArray<FVector>& Vertices, double MinDistance)
{
	const double MinDistSq = MinDistance * MinDistance;

	TArray<FVector> Out;
	Out.Reserve(Vertices.Num());
	for (const FVector& V : Vertices)
	{
		if (Out.Num() == 0 ||
		    FVector2D::DistSquared(FVector2D(V), FVector2D(Out.Last())) > MinDistSq)
		{
			Out.Add(V);
		}
	}

	// Collapse the closing wrap (last coincident with first).
	while (Out.Num() > 3 &&
	       FVector2D::DistSquared(FVector2D(Out.Last()), FVector2D(Out[0])) <= MinDistSq)
	{
		Out.Pop(EAllowShrinking::No);
	}

	Vertices = MoveTemp(Out);
	return Vertices.Num() >= 3;
}

void RemovedPolylineSelfIntersection(TArray<FVector2D>& Polyline, bool bParallel )
{
	// Remove self intersections
	FVector2D IntersectionPoint;
	FIndex2i IntersectionIndex;
	while (FindPolylineSelfIntersection(Polyline, IntersectionPoint, IntersectionIndex, true))
	{
		FVector2D PtA = Polyline[IntersectionIndex.A];
		FVector2D PtB = Polyline[IntersectionIndex.B];
		Polyline.RemoveAt(IntersectionIndex.A + 1, IntersectionIndex.B - IntersectionIndex.A, EAllowShrinking::No);
		Polyline.Insert(IntersectionPoint, IntersectionIndex.A + 1);
	}
};

bool FPolygonIntersectApproximation::UpdateAABBTree(const TArray<FVector>& Vertices)
{
	using namespace UE::Geometry;

	MaxZ = 0;

	// De-duplicate near-coincident ring vertices before triangulating. The constrained Delaunay works
	// in XY, and coincident XY vertices produce zero-length edges that make GetFilledTriangles() return
	// 0 (e.g. the apex chevron polygon collapses its inner edge to a point).
	TArray<FVector> CleanedVertices = Vertices;
	if (!RemoveCloseRingVerticesXY(CleanedVertices))
	{
		return false; // degenerate ring — nothing to triangulate
	}

	FDelaunay2 Delaunay;
	TArray<FVector2d> InputVertices;
	TArray<FIndex2i> Edges;
	InputVertices.Reserve(CleanedVertices.Num());
	Edges.Reserve(CleanedVertices.Num());
	for (int i = 0; i < CleanedVertices.Num(); ++i)
	{
		const auto& V = CleanedVertices[i];
		InputVertices.Add(FVector2d{ V });
		Edges.Add({ i, (i + 1) % CleanedVertices.Num() });
		DynamicMesh2d.AppendVertex(FVector(V.X, V.Y, 0.0));
		DynamicMesh3d.AppendVertex(V);

		MaxZ = FMath::Max(MaxZ, V.Z);
	}

	TArray<FIndex3i> Triangles;
	if (Delaunay.Triangulate(InputVertices, Edges))
	{
		// Constrained Delaunay: the boundary edges are in the triangulation, so the Solid flood-fill
		// keeps exactly the polygon interior (correct for concave polygons, not just convex ones).
		Triangles = Delaunay.GetFilledTriangles(Edges, FDelaunay2::EFillMode::Solid);
	}
	else
	{
		// Constrained triangulation failed (a self-intersecting / degenerate ring). Fall back to the
		// unconstrained triangulation and keep all triangles so we still get an approximate Z-lookup
		// surface instead of failing the whole road bake. Intersect() uses nearest-triangle, so the
		// extra convex-hull triangles are harmless for query points inside the polygon.
		if (!Delaunay.Triangulate(InputVertices))
		{
			return false; // truly degenerate (empty or all-collinear points)
		}
		Triangles = Delaunay.GetTriangles();
	}

	if (Triangles.Num() == 0)
	{
		return false;
	}

	for (const FIndex3i& Tri : Triangles)
	{
		DynamicMesh2d.AppendTriangle(Tri);
		DynamicMesh3d.AppendTriangle(Tri);
	}


	AABBTree2d.SetMesh(&DynamicMesh2d, true);

	return true;
}

bool FPolygonIntersectApproximation::Intersect(const FVector2D& Point, FVector& OutPoint, FVector& OutNormal) const
{
	using namespace UE::Geometry;

	const FVector Point3d{ Point.X, Point.Y, 0.0 };
	AABBTree2d.FindNearestPoint(Point3d);
	double NearestDistSqr;
	int32 NearTriID = AABBTree2d.FindNearestTriangle(Point3d, NearestDistSqr);
	if (NearTriID >= 0)
	{
		FTriangle3d Triangle;
		DynamicMesh3d.GetTriVertices(NearTriID, Triangle.V[0], Triangle.V[1], Triangle.V[2]);

		OutPoint = FMath::RayPlaneIntersection(
			FVector{ Point.X, Point.Y, MaxZ + 100.0 }, FVector{ 0.0, 0.0, -1.0 },
			FPlane{ Triangle.V[0], Triangle.V[1], Triangle.V[2] });

		OutNormal = Triangle.Normal();

		return true;
	}

	return false;
}

} // GeomUtils