/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "TriangulateRoadOp.h"
#include "RoadMeshBuild/RoadLanePolylineArrangement.h"
#include "Utils/OpUtils.h"
#include "MetaRoadEditorModule.h"
#include "Assets/RoadLaneAttributeGenerate.h"
#include "Utils/AssetUtils.h"
#include "Utils/GeomUtils.h"

#define LOCTEXT_NAMESPACE "FSplineMeshOp"

using namespace MetaRoad;

struct FRoadSplineMeshPosition
{
	FVector Location = FVector::ZeroVector;
	FQuat Quat = FQuat::Identity;
	bool bIsReverse = false;

	// Arc-length position on the road spline. -1 = not set.
	double SOffset = -1.0;

	// Key params
	bool bIsKey = false;
	// True when LeftWidth/RightWidth have been computed from the Width FRichCurve
	// at this vertex's SOffset. Prevents CompletePolyline from overwriting them.
	bool bWidthComputed = false;
	FVector2D Scale = FVector2D::One();
	FVector2D Offset = FVector2D::ZeroVector;
	double Roll = 0;
	float LeftWidth  = 0.f;
	float RightWidth = 0.f;
};

struct FRoadLanePolylineSplineMesh : public TRoadLanePolyline<FRoadSplineMeshPosition, FRoadLanePolylineSplineMesh>
{
	const URoadLaneAttributeGenerateDescriptor* SplineMeshProfile = nullptr;

	virtual bool CanAppend(const FRoadLanePolylineSplineMesh& Other, EAppandMode AppandMode, double Tolerance) const override
	{
		if (SplineMeshProfile != Other.SplineMeshProfile)
		{
			return false;
		}

		return TRoadLanePolyline<FRoadSplineMeshPosition, FRoadLanePolylineSplineMesh>::CanAppend(Other, AppandMode, Tolerance);
	}

	virtual void Reverse() override
	{
		for (auto& It : Vertices)
		{
			It.bIsReverse = !It.bIsReverse;
		}
		TRoadLanePolyline<FRoadSplineMeshPosition, FRoadLanePolylineSplineMesh>::Reverse();
	}
};

using FRoadArrangemenSplineMesh = TRoadLanePolylineArrangement<FRoadLanePolylineSplineMesh>;


static int32 UpperBound(const TArray<FInterpCurvePoint<FVector>>& SplinePoints, float Value)
{
	int32 Count = SplinePoints.Num();
	int32 First = 0;

	while (Count > 0)
	{
		const int32 Middle = Count / 2;
		if (Value >= SplinePoints[First + Middle].InVal)
		{
			First += Middle + 1;
			Count -= Middle + 1;
		}
		else
		{
			Count = Middle;
		}
	}

	return First;
}


static void AddPoint(FSplineCurves& SplineCurves, const FSplinePoint& SplinePoint)
{
	const int32 Index = UpperBound(SplineCurves.Position.Points, SplinePoint.InputKey);

	SplineCurves.Position.Points.Insert(FInterpCurvePoint<FVector>(
		SplinePoint.InputKey,
		SplinePoint.Position,
		SplinePoint.ArriveTangent,
		SplinePoint.LeaveTangent,
		ConvertSplinePointTypeToInterpCurveMode(SplinePoint.Type)
	), Index);

	SplineCurves.Rotation.Points.Insert(FInterpCurvePoint<FQuat>(
		SplinePoint.InputKey,
		SplinePoint.Rotation.Quaternion(),
		FQuat::Identity,
		FQuat::Identity,
		CIM_CurveAuto
	), Index);

	SplineCurves.Scale.Points.Insert(FInterpCurvePoint<FVector>(
		SplinePoint.InputKey,
		SplinePoint.Scale,
		FVector::ZeroVector,
		FVector::ZeroVector,
		CIM_CurveAuto
	), Index);
}


