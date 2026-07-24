/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "TriangulateRoadOp.h"
#include "DynamicMesh/MeshNormals.h"
#include "SmoothingOps/CotanSmoothingOp.h"
#include "MetaRoadModule.h"
#include "MetaRoadActor.h" // AMetaRoad (SetActorWithRoads param)
#include "RoadMeshBuild/RoadSplineEditorComponent.h"

//#include UE_INLINE_GENERATED_CPP_BY_NAME(TriangulateRoadOp)

#define LOCTEXT_NAMESPACE "RoadBaseOperator"

using namespace MetaRoad;

static bool MakeDelaunay2(const MetaRoad::FDynamicGraph2d Graph, FDelaunay2& Delaunay, TArray<int32>* SkippedEdges)
{

	// A flat array of the vertices, copied out of the graph
	TArray<FVector2d> InputVertices;

	check(Graph.MaxVertexID() == Graph.VertexCount());

	TArray<int> InputIndices, OutputIndices;

	for (int i = 0; i < Graph.MaxVertexID(); i++)
	{
		FVector2d Vertex = FVector2d(Graph.GetVertex(i));
		InputVertices.Add(Vertex);
	}

	Delaunay.bAutomaticallyFixEdgesToDuplicateVertices = false; // Arrangement will remove duplicates already

	if (!Delaunay.Triangulate(InputVertices))
	{
		return false;
	}

	Delaunay.bValidateEdges = false;
	Delaunay.bKeepFastEdgeAdjacencyData = true;

	//bool bInsertConstraintFailure = false;

	TArray<FIndex2i> AllEdges;

	for (int EdgeIdx : Graph.EdgeIndices())
	{
		auto& Edge = Graph.GetEdgeRef(EdgeIdx);
		AllEdges.Emplace(Edge.A, Edge.B);
	}

	Delaunay.ConstrainEdges(InputVertices, AllEdges);

	// Verify all edges after all constraints are in -- to ensure that inserted edges were also not removed by subsequent edge insertion
	for (int EdgeIdx : Graph.EdgeIndices())
	{
		auto& Edge = Graph.GetEdgeRef(EdgeIdx);
		if (!Delaunay.HasEdge(FIndex2i(Edge.A, Edge.B), false))
		{
			//bInsertConstraintFailure = true;
			if (SkippedEdges)
			{
				SkippedEdges->Add(EdgeIdx);
			}
		}
	}

	//return !bInsertConstraintFailure;
	return true;

}

static bool IsSameTri(const FIndex3i& A, const FIndex3i& B)
{
	return A.Contains(B.A) && A.Contains(B.B) && A.Contains(B.C);
}

static FGeometryWarning ErrorToWarning(const FGeometryError& Error)
{
	FGeometryWarning Ret;
	Ret.WarningCode = Error.ErrorCode;
	Ret.Message = Error.Message;
	Ret.Timestamp = Error.Timestamp;
	Ret.CustomData = Error.CustomData;
	return Ret;
}

static TArray<FGeometryWarning> ErrorToWarning(const TArray<FGeometryError>& Errors)
{
	TArray<FGeometryWarning> Ret;
	Ret.Reserve(Errors.Num());
	for (auto& It : Errors)
	{
		Ret.Add(ErrorToWarning(It));
	}
	return Ret;
}


//------------------------------------------------------------------------------------------------------------------------------------------------------

void FRoadTriangulationData::AddDebugLines(const TArray<FIndex2i>& InBoundaries, const FColor& Color, float Thickness)
{
	FDebugLineBuffer::FBatch Batch;
	Batch.Color = Color;
	Batch.Thickness = Thickness;

	for (auto& Ind : InBoundaries)
	{
		Batch.Lines.Add({ Vertices3d[Ind.A].Vertex, Vertices3d[Ind.B].Vertex });
	}

	DebugDraw.Add(MoveTemp(Batch));
}

