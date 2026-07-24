/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */


#include "Utils/IntersectionFounder.h"
#include "MetaRoadSettings.h"
#include "Polygon2.h"
//#include "Math/ConvexHull2d.h"

/*
static int GetMinLaneIndex(const URoadSplineComponent* Section)
{
	if (Section.Left.Num() > 0)
	{
		return  -Section.Left.Num();
	}
	else
	{
		return 0;
	}
}
*/

namespace MetaRoad
{


namespace
{

	FSplinePosition FillIntersections(const UE::Geometry::FIntrSegment2Segment2d& intr, const UE::Geometry::FSegment2d& seg, const FSplinePosition& PointA, const FSplinePosition& PointB)
	{
		double Alpha0 = intr.Parameter0 / seg.Length() + 0.5;
		double SOffset0 = FMath::Lerp(PointA.SOffset, PointB.SOffset, Alpha0);
		double Z0 = FMath::Lerp(PointA.Position.Z, PointB.Position.Z, Alpha0);
		return { FVector(intr.Point0.X, intr.Point0.Y, Z0), SOffset0 };
	}

	bool FindIntersections(const TArray<FSplinePosition>& PolyA, const TArray<FSplinePosition>& PolyB, TArray<FSplinePosition>& OutA, TArray<FSplinePosition>& OutB)
	{
		/*
		if (!Bounds().Intersects(OtherPoly.Bounds()))
		{
			return false;
		}
		*/

		bool bFoundIntersections = false;
		for (int i = 0; i < PolyA.Num() - 1; ++i)
		{
			UE::Geometry::FSegment2d seg(FVector2D{ PolyA[i].Position }, FVector2D{ PolyA[i + 1].Position });

			for (int j = 0; j < PolyB.Num() - 1; ++j)
			{
				UE::Geometry::FSegment2d oseg(FVector2D{ PolyB[j].Position }, FVector2D{ PolyB[j + 1].Position });

				// this computes test twice for intersections, but seg.intersects doesn't
				// create any new objects so it should be much faster for majority of segments (should profile!)
				if (seg.Intersects(oseg))
				{
					//@todo can we replace with something like seg.intersects?
					UE::Geometry::FIntrSegment2Segment2d intrA(seg, oseg);
					UE::Geometry::FIntrSegment2Segment2d intrB(oseg, seg);
					if (intrA.Find() && intrB.Find() && intrA.Quantity == 1 && intrB.Quantity == 1)
					{
						bFoundIntersections = true;

						OutA.Add(FillIntersections(intrA, seg, PolyA[i], PolyA[i + 1]));
						OutB.Add(FillIntersections(intrB, oseg, PolyB[j], PolyB[j + 1]));
					}
				}
			}
		}

		return bFoundIntersections;
	}