static TArray<FRoadSplineMeshPosition> MakePolylineSpline(const FProceduralPolygon_RoadLane& Poly, double S0, double S1, const FRoadLaneAttributeGenerateValue* KeyStart, const FRoadLaneAttributeGenerateValue* KeyEnd, double ChordToleranceSq, double MinSegmentLength, bool bIsReverse)
{
	auto RoadPositionFunc = [&Poly, KeyStart, KeyEnd, S0, S1](double SOffset, ESplineCoordinateSpace::Type CoordinateSpace) -> FRoadPosition
	{
		const int LaneIdx = Poly.GetLaneIndex();
		const ERoadLaneAlignment Mode = KeyStart->Alignment;  // stepped field

		if (LaneIdx == MetaRoad::ZeroLaneIndex)
		{
			// 4-arg GetRoadPosition ignores Alpha for ZeroLaneIndex — compute R explicitly
			const auto [LeftR, RightR] = Poly.GetSection().GetCenterLaneEdgeROffsets(SOffset);
			double AnchorR;
			if (Mode == ERoadLaneAlignment::Auto)
			{
				AnchorR = (LeftR + RightR) * 0.5;
			}
			else
			{
				const double A = !KeyEnd ? KeyStart->Alpha : FMath::CubicInterp(KeyStart->Alpha, 0.0, KeyEnd->Alpha, 0.0, (SOffset - S0) / (S1 - S0));
				AnchorR = MetaRoad::MapCenterLaneROffset(A, LeftR, RightR);
			}
			return Poly.GetRoadSpline().GetRoadPosition(SOffset, AnchorR + Poly.GetRoadSpline().EvalROffset(SOffset), CoordinateSpace);
		}
		else
		{
			// Non-center lane: Auto→lane center, Fixed→interpolate Alpha
			const float Alpha = (Mode == ERoadLaneAlignment::Auto)
				? 0.5f
				: (!KeyEnd ? (float)KeyStart->Alpha : FMath::CubicInterp((float)KeyStart->Alpha, 0.0f, (float)KeyEnd->Alpha, 0.0f, (float)((SOffset - S0) / (S1 - S0))));
			return Poly.GetRoadSpline().GetRoadPosition(Poly.GetSectionIndex(), LaneIdx, Alpha, SOffset, CoordinateSpace);
		}
	};

	TArray<FRoadPosition> Points;
	if (!Poly.GetRoadSpline().ConvertSplineToPolyline(RoadPositionFunc, ESplineCoordinateSpace::World, ChordToleranceSq, MinSegmentLength, S0, S1, {}, true, Points))
	{
		return {};
	}

	TArray<FVector2D> Points2D;
	Points2D.Reserve(Points.Num());
	for (auto& It : Points)
	{
		Points2D.Add(FVector2D{ It.Location });
	}
	GeomUtils::RemovedPolylineSelfIntersection(Points2D);

	if (Points2D.Num() < 2)
	{
		return {};
	}

	TArray<FRoadSplineMeshPosition> OutPoints;
	TArray<FVector> Normals;
	OutPoints.Reserve(Points2D.Num());
	Normals.Reserve(Points2D.Num());
	for (auto& Point2D : Points2D)
	{
		FHitResult Hit;
		if (!Poly.Owner.FindRayIntersection(Point2D, Hit))
		{
			return {};
		}

		FRoadSplineMeshPosition Pos{};
		Pos.Location = Hit.ImpactPoint;
		OutPoints.Add(Pos);
		Normals.Add(Hit.Normal);
	}

	for (int Index = 0; Index < OutPoints.Num(); ++Index)
	{
		//auto UpVector = Normals[i];

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
		OutPoints[Index].bIsReverse = ((Poly.GetLaneIndex() != MetaRoad::ZeroLaneIndex) ? !Poly.GetLane().IsForwardLane() : false) ^ bIsReverse;
	}

	// Assign SOffset to each vertex via chord-length proportion over [S0, S1].
	// This survives self-intersection removal and any point insertion without a parallel array.
	{
		float TotalChord = 0.f;
		TArray<float> CumChord;
		CumChord.Reserve(OutPoints.Num());
		CumChord.Add(0.f);
		for (int i = 1; i < OutPoints.Num(); ++i)
		{
			TotalChord += (float)(OutPoints[i].Location - OutPoints[i - 1].Location).Size();
			CumChord.Add(TotalChord);
		}
		const double SSpan = S1 - S0;
		for (int i = 0; i < OutPoints.Num(); ++i)
		{
			const double Frac = (TotalChord > 0.f) ? (double)CumChord[i] / TotalChord : 0.0;
			OutPoints[i].SOffset = S0 + Frac * SSpan;
		}
	}

	OutPoints[0].bIsKey = true;
	OutPoints[0].Scale = KeyStart->Scale;
	OutPoints[0].Offset = KeyStart->Offset;
	OutPoints[0].Roll = KeyStart->Roll;

	if (KeyEnd)
	{
		OutPoints.Last().bIsKey = true;
		OutPoints.Last().Scale = KeyStart->Scale;
		OutPoints.Last().Offset = KeyStart->Offset;
		OutPoints.Last().Roll = KeyStart->Roll;
	}


	return OutPoints;
}