void FRoadTriangulationData::AddDebugLines(int GID, const FColor& Color, float Thickness)
{
	FDebugLineBuffer::FBatch Batch;
	Batch.Color = Color;
	Batch.Thickness = Thickness;

	auto& Graph = Arrangement->Graph;

	for (int EID : Graph.EdgeIndices())
	{
		auto& Edge = Graph.GetEdgeRef(EID);

		if (GID == -1 || Graph.GetEdgeGroup(EID) == GID )
		{
			FVector2d A = Graph.GetVertex(Edge.A);
			FVector2d B = Graph.GetVertex(Edge.B);

			Batch.Lines.Add({ FVector{A.X, A.Y, 50}, FVector{B.X, B.Y, 50} });
		}
	}

	DebugDraw.Add(MoveTemp(Batch));
}

bool FRoadTriangulationData::IsBoundaryVertex(int VID) const
{
	for (auto& Boundary : Boundaries)
	{
		if (Boundary.FindByPredicate([VID](const FIndex2i& Index) { return Index.A == VID || Index.B == VID; }))
		{
			return true;
		}
	}
	return false;
}

void FSpatialQuery::Build(const TArray<FArrangementVertex3d>& Vertices3d, const TArray<FIndex3i>& Triangles)
{
	// Idempotent: clear any previous geometry so Build() can be called more than once on the same query
	// (the Snap to Ground pass re-finalizes the geometry on the game thread after overriding Z).
	Mesh3d.Clear();
	Mesh2d.Clear();
	for (const auto& Vertex3d : Vertices3d)
	{
		const FVector& Vertex = Vertex3d.Vertex;
		Mesh3d.AppendVertex(Vertex);
		Mesh2d.AppendVertex({ Vertex.X, Vertex.Y, 0.0 });
	}
	for (const FIndex3i& Triangle : Triangles)
	{
		Mesh3d.AppendTriangle(Triangle);
		Mesh2d.AppendTriangle(Triangle);
	}
	Tree3d.SetMesh(&Mesh3d, true);
	Tree2d.SetMesh(&Mesh2d, true);
}

bool FSpatialQuery::FindRayIntersection(const FVector2D& Point, double TopZ, FHitResult& HitOut) const
{
	const FVector Point3d{ Point.X, Point.Y, 0.0 };
	Tree2d.FindNearestPoint(Point3d);
	double NearestDistSqr;
	int32 NearTriID = Tree2d.FindNearestTriangle(Point3d, NearestDistSqr);
	if (NearTriID >= 0)
	{
		FTriangle3d Triangle;
		Mesh3d.GetTriVertices(NearTriID, Triangle.V[0], Triangle.V[1], Triangle.V[2]);

		HitOut.FaceIndex = NearTriID;
		HitOut.Normal = Triangle.Normal();
		HitOut.ImpactPoint = FMath::RayPlaneIntersection(FVector{ Point.X, Point.Y, TopZ }, FVector{ 0.0, 0.0, -1.0 }, FPlane{ Triangle.V[0], Triangle.V[1], Triangle.V[2] });
		return true;
	}

	return false;
}

bool FRoadTriangulationData::FindRayIntersection(const FVector2D& Point, FHitResult& HitOut) const
{
	return Spatial.FindRayIntersection(Point, Bounds.Max.Z, HitOut);
}

