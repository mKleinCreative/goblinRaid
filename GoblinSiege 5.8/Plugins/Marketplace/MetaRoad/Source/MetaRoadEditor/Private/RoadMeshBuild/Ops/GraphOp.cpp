/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "TriangulateRoadOp.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "RoadMeshBuild/RoadLanePolylineArrangement.h"
#include "MetaRoadModule.h"
#include "Algo/MaxElement.h"

#define LOCTEXT_NAMESPACE "FGraphOp"

using namespace MetaRoad;


static TArray<FRoadPosition> MakePolylineGraph(const FProceduralPolygon_RoadLane& Poly, double Alpha, double MaxSquareDistanceFromSpline, double MinSegmentLength)
{
	auto RoadPositionFunc = [&Poly, Alpha](double SOffset, ESplineCoordinateSpace::Type CoordinateSpace)
	{ 
		return Poly.GetRoadSpline().GetRoadPosition(Poly.GetSectionIndex(), Poly.GetLaneIndex(), Alpha, SOffset, CoordinateSpace);
	};

	double S0 = Poly.GetStartOffset();
	double S1 = Poly.GetEndOffset();

	TArray<FRoadPosition> Points;
	if (!Poly.GetRoadSpline().ConvertSplineToPolyline(RoadPositionFunc, ESplineCoordinateSpace::World, MaxSquareDistanceFromSpline, MinSegmentLength, S0, S1, {}, true, Points))
	{
		return {};
	}

	/*
	TArray<FVector2D> Points2D;
	Points2D.Reserve(Points.Num());
	for (auto& It : Points)
	{
		Points2D.Add(FVector2D{ It.Location });
	}
	OpUtils::RemovedPolylineSelfIntersection(Points2D);
	if (Points2D.Num() < 2)
	{
		return {};
	}
	*/

	TArray<FRoadPosition> OutPoints;
	TArray<FVector> Normals;

	OutPoints.Reserve(Points.Num());
	Normals.Reserve(Points.Num());

	for (int PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
	{
		//auto& Point2D = Points2D[PointIndex];
		auto& Point = Points[PointIndex];

		FHitResult Hit;
		if (!Poly.Owner.FindRayIntersection(FVector2D(Point.Location), Hit))
		{
			return {};
		}

		FRoadPosition Pos = Point;
		Pos.Location = Hit.ImpactPoint;
		OutPoints.Add(Pos);
		Normals.Add(Hit.Normal);
	}

	for (int Index = 0; Index < OutPoints.Num(); ++Index)
	{
		FVector ForwardVector;
		if (Index == 0)
		{
			auto& PtB = OutPoints[Index].Location;
			auto& PtC = OutPoints[Index + 1].Location;
			ForwardVector = (PtC - PtB).GetSafeNormal();

		}
		else if (Index == OutPoints.Num() - 1)
		{
			auto& PtA = OutPoints[Index - 1].Location;
			auto& PtB = OutPoints[Index].Location;
			ForwardVector = (PtB - PtA).GetSafeNormal();
		}
		else
		{
			auto& PtA = OutPoints[Index - 1].Location;
			auto& PtB = OutPoints[Index].Location;
			auto& PtC = OutPoints[Index + 1].Location;
			FVector ForwardVector0 = (PtB - PtA).GetSafeNormal();
			FVector ForwardVector1 = (PtC - PtB).GetSafeNormal();
			ForwardVector = (ForwardVector0 + ForwardVector1).GetSafeNormal();
		}

		OutPoints[Index].Quat = (FRotationMatrix::MakeFromXZ(ForwardVector, Normals[Index])).ToQuat();
	}

	const bool bIsReverse = ((Poly.GetLaneIndex() != MetaRoad::ZeroLaneIndex) ? !Poly.GetLane().IsForwardLane() : false);

	if (bIsReverse)
	{
		Algo::Reverse(OutPoints);
	}

	return OutPoints;
}

int FindContainsPoly(const TArray<FPolygon2d>& Polygones, const TArray<FRoadGrapPoint>& Points)
{
	if (Polygones.Num() == 0)
	{
		return -1;
	}

	TArray<int> Nums;
	Nums.SetNumZeroed(Polygones.Num());

	for (int i = 0; i < Polygones.Num(); ++i)
	{
		auto& Poly = Polygones[i];
		for (auto& Pt : Points)
		{
			if (Poly.Contains(FVector2D(Pt.Location)))
			{
				++Nums[i];
			}
		}
	}

	int MaxInd = 0;
	int MaxVal = Nums[0];
	for (int i = 1; i < Nums.Num(); ++i)
	{
		if (Nums[i] > MaxVal)
		{
			MaxVal = Nums[i];
			MaxInd = i;
		}
	}

	return MaxInd;
}



// ---------------------------------------------------------------------------------------------------------------------------------

void FGraphOp::CalculateResult(FProgressCancel* Progress)
{
	/*
#define CHECK_CANCLE() if (Progress && Progress->Cancelled()) { ResultInfo.Result = EGeometryResultType::Cancelled; return; }

	ResultInfo.Result = EGeometryResultType::InProgress;

#undef CHECK_CANCLE
*/

	if (!BaseData || BaseData->ResultInfo.HasFailed())
	{
		//ResultInfo.SetFailed();
		return;
	}

	Result->Bounds = FBoxSphereBounds(FBox(BaseData->Bounds)).TransformBy(BaseData->ActorTransform);

	TArray<FPolygon2d> Polygones;
	for (auto& SrcBoundary : BaseData->Boundaries)
	{
		auto& DstBoundary = Result->Boundaries.Add_GetRef({});
		for (auto& Ind : SrcBoundary)
		{
			DstBoundary.Points.Add(BaseData->Vertices3d[Ind.A].Vertex);
		}
		Polygones.Add(DstBoundary.Points);
	}

	TMap<const URoadSplineEditorComponent*, FRoadGraphSpline> MapResult;


	for (const auto& Poly : BaseData->Polygons)
	{
		if (Poly->GetType() != ERoadPolygonType::RoadLane)
		{
			continue;
		}

		const auto* LanePoly = static_cast<FProceduralPolygon_RoadLane*>(Poly.Get());
		if (LanePoly->GetLaneIndex() == 0)
		{
			continue;
		}

		auto* Spline = &LanePoly->GetRoadSpline();
		if (!MapResult.Find(Spline))
		{
			auto& SplineResult = MapResult.Add(Spline);
			SplineResult.Sections.SetNum(Spline->GetRoadLayout().Sections.Num());
			for (int SectionIndex = 0; SectionIndex < SplineResult.Sections.Num(); ++SectionIndex)
			{
				SplineResult.Sections[SectionIndex].LeftLanes.SetNum(Spline->GetRoadLayout().Sections[SectionIndex].Left.Num());
				SplineResult.Sections[SectionIndex].RightLanes.SetNum(Spline->GetRoadLayout().Sections[SectionIndex].Right.Num());
			}
		}

		auto& SplineResult = MapResult[Spline];
		SplineResult.RoadSpline = LanePoly->GetRoadSpline().GetOriginSpline();
		auto& SectionResult = SplineResult.Sections[LanePoly->GetSectionIndex()];
		auto& LaneResult = LanePoly->GetLaneIndex() > 0 ? SectionResult.RightLanes[LanePoly->GetLaneIndex() - 1] : SectionResult.LeftLanes[-LanePoly->GetLaneIndex() -1];

		if (auto* RoadZone = Poly->GetRoadZone().GetPtr<FRoadZone>())
		{
			LaneResult.Type = RoadZone->ZoneType;
		}
		else
		{
			LaneResult.Type = FRoadZoneType();
		}

		LaneResult.ZoneTags = LanePoly->GetLane().ZoneTags;
		LaneResult.bIsForward = LanePoly->GetLane().IsForwardLane();

		TArray<FRoadPosition> CenterLine = MakePolylineGraph(*LanePoly, 0.5, MaxSquareDistanceFromSpline, MinSegmentLength);
		LaneResult.Points.SetNum(CenterLine.Num());
		for (int i = 0; i < CenterLine.Num(); ++i)
		{
			LaneResult.Points[i].Location = CenterLine[i].Location + ZOffset * FVector::UpVector /*CenterLine[i].Quat.GetUpVector()*/ ;
			LaneResult.Points[i].Quat = CenterLine[i].Quat;
			LaneResult.Points[i].SOffset = CenterLine[i].SOffset;
			LaneResult.Points[i].Width = LanePoly->GetLane().Width.Eval(CenterLine[i].SOffset - LanePoly->GetStartOffset());
		}

		TArray<FRoadPosition> InnerBorder = MakePolylineGraph(*LanePoly, 0.0, MaxSquareDistanceFromSpline, MinSegmentLength);
		LaneResult.InnerBorder.SetNum(InnerBorder.Num());
		for (int i = 0; i < InnerBorder.Num(); ++i)
		{
			LaneResult.InnerBorder[i] = InnerBorder[i].Location + ZOffset * InnerBorder[i].Quat.GetUpVector();
		}

		TArray<FRoadPosition> OuterBorder = MakePolylineGraph(*LanePoly, 1.0, MaxSquareDistanceFromSpline, MinSegmentLength);
		LaneResult.OuterBorder.SetNum(OuterBorder.Num());
		for (int i = 0; i < OuterBorder.Num(); ++i)
		{
			LaneResult.OuterBorder[i] = OuterBorder[i].Location + ZOffset * OuterBorder[i].Quat.GetUpVector();
		}

		SectionResult.BoundaryIndex = FindContainsPoly(Polygones, LaneResult.Points);
		//UE_LOG(LogMetaRoad, Error, TEXT("*** %s:%i:%i = %i"), *Spline->GetName(), LanePoly->SectionIndex, LanePoly->LaneIndex, SectionResult.BoundaryIndex);

	}

	for (auto& [Spline, Data] : MapResult)
	{
		Result->Splines.Emplace(MoveTemp(Data));
	}


}

#undef LOCTEXT_NAMESPACE