static FQuat GetQuaternionAtSplineInputKey(const FSplineCurves& SplineCurves, float InKey)
{
	FQuat Quat = SplineCurves.Rotation.Eval(InKey, FQuat::Identity);
	Quat.Normalize();
	const FVector Direction = SplineCurves.Position.EvalDerivative(InKey, FVector::ZeroVector).GetSafeNormal();
	const FVector UpVector = Quat.RotateVector(FVector::UpVector);
	return (FRotationMatrix::MakeFromXZ(Direction, UpVector)).ToQuat();
}

static void FitHermiteTangents(
	const FSplineCurves& SplineCurves,
	double SStart, double SEnd,
	const FVector& P0, const FVector& P3,
	FVector& OutT0, FVector& OutT3)
{
	constexpr int NSamples = 10;

	double SumA2 = 0.0, SumAB = 0.0, SumB2 = 0.0;
	FVector SumAR = FVector::ZeroVector;
	FVector SumBR = FVector::ZeroVector;

	for (int i = 1; i <= NSamples; ++i)
	{
		const double t   = (double)i / (NSamples + 1);
		const double S   = SStart + t * (SEnd - SStart);
		const float  Key = SplineCurves.ReparamTable.Eval((float)S, 0.0f);
		const FVector Q  = SplineCurves.Position.Eval(Key, FVector::ZeroVector);

		const double t2 = t * t, t3 = t2 * t;
		const double h00 =  2*t3 - 3*t2 + 1;
		const double h10 =    t3 - 2*t2 + t;
		const double h01 = -2*t3 + 3*t2;
		const double h11 =    t3 -   t2;

		const FVector R = Q - (P0 * h00 + P3 * h01);
		SumA2 += h10 * h10;
		SumAB += h10 * h11;
		SumB2 += h11 * h11;
		SumAR += R * h10;
		SumBR += R * h11;
	}

	const double Det = SumA2 * SumB2 - SumAB * SumAB;
	if (FMath::Abs(Det) < 1e-10)
	{
		const float KeyStart = SplineCurves.ReparamTable.Eval((float)SStart, 0.0f);
		const float KeyEnd   = SplineCurves.ReparamTable.Eval((float)SEnd,   0.0f);
		OutT0 = SplineCurves.Position.EvalDerivative(KeyStart, FVector::ZeroVector);
		OutT3 = SplineCurves.Position.EvalDerivative(KeyEnd,   FVector::ZeroVector);
		return;
	}

	const double Inv = 1.0 / Det;
	OutT0 = (SumAR * SumB2 - SumBR * SumAB) * Inv;
	OutT3 = (SumBR * SumA2 - SumAR * SumAB) * Inv;
}

struct FPolylineSplineCurves
{
	FSplineCurves SplineCurves;
	FInterpCurveVector2D ScaleCurve;
	FInterpCurveVector2D OffsetCurve;
	FInterpCurveFloat RollCurve;
	FInterpCurveFloat LeftWidthCurve;
	FInterpCurveFloat RightWidthCurve;
	/** Arc-length positions of bIsKey vertices, ascending. Populated by BuildPolylineSplineCurves().
	 *  Used by MakeSegments() in ERoadLaneAttributeGenerateSampler::BetweenKeys mode. */
	TArray<float> KeyArcs;
};