void FRoadTriangulationData::FinalizeVertexGeometry(bool bCotanSmoothZ, float SmoothSpeed, float Smoothness,
                                                    bool bRebuildSpatial, FProgressCancel* Progress)
{
#define CHECK_CANCLE() if (Progress && Progress->Cancelled()) { ResultInfo.Result = UE::Geometry::EGeometryResultType::Cancelled; return; }

	const int32 NumVerts = Vertices3d.Num();

	// ========================== Create DynamicMesh and Compute normals ==========================
	FDynamicMesh3 DynamicMesh(true, false, false, false);
	for (int VID = 0; VID < NumVerts; ++VID)
	{
		int32 NewVID = DynamicMesh.AppendVertex(Vertices3d[VID].Vertex);
		check(NewVID == VID);
	}
	for (int TID = 0; TID < Triangles.Num(); ++TID)
	{
		DynamicMesh.InsertTriangle(TID, Triangles[TID]);
	}
	FMeshNormals::QuickComputeVertexNormals(DynamicMesh);
	CHECK_CANCLE();

	// ========================== CotanSmoothingOp ==========================
	if (bCotanSmoothZ)
	{
		FSmoothingOpBase::FOptions SmoothingOptions;
		SmoothingOptions.SmoothAlpha = SmoothSpeed;
		SmoothingOptions.BoundarySmoothAlpha = 0.0;
		double NonlinearT = FMathd::Pow(Smoothness, 2.0);
		// this is an empirically-determined hack that seems to work OK to normalize the smoothing result for variable vertex count...
		double ScaledPower = (NonlinearT / 50.0) * NumVerts;
		SmoothingOptions.SmoothPower = ScaledPower;
		SmoothingOptions.bUniform = false;
		SmoothingOptions.bUseImplicit = true;
		SmoothingOptions.NormalOffset = 0.0f;

		FCotanSmoothingOp SmoothingOp(&DynamicMesh, SmoothingOptions);
		SmoothingOp.CalculateResult(Progress);
		CHECK_CANCLE();

		auto SmoothedMesh = SmoothingOp.ExtractResult();
		if (SmoothedMesh && SmoothedMesh->VertexCount() == DynamicMesh.VertexCount())
		{
			for (int VID = 0; VID < NumVerts; ++VID)
			{
				Vertices3d[VID].Vertex.Z = SmoothedMesh->GetVertexRef(VID).Z;
			}

			FMeshNormals::QuickComputeVertexNormals(DynamicMesh);
			CHECK_CANCLE();
		}
		else
		{
			ResultInfo.AddWarning({ 0, LOCTEXT("CalculateResultFail_Smoothing", "FRoadTriangulationData: Can't smooth mesh") });
		}
	}

	// ========================== Set vertex normals ==========================
	for (int VID = 0; VID < NumVerts; ++VID)
	{
		auto Normal = DynamicMesh.GetVertexNormal(VID);
		Vertices3d[VID].Normal = FVector{ Normal.X, Normal.Y, Normal.Z };
	}
	CHECK_CANCLE();

	// ========================== AABBTree ==========================
	if (bRebuildSpatial)
	{
		Spatial.Build(Vertices3d, Triangles);
		CHECK_CANCLE();
	}

#undef CHECK_CANCLE
}

FRoadTriangulationData::~FRoadTriangulationData()
{
	FRoadSplineEditorComponentPool::Get().Release(MoveTemp(Splines));
}

FRoadTriangulationOp::~FRoadTriangulationOp()
{
}

void FRoadTriangulationOp::SetActorWithRoads(const AMetaRoad* Actor,
                                          const TArray<TWeakObjectPtr<URoadSplineComponent>>& SplineFilter)
{
	Result = MakeUnique<FRoadTriangulationData>();

	// The target actor may have been destroyed between scheduling and this background run — fail gracefully.
	if (!Actor)
	{
		Result->ResultInfo = { UE::Geometry::EGeometryResultType::Failure };
		return;
	}

	Result->ActorTransform = Actor->GetTransform();

	TArray<const URoadSplineComponent*> Splines;
	Actor->GetComponents(Splines);

	if (!SplineFilter.IsEmpty())
	{
		Splines = Splines.FilterByPredicate([&](const URoadSplineComponent* S)
		{
			return SplineFilter.ContainsByPredicate([S](const TWeakObjectPtr<URoadSplineComponent>& W)
			{
				return W.Get() == S;
			});
		});
	}

	Result->Splines.Reserve(Splines.Num());
	for (auto& Spline : Splines)
		Result->Splines.Emplace(FRoadSplineEditorComponentPool::Get().Acquire(Spline));

	TArray<UActorComponent*> RoadPolygones = Actor->GetComponentsByInterface(URoadPolygonInterfcae::StaticClass());;
	for (auto& It : RoadPolygones)
	{
		if (const auto* Polygon = Cast<IRoadPolygonInterfcae>(It))
		{
			TArray<FRoadPolygonData> Polygones;
			Polygon->GeneratePolygones(Polygones);
			Result->SimplePolygones.Append(MoveTemp(Polygones));
		}
	}

}