	TArray<FSplinePosition> BuildLinearApproximation(const URoadSplineComponent* Spline, int SectionIndex, int LaneIndex)
	{
		const int NumPointPerSegmaent = GetDefault<UMetaRoadSettings>()->NumPointPerSegmaent;
		const int NumPointPerSection = GetDefault<UMetaRoadSettings>()->NumPointPerSection;

		TArray<FSplinePositionLinearApproximation> Points;
		auto& Section = Spline->GetLaneSection(SectionIndex);
		Spline->BuildLinearApproximation(Points, [&](double S)
		{
			return Section.EvalLaneROffset(LaneIndex, S) + Spline->EvalROffset(S);
		}, Section.SOffset, Section.SOffsetEnd_Cashed, NumPointPerSegmaent, NumPointPerSection, ESplineCoordinateSpace::World);

		TArray<FSplinePosition> Ret;
		for (auto& Pt : Points)
		{
			Ret.Add({ Pt.Position, Spline->GetDistanceAlongSplineAtSplineInputKey(Pt.SplineParam)});
		}
		return Ret;
	}

}


bool FIntersectionFounder::Setup(TArray<TObjectPtr<const URoadSplineComponent>>& InputSplines)
{
	if (InputSplines.Num() != 2)
	{
		return false;
	}

	const int NumPointPerSegmaent = GetDefault<UMetaRoadSettings>()->NumPointPerSegmaent;
	const int NumPointPerSection = GetDefault<UMetaRoadSettings>()->NumPointPerSection;

	SplinesData.Reset();
	
	// Fill PolyLeft and PolyRight
	for (auto Spline: InputSplines)
	{
		auto& SplineData = SplinesData.Add_GetRef({});
		SplineData.Spline = Spline;

		for (int SectionIndex = 0; SectionIndex < Spline->GetLaneSections().Num(); ++SectionIndex)
		{
			auto& Section = Spline->GetLaneSection(SectionIndex);
			int LeftLaneIndex  = Section.Left.Num()  > 0 ? -Section.Left.Num()  : 0;
			int RightLaneIndex = Section.Right.Num() > 0 ?  Section.Right.Num() : 0;

			if (LeftLaneIndex == 0 && RightLaneIndex == 0)
			{
				continue;
			}

			SplineData.PolyLeft = BuildLinearApproximation(Spline, SectionIndex, LeftLaneIndex);
			SplineData.PolyRight = BuildLinearApproximation(Spline, SectionIndex, RightLaneIndex);
		}
	}

	// Fill LeftIntersections and RightIntersections
	TArray<FSplinePosition> OutInterA, OutInterB;

	auto AddIntersections = [this, &OutInterA, &OutInterB](TArray<FIntersection>& SplineDataA, TArray<FIntersection>& SplineDataB)
	{
			check(OutInterA.Num() == OutInterB.Num());
			for (int i = 0; i < OutInterA.Num(); ++i)
			{
				auto& A = OutInterA[i];
				auto& B = OutInterB[i];
				Intersections.Add(A.Position);
				SplineDataA.Emplace(Intersections.Num() - 1, A.SOffset);
				SplineDataB.Emplace(Intersections.Num() - 1, B.SOffset);
			}
	};

	for (int i = 0; i < SplinesData.Num(); ++i)
	{
		auto& SplineDataA = SplinesData[i];
		for (int j = i + 1; j < SplinesData.Num(); ++j)
		{
			auto& SplineDataB = SplinesData[j];

			OutInterA.SetNum(0, EAllowShrinking::No);
			OutInterB.SetNum(0, EAllowShrinking::No);
			FindIntersections(SplineDataA.PolyLeft, SplineDataB.PolyLeft, OutInterA, OutInterB);
			AddIntersections(SplineDataA.LeftIntersections, SplineDataB.LeftIntersections);
			//SplineDataA.LeftIntersections.Append(OutInterA);
			//SplineDataB.LeftIntersections.Append(OutInterB);

			OutInterA.SetNum(0, EAllowShrinking::No);
			OutInterB.SetNum(0, EAllowShrinking::No);
			FindIntersections(SplineDataA.PolyLeft, SplineDataB.PolyRight, OutInterA, OutInterB);
			AddIntersections(SplineDataA.LeftIntersections, SplineDataB.RightIntersections);
			//SplineDataA.LeftIntersections.Append(OutInterA);
			//SplineDataB.RightIntersections.Append(OutInterB);

			OutInterA.SetNum(0, EAllowShrinking::No);
			OutInterB.SetNum(0, EAllowShrinking::No);
			FindIntersections(SplineDataA.PolyRight, SplineDataB.PolyLeft, OutInterA, OutInterB);
			AddIntersections(SplineDataA.RightIntersections, SplineDataB.LeftIntersections);
			//SplineDataA.RightIntersections.Append(OutInterA);
			//SplineDataB.LeftIntersections.Append(OutInterB);

			OutInterA.SetNum(0, EAllowShrinking::No);
			OutInterB.SetNum(0, EAllowShrinking::No);
			FindIntersections(SplineDataA.PolyRight, SplineDataB.PolyRight, OutInterA, OutInterB);
			AddIntersections(SplineDataA.RightIntersections, SplineDataB.RightIntersections);
			//SplineDataA.RightIntersections.Append(OutInterA);
			//SplineDataB.RightIntersections.Append(OutInterB);
		}

	}

	/*
	TArray<FVector2D> Vertices;
	for (auto& SplineData : SplinesData)
	{
		Vertices.Append(SplineData.LeftIntersections);
		Vertices.Append(SplineData.RightIntersections);
	}
	TArray< int32 > ConvexHullIndices;
	ConvexHull2D::ComputeConvexHull(Vertices, ConvexHullIndices);
	*/

	for (auto& SplineData : SplinesData)
	{	
		if (SplineData.LeftIntersections.Num() == 1 && SplineData.RightIntersections.Num() == 1)
		{
			if (SplineData.LeftIntersections[0].SOffset / SplineData.Spline->GetSplineLength() < 0.5)
			{
				SplineData.NodeBegin = MakeShared<FNode>();
				SplineData.NodeBegin->SOffset = FMath::Max(SplineData.LeftIntersections[0].SOffset, SplineData.RightIntersections[0].SOffset);
				SplineData.NodeBegin->Dir = ENodeDir::Backward;
				SplineData.NodeBegin->LeftIntersectionIndex = SplineData.LeftIntersections[0].Index;
				SplineData.NodeBegin->RightIntersectionIndex = SplineData.RightIntersections[0].Index;
			}
			else
			{
				SplineData.NodeEnd = MakeShared<FNode>();
				SplineData.NodeEnd->SOffset = FMath::Min(SplineData.LeftIntersections[0].SOffset, SplineData.RightIntersections[0].SOffset);
				SplineData.NodeEnd->Dir = ENodeDir::Forward;
				SplineData.NodeEnd->LeftIntersectionIndex = SplineData.LeftIntersections[0].Index;
				SplineData.NodeEnd->RightIntersectionIndex = SplineData.RightIntersections[0].Index;
			}
		}
		else if (SplineData.LeftIntersections.Num() == 2 && SplineData.RightIntersections.Num() == 2)
		{
			double LeftMin = FMath::Min(SplineData.LeftIntersections[0].SOffset, SplineData.LeftIntersections.Last().SOffset);
			double LeftMax = FMath::Max(SplineData.LeftIntersections[0].SOffset, SplineData.LeftIntersections.Last().SOffset);
			double RightMin = FMath::Min(SplineData.RightIntersections[0].SOffset, SplineData.RightIntersections.Last().SOffset);
			double RightMax = FMath::Max(SplineData.RightIntersections[0].SOffset, SplineData.RightIntersections.Last().SOffset);
			
			SplineData.NodeBegin = MakeShared<FNode>();
			SplineData.NodeBegin->SOffset = FMath::Min(LeftMin, RightMin);
			SplineData.NodeBegin->Dir = ENodeDir::Forward;

			SplineData.NodeEnd = MakeShared<FNode>();
			SplineData.NodeEnd->SOffset = FMath::Max(LeftMax, RightMax);
			SplineData.NodeEnd->Dir = ENodeDir::Backward;

			// TODO:
			//SplineData.NodeBegin->LeftIntersectionIndex = ;
			//SplineData.NodeBegin->RightIntersectionIndex = ;
			//SplineData.NodeEnd->LeftIntersectionIndex = ;
			//SplineData.NodeEnd->RightIntersectionIndex = ;
		}
		else if (SplineData.LeftIntersections.Num() == 2)
		{
			SplineData.NodeBegin = MakeShared<FNode>();
			SplineData.NodeBegin->SOffset = FMath::Min(SplineData.LeftIntersections[0].SOffset, SplineData.LeftIntersections[1].SOffset);
			SplineData.NodeBegin->Dir = ENodeDir::Forward;

			SplineData.NodeEnd = MakeShared<FNode>();
			SplineData.NodeEnd->SOffset = FMath::Max(SplineData.LeftIntersections[0].SOffset, SplineData.LeftIntersections[1].SOffset);
			SplineData.NodeEnd->Dir = ENodeDir::Backward;

			// TODO:
			//SplineData.NodeBegin->LeftIntersectionIndex = ;
			//SplineData.NodeBegin->RightIntersectionIndex = ;
			//SplineData.NodeEnd->LeftIntersectionIndex = ;
			//SplineData.NodeEnd->RightIntersectionIndex = ;
		}
		else if (SplineData.RightIntersections.Num() == 2)
		{
			SplineData.NodeBegin = MakeShared<FNode>();
			SplineData.NodeBegin->SOffset = FMath::Min(SplineData.RightIntersections[0].SOffset, SplineData.RightIntersections[1].SOffset);
			SplineData.NodeBegin->Dir = ENodeDir::Forward;

			SplineData.NodeEnd = MakeShared<FNode>();
			SplineData.NodeEnd->SOffset = FMath::Max(SplineData.RightIntersections[0].SOffset, SplineData.RightIntersections[1].SOffset);
			SplineData.NodeEnd->Dir = ENodeDir::Backward;

			// TODO:
			//SplineData.NodeBegin->LeftIntersectionIndex = ;
			//SplineData.NodeBegin->RightIntersectionIndex = ;
			//SplineData.NodeEnd->LeftIntersectionIndex = ;
			//SplineData.NodeEnd->RightIntersectionIndex = ;
		}
		else
		{
			return false;
		}
	}

	// TODO: fill FNode::ToLeft, FNode::ToRight
	for (auto& SplineData : SplinesData)
	{
		if (SplineData.NodeBegin)
		{

		}
	}



	return true;
}

bool FIntersectionFounder::Solve()
{

	return false;
}

bool FIntersectionFounder::SolveTIntersection()
{
	return false;
}

bool FIntersectionFounder::SolveXIntersection()
{

	return false;
}

} // namespace MetaRoad