static FSplineCurves CompletePolyline(FRoadLanePolylineSplineMesh& Polyline)
{
	if (Polyline[0].bIsReverse ^ Polyline.SplineMeshProfile->bReversSplineDirection)
	{
		Algo::Reverse(Polyline.Vertices);
	}

	// Build SplineCurves (needed for arc-length ReparamTable)
	FSplineCurves SplineCurves;
	SplineCurves.Position.Points.Reserve(Polyline.Vertices.Num());
	SplineCurves.Rotation.Points.Reserve(Polyline.Vertices.Num());
	SplineCurves.Scale.Points.Reserve(Polyline.Vertices.Num());
	float InputKey = 0.0f;
	for (int i = 0; i < Polyline.Vertices.Num(); ++i)
	{
		auto& Point = Polyline[i];
		FVector RightVector, UpVector, ForwardVector;
		double SinA;
		GetThreeVectors(Polyline.Vertices, i, RightVector, UpVector, ForwardVector, SinA);

		SplineCurves.Position.Points.Emplace(InputKey, Point.Location, FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
		SplineCurves.Rotation.Points.Emplace(InputKey, FRotationMatrix::MakeFromXZ(ForwardVector, UpVector).ToQuat(), FQuat::Identity, FQuat::Identity, CIM_CurveAuto);
		SplineCurves.Scale.Points.Emplace(InputKey, FVector::OneVector, FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
		InputKey += 1.0f;
	}
	SplineCurves.UpdateSpline();

	// Estimate first key
	if (!Polyline.Vertices[0].bIsKey)
	{
		for (int i = 1; i <= Polyline.Vertices.Num(); ++i)
		{
			if (Polyline.Vertices[i].bIsKey)
			{
				auto& Cur = Polyline.Vertices[i];
				auto& First = Polyline.Vertices[0];
				First.Scale      = Cur.Scale;
				First.Offset     = Cur.Offset;
				First.Roll       = Cur.Roll;
				First.LeftWidth  = Cur.LeftWidth;
				First.RightWidth = Cur.RightWidth;
				First.bIsKey     = true;
				break;
			}
		}
	}

	// Estimate last key
	if (!Polyline.Vertices.Last().bIsKey)
	{
		for (int i = Polyline.Vertices.Num() - 2; i >= 0; --i)
		{
			if (Polyline.Vertices[i].bIsKey)
			{
				auto& Cur = Polyline.Vertices[i];
				auto& Last = Polyline.Vertices.Last();
				Last.Scale      = Cur.Scale;
				Last.Offset     = Cur.Offset;
				Last.Roll       = Cur.Roll;
				Last.LeftWidth  = Cur.LeftWidth;
				Last.RightWidth = Cur.RightWidth;
				Last.bIsKey     = true;
				break;
			}
		}
	}

	// Interpolate Scale/Offset/Roll between keys by arc length
	int StartKey = 0;
	for (int i = 1; i < Polyline.Vertices.Num(); ++i)
	{
		if (Polyline.Vertices[i].bIsKey)
		{
			const int EndKey = i;
			const auto& StartPos = Polyline.Vertices[StartKey];
			const auto& EndPos = Polyline.Vertices[EndKey];
			const float FullSegmentLength = SplineCurves.ReparamTable.Points[EndKey].InVal - SplineCurves.ReparamTable.Points[StartKey].InVal;
			for (int j = StartKey + 1; j < EndKey; ++j)
			{
				const float SegmentLength = SplineCurves.ReparamTable.Points[j].InVal - SplineCurves.ReparamTable.Points[StartKey].InVal;
				const float Alpha = SegmentLength / FullSegmentLength;
				Polyline.Vertices[j].Scale      = FMath::CubicInterp(StartPos.Scale, FVector2D::ZeroVector, EndPos.Scale, FVector2D::ZeroVector, Alpha);
				Polyline.Vertices[j].Offset     = FMath::CubicInterp(StartPos.Offset, FVector2D::ZeroVector, EndPos.Offset, FVector2D::ZeroVector, Alpha);
				Polyline.Vertices[j].Roll       = FMath::CubicInterp(StartPos.Roll, 0.0, EndPos.Roll, 0.0, Alpha);
				if (!Polyline.Vertices[j].bWidthComputed)
				{
					Polyline.Vertices[j].LeftWidth  = FMath::Lerp(StartPos.LeftWidth,  EndPos.LeftWidth,  Alpha);
					Polyline.Vertices[j].RightWidth = FMath::Lerp(StartPos.RightWidth, EndPos.RightWidth, Alpha);
				}
			}
			StartKey = EndKey;
		}
	}

	return SplineCurves;
}

static FPolylineSplineCurves BuildPolylineSplineCurves(
	const FRoadLanePolylineSplineMesh& Polyline,
	FSplineCurves SplineCurves)
{
	FPolylineSplineCurves Result;
	Result.SplineCurves = MoveTemp(SplineCurves);
	Result.ScaleCurve.Points.Reserve(Polyline.Vertices.Num());
	Result.OffsetCurve.Points.Reserve(Polyline.Vertices.Num());
	Result.RollCurve.Points.Reserve(Polyline.Vertices.Num());
	Result.LeftWidthCurve.Points.Reserve(Polyline.Vertices.Num());
	Result.RightWidthCurve.Points.Reserve(Polyline.Vertices.Num());
	float InputKey = 0.0f;
	for (int i = 0; i < Polyline.Vertices.Num(); ++i)
	{
		const auto& Point = Polyline.Vertices[i];
		Result.ScaleCurve.Points.Emplace(InputKey, Point.Scale, FVector2D::ZeroVector, FVector2D::ZeroVector, CIM_CurveAuto);
		Result.OffsetCurve.Points.Emplace(InputKey, Point.Offset, FVector2D::ZeroVector, FVector2D::ZeroVector, CIM_CurveAuto);
		Result.RollCurve.Points.Emplace(InputKey, Point.Roll, 0.0, 0.0, CIM_CurveAuto);
		Result.LeftWidthCurve.Points.Emplace(InputKey, Point.LeftWidth, 0.f, 0.f, CIM_CurveAuto);
		Result.RightWidthCurve.Points.Emplace(InputKey, Point.RightWidth, 0.f, 0.f, CIM_CurveAuto);
		InputKey += 1.0f;
	}
	Result.ScaleCurve.AutoSetTangents(0.0f, false);
	Result.OffsetCurve.AutoSetTangents(0.0f, false);
	Result.RollCurve.AutoSetTangents(0.0f, false);
	Result.LeftWidthCurve.AutoSetTangents(0.0f, false);
	Result.RightWidthCurve.AutoSetTangents(0.0f, false);

	// Collect arc-length positions of key vertices.
	// ReparamTable maps arc-length (InVal) → vertex parameter (OutVal).
	// ReparamTable.Points[k] is the k-th reparam sample — NOT the k-th vertex — so direct
	// Points[i].InVal gives the wrong arc-length. Binary-search OutVal == i to get the correct InVal.
	const auto& ReparamPts = Result.SplineCurves.ReparamTable.Points;
	for (int32 i = 0; i < Polyline.Vertices.Num(); ++i)
	{
		if (!Polyline.Vertices[i].bIsKey || ReparamPts.IsEmpty())
			continue;

		const float TargetParam = static_cast<float>(i);
		int32 Lo = 0, Hi = ReparamPts.Num() - 1;
		while (Lo < Hi)
		{
			const int32 Mid = (Lo + Hi) / 2;
			(ReparamPts[Mid].OutVal < TargetParam) ? (Lo = Mid + 1) : (Hi = Mid);
		}
		float ArcLength = ReparamPts[Lo].InVal;
		if (Lo > 0 && ReparamPts[Lo].OutVal > TargetParam)
		{
			const float Denom = ReparamPts[Lo].OutVal - ReparamPts[Lo - 1].OutVal;
			if (Denom > UE_KINDA_SMALL_NUMBER)
				ArcLength = FMath::Lerp(ReparamPts[Lo - 1].InVal, ReparamPts[Lo].InVal,
				                        (TargetParam - ReparamPts[Lo - 1].OutVal) / Denom);
		}
		Result.KeyArcs.Add(ArcLength);
	}

	return Result;
}

static TArray<FSplineMeshSegments::FSegment> MakeSegments(
	const FPolylineSplineCurves& Curves,
	const URoadLaneAttributeGenerateDescriptor* Profile)
{
	const FSplineCurves& SplineCurves = Curves.SplineCurves;

	TArray<FSplineMeshSegments::FSegment> OutSegments;

	auto MakeSegment = [&](double SStart, double SEnd)
	{
		const float ParamStart = SplineCurves.ReparamTable.Eval(SStart, 0.0f);
		const float ParamEnd   = SplineCurves.ReparamTable.Eval(SEnd,   0.0f);

		auto& NewSegment = OutSegments.Add_GetRef({});
		NewSegment.bAlignWorldUpVector = Profile->bAlignWorldUpVector;

		NewSegment.SplineMeshParams.StartPos = SplineCurves.Position.Eval(ParamStart, FVector::ZeroVector);
		NewSegment.SplineMeshParams.EndPos   = SplineCurves.Position.Eval(ParamEnd,   FVector::ZeroVector);

		FitHermiteTangents(SplineCurves, SStart, SEnd,
			NewSegment.SplineMeshParams.StartPos,
			NewSegment.SplineMeshParams.EndPos,
			NewSegment.SplineMeshParams.StartTangent,
			NewSegment.SplineMeshParams.EndTangent);

		NewSegment.SplineMeshParams.StartScale  = Curves.ScaleCurve.Eval(ParamStart,  FVector2D::One());
		NewSegment.SplineMeshParams.EndScale    = Curves.ScaleCurve.Eval(ParamEnd,    FVector2D::One());

		NewSegment.SplineMeshParams.StartOffset = Curves.OffsetCurve.Eval(ParamStart, FVector2D::One());
		NewSegment.SplineMeshParams.EndOffset   = Curves.OffsetCurve.Eval(ParamEnd,   FVector2D::One());

		NewSegment.StartLeftWidth  = Curves.LeftWidthCurve.Eval(ParamStart,  0.f);
		NewSegment.StartRightWidth = Curves.RightWidthCurve.Eval(ParamStart, 0.f);
		NewSegment.EndLeftWidth    = Curves.LeftWidthCurve.Eval(ParamEnd,    0.f);
		NewSegment.EndRightWidth   = Curves.RightWidthCurve.Eval(ParamEnd,   0.f);

		if (!Profile->bAlignWorldUpVector)
		{
			NewSegment.SplineMeshParams.StartRoll = FMath::DegreesToRadians(GetQuaternionAtSplineInputKey(SplineCurves, ParamStart).Rotator().Roll);
			NewSegment.SplineMeshParams.EndRoll   = FMath::DegreesToRadians(GetQuaternionAtSplineInputKey(SplineCurves, ParamEnd).Rotator().Roll);
		}

		NewSegment.SplineMeshParams.StartRoll += FMath::DegreesToRadians(Curves.RollCurve.Eval(ParamStart, 0.0));
		NewSegment.SplineMeshParams.EndRoll   += FMath::DegreesToRadians(Curves.RollCurve.Eval(ParamEnd,   0.0));

		NewSegment.Profile = Profile;
	};

	if (Profile->Sampler == ERoadLaneAttributeGenerateSampler::BetweenKeys && Curves.KeyArcs.Num() >= 2)
	{
		for (int32 i = 0; i + 1 < Curves.KeyArcs.Num(); ++i)
			MakeSegment(Curves.KeyArcs[i], Curves.KeyArcs[i + 1]);
	}
	else
	{
		const int32 NumberOfMeshes = FMath::Max(1, FMath::RoundToInt(SplineCurves.GetSplineLength() / Profile->LengthOfSegment));
		const double LengthOfSegment = SplineCurves.GetSplineLength() / NumberOfMeshes;
		for (int32 SplineCount = 0; SplineCount < NumberOfMeshes; SplineCount++)
			MakeSegment(SplineCount * LengthOfSegment, (SplineCount + 1) * LengthOfSegment);
	}

	return OutSegments;
}

static const URoadLaneAttributeGenerateDescriptor* FindProfile(const TSet<const URoadLaneAttributeGenerateDescriptor*>& Profiles, const TSoftClassPtr<URoadLaneAttributeDescriptor>& Profile)
{
	if (!Profile.ToSoftObjectPath().IsValid())
	{
		return nullptr;
	}
	for (auto& It : Profiles)
	{
		if (Profile.ToSoftObjectPath().GetAssetPath() == It->GetClass()->GetClassPathName())
		{
			return It;
		}
	}
	return nullptr;
}

FSplineMeshOp::FSplineMeshOp()
{
	TSet<UClass*> Classes = AssetUtils::GetAllClassesOfSubClass(URoadLaneAttributeGenerateDescriptor::StaticClass());
	for (auto& It : Classes)
	{
		if (auto * DefaultObject = It->GetDefaultObject<URoadLaneAttributeGenerateDescriptor>())
		{
			Profiles.Add(DefaultObject);
		}
	}
}


void FSplineMeshOp::CalculateResult(FProgressCancel* Progress)
{
#define CHECK_CANCLE() if (Progress && Progress->Cancelled()) { ResultInfo.Result = EGeometryResultType::Cancelled; return; }

	ResultInfo.Result = EGeometryResultType::InProgress;

	if (!BaseData || BaseData->ResultInfo.HasFailed())
	{
		ResultInfo.SetFailed(LOCTEXT("CalculateResultFail_Input", "FSplineMeshOp: input data is faild"));
		return;
	}

	// ========================== Add FRoadLaneAttributeGenerateValue attributes to thr arrangemen ==========================

	FRoadArrangemenSplineMesh Arrangemen;
	
	for (const auto& Poly : BaseData->Polygons)
	{
		if (Poly->GetType() != ERoadPolygonType::RoadLane)
		{
			continue;
		}
		auto* LanePoly = static_cast<const FProceduralPolygon_RoadLane*>(Poly.Get());
		auto& Section = LanePoly->GetSection();
		for (auto& [AttribyteDescriptor, Attribute] : LanePoly->GetLaneAttributes())
		{
			if (const URoadLaneAttributeGenerateDescriptor* FoundProfile = FindProfile(Profiles, AttribyteDescriptor))
			{
				bool bIsReverse = false;
				if (Attribute.Keys.Num())
				{
					if (auto* Value = Attribute.Keys[0].GetValuePtr<FRoadLaneAttributeGenerateValue>())
					{
						bIsReverse = Value->bIsReverse;
					}
				}

				for (int AttributeIndex = 0; AttributeIndex < Attribute.Keys.Num(); ++AttributeIndex)
				{
					const auto* KeyStart = &Attribute.Keys[AttributeIndex];
					const auto* ValueStart = KeyStart->GetValuePtr<FRoadLaneAttributeGenerateValue>();

					if (ValueStart && !ValueStart->bSkipSegment)
					{
						const auto* KeyEnd = (AttributeIndex < Attribute.Keys.Num() - 1) ? &Attribute.Keys[AttributeIndex + 1] : nullptr;
						const auto* ValueEnd = KeyEnd ? KeyEnd->GetValuePtr<FRoadLaneAttributeGenerateValue>() : nullptr;

						const double SOffsetStart = KeyStart->SOffset + Section.SOffset;
						const double SOffsetEnd = KeyEnd ? KeyEnd->SOffset + Section.SOffset : LanePoly->GetEndOffset();

						FRoadLanePolylineSplineMesh Polyline;
						Polyline.SplineMeshProfile = FoundProfile;
						Polyline.Vertices = MakePolylineSpline(*LanePoly, SOffsetStart, SOffsetEnd, ValueStart, ValueEnd, ChordToleranceSq, MinSegmentLength, bIsReverse);
						if (Polyline.Vertices.Num() > 1)
						{
							// Compute LeftWidth / RightWidth at the segment endpoints and store on
							// the key vertices so they propagate through CompletePolyline.
							const int LaneIndex = LanePoly->GetLaneIndex();
							auto ComputeWidths = [&](double SOffset, float Alpha,
							                         ERoadLaneAlignment Mode) -> FVector2D
							{
								if (LaneIndex == MetaRoad::ZeroLaneIndex)
								{
									const auto [LeftR, RightR] = Section.GetCenterLaneEdgeROffsets(SOffset);
									if (Mode == ERoadLaneAlignment::Auto)
									{
										const double W = RightR - LeftR;
										return FVector2D(W * 0.5, W * 0.5);
									}
									// Fixed center: Alpha=0→left edge, 0.5→R=0 (road ref), 1→right edge
									const double AnchorR = MetaRoad::MapCenterLaneROffset((double)Alpha, LeftR, RightR);
									return FVector2D(AnchorR - LeftR, RightR - AnchorR);
								}
								double LeftR, RightR;
								if (LaneIndex > 0)
								{
									LeftR  = (LaneIndex == 1) ? 0.0 : Section.EvalLaneROffset(LaneIndex - 1, SOffset);
									RightR = Section.EvalLaneROffset(LaneIndex, SOffset);
								}
								else
								{
									LeftR  = Section.EvalLaneROffset(LaneIndex, SOffset);
									RightR = (LaneIndex == -1) ? 0.0 : Section.EvalLaneROffset(LaneIndex + 1, SOffset);
								}
								const double W = RightR - LeftR;
								if (Mode == ERoadLaneAlignment::Auto)
									return FVector2D(W * 0.5, W * 0.5);
								else
									return FVector2D((double)Alpha * W, (1.0 - (double)Alpha) * W);
							};

							const ERoadLaneAlignment ModeStart = ValueStart ? ValueStart->Alignment : ERoadLaneAlignment::Auto;
							const ERoadLaneAlignment ModeEnd   = ValueEnd   ? ValueEnd->Alignment   : ModeStart;
							const float AlphaStart = ValueStart ? (float)ValueStart->Alpha : 0.5f;
							const float AlphaEnd   = ValueEnd   ? (float)ValueEnd->Alpha   : AlphaStart;

							// Compute LeftWidth/RightWidth at every vertex using its SOffset.
							// This follows the Width FRichCurve accurately instead of linearly
							// interpolating between just the two attribute-key endpoints.
							const double SpanLen = FMath::Max(SOffsetEnd - SOffsetStart, 1.0);
							for (auto& V : Polyline.Vertices)
							{
								const double VSOffset = (V.SOffset >= 0.0) ? V.SOffset : SOffsetStart;
								const double Frac     = FMath::Clamp((VSOffset - SOffsetStart) / SpanLen, 0.0, 1.0);
								const float  Alpha    = FMath::Lerp(AlphaStart, AlphaEnd, (float)Frac);
								const ERoadLaneAlignment Mode = (Frac <= 0.5) ? ModeStart : ModeEnd;
								FVector2D W = ComputeWidths(VSOffset, Alpha, Mode);
								if (V.bIsKey && Frac < UE_KINDA_SMALL_NUMBER)
								{
									if (ValueStart && ValueStart->OverrideLeftWidth)  W.X = (float)ValueStart->LeftWidth;
									if (ValueStart && ValueStart->OverrideRightWidth) W.Y = (float)ValueStart->RightWidth;
								}
								else if (V.bIsKey && Frac > 1.0 - UE_KINDA_SMALL_NUMBER)
								{
									if (ValueEnd && ValueEnd->OverrideLeftWidth)      W.X = (float)ValueEnd->LeftWidth;
									if (ValueEnd && ValueEnd->OverrideRightWidth)     W.Y = (float)ValueEnd->RightWidth;
								}
								V.LeftWidth      = (float)W.X;
								V.RightWidth     = (float)W.Y;
								V.bWidthComputed = true;
							}

							const double ArrangemenTolerance = 10.0;
							Arrangemen.Insert(MoveTemp(Polyline), ArrangemenTolerance);
						}
					}
				}
			}
		}

		CHECK_CANCLE();
	}

	// ========================== Create segments  ==========================
	for (auto& Polyline : Arrangemen.Polylines)
	{
		FSplineCurves SplineCurves = CompletePolyline(Polyline);
		const FPolylineSplineCurves Curves = BuildPolylineSplineCurves(Polyline, MoveTemp(SplineCurves));
		auto Segments = MakeSegments(Curves, Polyline.SplineMeshProfile);
		ResultSegments->Segments.Append(MoveTemp(Segments));

		CHECK_CANCLE();
	}

	// ========================== Draw debug lines  ==========================
	if (bDrawRefSplines)
	{
		for (auto& Polyline : Arrangemen.Polylines)
		{
			FDebugLineBuffer::FBatch Batch;
			Batch.Thickness = 4.0;
			Batch.Color = FColor(0, 255, 0, 100);
			for (int i = 0; i < Polyline.Num() - 1; ++i)
			{
				Batch.Lines.Add({ Polyline[i].Location, Polyline[i + 1].Location, });
			}
			BaseData->DebugDraw.Add(MoveTemp(Batch));
		}
	}

	ResultInfo.SetSuccess();

#undef CHECK_CANCLE
}

#undef LOCTEXT_NAMESPACE