void FRoadTriangulationOp::CalculateResult(FProgressCancel* Progress)
{
	//SCOPE_LOG_TIME_IN_SECONDS(TEXT("    Full time"), nullptr);

#define CHECK_CANCLE() if (Progress && Progress->Cancelled()) { Result->ResultInfo.Result = EGeometryResultType::Cancelled; return; }

	Result->ResultInfo.Result = EGeometryResultType::InProgress;
	Result->Params = Params;
	Result->Bounds = {};
	for (auto& Spline : Result->Splines)
	{
		auto Bound = Spline->CalcBounds(FTransform::Identity);
		Result->Bounds.Contain( -Bound.BoxExtent );
		Result->Bounds.Contain( Bound.BoxExtent );
	}
	Result->Arrangement = MakeUnique<MetaRoad::FArrangement2d>(FAxisAlignedBox2d{ FVector2d{Result->Bounds.Min}, FVector2d{Result->Bounds.Max} });
	Result->Arrangement->VertexSnapTol = VertexSnapTol;

	const auto& Graph = Result->Arrangement->Graph;

	CHECK_CANCLE();

	// ========================== Prepare SplinesCurves2d ==========================
	for (auto& Spline : Result->Splines)
	{
		Spline->UpdateSplinesCurves2d();
	}

	// ========================== Add FProceduralPolygon_RoadLane ==========================
	{
		//SCOPE_LOG_TIME_IN_SECONDS(TEXT("Make FProceduralPolygon_RoadLane"), nullptr);
		for (int SplineIndex = 0; SplineIndex < Result->Splines.Num(); ++SplineIndex)
		{
			auto& Spline = *Result->Splines[SplineIndex];
			if (Spline.bSkipProceduralGeneration)
			{
				continue;
			}

			for (int SectionIndex = 0; SectionIndex < Spline.RoadLayout.Sections.Num(); ++SectionIndex)
			{
				auto& Section = Spline.RoadLayout.Sections[SectionIndex];
				for (int LaneIndex = -Section.Left.Num(); LaneIndex <= Section.Right.Num(); ++LaneIndex)
				{
					if (LaneIndex != MetaRoad::ZeroLaneIndex && Section.GetLaneByIndex(LaneIndex).bSkipProceduralGeneration)
					{
						continue;
					}

					auto Poly = MakeUnique<FProceduralPolygon_RoadLane>(*Result, SplineIndex, SectionIndex, LaneIndex, Params.ChordToleranceSq, SidewalkCapToleranceSq, Params.MinSegmentLength);
					if (!Poly->GetResult().HasResult())
					{
						Result->ResultInfo.Warnings.Append(Poly->GetResult().Warnings);
						Result->ResultInfo.Warnings.Append(ErrorToWarning(Poly->GetResult().Errors));
						Result->ResultInfo.SetFailed(LOCTEXT("CalculateResultFail_LanePoly", "FRoadTriangulationData: Can't make the lane polygone"));
						return;
					}
					Result->Polygons.Add(MoveTemp(Poly));
					CHECK_CANCLE();
				}
			}
		}
	}

	// ========================== Add FProceduralPolygon_RoadLoop ==========================
	{
		//SCOPE_LOG_TIME_IN_SECONDS(TEXT("Make RoadPolygoneLoop"), nullptr);
		for (int SplineIndex = 0; SplineIndex < Result->Splines.Num(); ++SplineIndex)
		{
			auto& Spline = *Result->Splines[SplineIndex];
			if (Spline.bSkipProceduralGeneration)
			{
				continue;
			}

			if (Spline.IsClosedLoop() && Spline.RoadLayout.LoopedRoadZone.IsValid())
			{
				auto Poly = MakeUnique<FProceduralPolygon_RoadLoop>(*Result, SplineIndex, Params.ChordToleranceSq, Params.MinSegmentLength);
				if (!Poly->GetResult().HasResult())
				{
					Result->ResultInfo.Warnings.Append(Poly->GetResult().Warnings);
					Result->ResultInfo.Warnings.Append(ErrorToWarning(Poly->GetResult().Errors));
					Result->ResultInfo.SetFailed(LOCTEXT("CalculateResultFail_SimplePoly", "FRoadTriangulationData: Can't make the simple polygone"));
					return;
				}
				Result->Polygons.Add(MoveTemp(Poly));
				CHECK_CANCLE();
			}
		}
	}

	// ========================== Complete FProceduralPolygon_Simple arrangement  ==========================
	for (int i = 0; i < Result->SimplePolygones.Num(); ++i)
	{
		auto& It = Result->SimplePolygones[i];
		Result->Polygons.Add(MakeUnique<FProceduralPolygon_Simple>(*Result, i));
		CHECK_CANCLE();
	}

	/*
	if (true)
	{
		Result->AddDebugLines(-1, FColor(255, 255, 0, 50), 4.0);
		Result->ResultInfo.SetFailed(LOCTEXT("CalculateResultFail_Debug", "DebugStop"));
		return;
	}
	*/

	// ========================== Complete FProceduralPolygon arrangement  ==========================
	{

		Result->Vertices3d.SetNum(Graph.MaxVertexID());

		for (auto& Poly : Result->Polygons)
		{
			bool bSucces = Poly->OnCompleteArrangement();
			Result->ResultInfo.Warnings.Append(Poly->GetResult().Warnings);
			Result->ResultInfo.Warnings.Append(ErrorToWarning(Poly->GetResult().Errors));
		}

		/*
		int Layer = 0;
		for (auto& Poly : Result->Polygons)
		{
			//auto& DebugLine = Result->DebugLines.Add_GetRef({});
			//DebugLine.Color = FColor(0, 255, 0, 50);
			//DebugLine.Thickness = 4;
			//for (int i = 0; i < Poly.Poly2d.VertexCount(); ++i)
			//{
			//	auto& PtA = Poly.Poly2d[i];
			//	auto& PtB = Poly.Poly2d[(i + 1) % Poly.Poly2d.VertexCount()];
			//	DebugLine.Lines.Add({ FVector{PtA.X, PtA.Y, 200.0 + Layer * 100.0},  FVector{PtB.X, PtB.Y, 200.0 + Layer * 100.0} });
			//}
			//++Layer;
			

			auto& DebugLine = Result->DebugLines.Add_GetRef({});
			DebugLine.Color = FColor(0, 255, 0, 50);
			DebugLine.Thickness = 4;
			for (auto& It : Poly->Boundary)
			{
				auto PtA = Graph.GetVertex(It.A);
				auto PtB = Graph.GetVertex(It.B);
				DebugLine.Lines.Add({ FVector{PtA.X, PtA.Y, 200.0 + Layer * 100.0},  FVector{PtB.X, PtB.Y, 200.0 + Layer * 100.0} });
			}
			++Layer;
		}
		*/
	}

	CHECK_CANCLE();

	// ========================== Find boundaries ==========================
	{
		//SCOPE_LOG_TIME_IN_SECONDS(TEXT("Find boundaries"), nullptr);
		int FoundBoundariesNum = OpUtils::FindBoundaries(Graph, {}, Result->Boundaries, [](int GID) { return GID != GUIFlags::CenterLine; });
		//UE_LOG(LogMetaRoad, Log, TEXT("%32s - %6i"), TEXT("Boundaries num"), FoundBoundariesNum);
	}
	if (!Result->Boundaries.Num())
	{
		Result->ResultInfo.SetFailed(LOCTEXT("CalculateResultFail_Boundaries", "FRoadTriangulationData: Can't find boundaries"));
		return;
	}

	CHECK_CANCLE();

	// ========================== Triangulate ==========================
	FDelaunay2 Delaunay;
	{
		TArray<int32> SkippedEdges;
		if (!MakeDelaunay2(Result->Arrangement->Graph, Delaunay, &SkippedEdges))
		{
			Result->ResultInfo.SetFailed(LOCTEXT("CalculateResultFail_Triangulate", "FRoadTriangulationData: Can't triangulate"));
			return;
		}
	}

	CHECK_CANCLE();

	// ========================== Get all triangles ==========================

	Result->Triangles = Delaunay.GetFilledTriangles(OpUtils::MergeBoundaries(Result->Boundaries), FDelaunay2::EFillMode::NonZeroWinding);
	if (Result->Triangles.Num() == 0)
	{
		Result->ResultInfo.SetFailed(LOCTEXT("CalculateResultFail_NoTriangles", "FRoadTriangulationData: No triangles"));
		return;
	}

	for (FIndex3i& T : Result->Triangles)
	{
		T = FIndex3i(T.C, T.B, T.A);
	}

	CHECK_CANCLE();


	// ========================== Find FProceduralPolygon inner vertices  ==========================

	for (auto& Poly : Result->Polygons)
	{
		if (!Poly->IsPolyline())
		{
			TArray<FIndex3i> Triangles = Delaunay.GetFilledTriangles(OpUtils::MergeBoundaries({ Poly->Boundary }, Poly->Holes), FDelaunay2::EFillMode::NonZeroWinding);
			if (Triangles.Num() == 0)
			{
				Result->ResultInfo.AddWarning({ 0, FText::Format(LOCTEXT("CalculateResultFail_PolyTry", "FRoadTriangulationData: Can't get filled triangles for {0}"), Poly->GetDescription()) });
				continue;
			}
			for (const FIndex3i& T : Triangles)
			{
				int TID = Result->Triangles.Find(FIndex3i(T.C, T.B, T.A));
				if (TID != INDEX_NONE)
				{
					Poly->TrianglesIDs.Add(TID);
				}

				Poly->AddVertexInfo(T.A, ERoadVertexInfoFlags::OverlapPoly);
				Poly->AddVertexInfo(T.B, ERoadVertexInfoFlags::OverlapPoly);
				Poly->AddVertexInfo(T.C, ERoadVertexInfoFlags::OverlapPoly);
			}
			CHECK_CANCLE();
		}
	}

	CHECK_CANCLE();

	// ========================== Compute height ==========================
	// Set max or min z value for all Vertices3d
	for (int VID = 0; VID < Result->Vertices3d.Num(); ++VID)
	{
		auto& Verticex3d = Result->Vertices3d[VID];
		if (ensure(Verticex3d.Infos.Num()))
		{
			//  Here we are only interested in the X, Y coordinates. Z needs to be calculated below.
			Verticex3d.Vertex = Verticex3d.Infos.begin()->Value.Pos.Location;

			double TargetZ_WithoutDecales = Verticex3d.Vertex.Z;
			double TargetZ_WithDecales = 0;

			bool bZWasSet = false;

			for (auto& [Poly, Info] : Verticex3d.Infos)
			{
				const FRoadZoneTypeDetails* Details = Poly->GetRoadZone().IsValid() ? Poly->GetRoadZone().Get<FRoadZone>().ZoneType.GetDetails() : nullptr;
				const bool bIsDecal = Details ? Details->bIsDecal : true;

				// SnapToGround uses the Max path here only as a fallback (overridden per-vertex by the
				// game-thread line-trace pass; this value survives only where the trace misses).
				TargetZ_WithoutDecales = OverlapStrategy != ERoadOverlapStrategy::UseMinZ
					? FMath::Max(TargetZ_WithoutDecales, Info.Pos.Location.Z)
					: FMath::Min(TargetZ_WithoutDecales, Info.Pos.Location.Z);

				if (!bIsDecal)
				{
					if (!bZWasSet)
					{
						TargetZ_WithDecales = Info.Pos.Location.Z;
					}
					else
					{
						TargetZ_WithDecales = OverlapStrategy != ERoadOverlapStrategy::UseMinZ
							? FMath::Max(TargetZ_WithDecales, Info.Pos.Location.Z)
							: FMath::Min(TargetZ_WithDecales, Info.Pos.Location.Z);
					}
					bZWasSet = true;
				}
			}

			Verticex3d.Vertex.Z = bZWasSet ? TargetZ_WithDecales : TargetZ_WithoutDecales;
		}
		else
		{
			Result->ResultInfo.SetFailed({LOCTEXT("CalculateResultFail_MeshBroken", "FRoadTriangulationData: Mesh is broken") });
			return;
		}
	}

	CHECK_CANCLE();

	// ========================== Smooth z by kernal OverlapRadius ==========================
	// Skipped for SnapToGround: per-vertex Z is replaced by the line-trace pass, so cross-spline
	// overlap blending here would be wasted work.
	if (OverlapStrategy != ERoadOverlapStrategy::SnapToGround && OverlapRadius > KINDA_SMALL_NUMBER)
	{
		for (int VID = 0; VID < Result->Vertices3d.Num(); ++VID)
		{
			auto& Verticex3d = Result->Vertices3d[VID];
			auto DistanceSqFunc = [VID_A = VID, &Graph](int VID_B)
			{
				return DistanceSquared(Graph.GetVertex(VID_A), Graph.GetVertex(VID_B));
			};
			auto IgnoreFunc = [this, CurVID = VID](const int& VID)
			{
				if (CurVID == VID)
				{
					return true;
				}
				if (Result->IsBoundaryVertex(VID))
				{
					return true;
				}
				return false;
			};
			auto Points = Result->Arrangement->PointHash.FindAllInRadius(Graph.GetVertex(VID), OverlapRadius, DistanceSqFunc, IgnoreFunc);
			for (auto& [NearVID, DistSq] : Points)
			{
				double Alpha = FMath::Sqrt(DistSq) / OverlapRadius;
				double& Z = Result->Vertices3d[NearVID].Vertex.Z;
				if (OverlapStrategy == ERoadOverlapStrategy::UseMaxZ)
				{
					double MinZ = FMath::CubicInterp(Verticex3d.Vertex.Z, 0.0, Verticex3d.Vertex.Z - OverlapRadius, 0.0, Alpha);
					Z = FMath::Max(Z, MinZ);
				}
				else // OverlapStrategy == ERoadOverlapStrategy::UseMinZ
				{
					double MaxZ = FMath::CubicInterp(Verticex3d.Vertex.Z, 0.0, Verticex3d.Vertex.Z + OverlapRadius, 0.0, Alpha);
					Z = FMath::Min(Z, MaxZ);
				}
			}

			CHECK_CANCLE();
		}
	}
	
	CHECK_CANCLE();

	// ========================== Finalize geometry (normals + optional smoothing + AABB) ==========================
	// Shared with the game-thread Snap to Ground pass (FRoadComputePipeline::ApplyGroundSnap).
	Result->FinalizeVertexGeometry(Result->Splines.Num() > 1 && bSmooth, SmoothSpeed, Smoothness,
	                               /*bRebuildSpatial*/ true, Progress);
	CHECK_CANCLE();

	// ========================== Debug ==========================
	// 
	//Result->AddDebugLines((int)EArrangementGID::DrivingSurface, FColor(255, 255, 0, 128), 4.0);
	//Result->AddDebugLines((int)EArrangementGID::SidewalksSoft, FColor(255, 255, 0, 128), 4.0);
	//Result->AddDebugLines((int)EArrangementGID::SidewalksHard, FColor(255, 255, 0, 128), 4.0);

	if (bDrawBoundaries)
	{
		for (auto& It : Result->Boundaries)
		{
			Result->AddDebugLines(It, FColor::Blue, 4.0);
		}
	}


	Result->ResultInfo.SetSuccess();

#undef CHECK_CANCLE

}

#undef LOCTEXT_NAMESPACE
