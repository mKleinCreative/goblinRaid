/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */


#include "Utils/DrawUtils.h"

#if WITH_EDITOR

#include "EngineUtils.h"
#include "MetaRoadSettings.h"
#include "DynamicMeshBuilder.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "Styling/StyleColors.h"


const FColor FMetaRoadColors::EmptyColor(106, 145, 196);
const FColor FMetaRoadColors::SelectedColor = FStyleColors::AccentOrange.GetSpecifiedColor().ToFColor(true);
const FColor FMetaRoadColors::ReadOnlyColor(255, 0, 255, 255);
const FColor FMetaRoadColors::ErrColor(184, 15, 10, 255);
const FColor FMetaRoadColors::RestrictedColor(104, 151, 187, 255);
const FColor FMetaRoadColors::AccentColorHi(129, 106, 196);
const FColor FMetaRoadColors::AccentColorLow = DrawUtils::MakeLowAccent(FMetaRoadColors::AccentColorHi).ToFColor(true);

const FColor FMetaRoadColors::SplineColor = FStyleColors::AccentPink.GetSpecifiedColor().ToFColor(true);
const FColor FMetaRoadColors::CrossSplineColor = FColor::Yellow;
const FColor FMetaRoadColors::TangentColor = FLinearColor(0.718f, 0.589f, 0.921f).ToFColor(true);

namespace DrawUtils
{

void TrimPoints(double SplineParam0, double SplineParam1, TArray<FSplinePositionLinearApproximation>& Points)
{
	int32 StartKey = INDEX_NONE;
	int32 EndKey = INDEX_NONE;

	for (int32 KeyIndex = 0; KeyIndex < Points.Num(); ++KeyIndex)
	{
		const double CurrentS = Points[KeyIndex].SplineParam;
		if (CurrentS <= SplineParam0)
		{
			StartKey = KeyIndex;
		}
		if (CurrentS >= SplineParam1)
		{
			EndKey = KeyIndex;
			break;
		}
	}

	if (EndKey != INDEX_NONE && (Points.Num() - 1 != EndKey))
	{
		Points.SetNumUninitialized(EndKey + 1);
	}

	if (StartKey != INDEX_NONE && StartKey != 0)
	{
		Points.RemoveAt(0, StartKey);
	}

	if (Points.Num())
	{
		if (Points[0].SplineParam < SplineParam0)
		{
			if (Points.Num() > 1)
			{
				double Alpha = (SplineParam0 - Points[0].SplineParam) / (Points[1].SplineParam - Points[0].SplineParam);
				Points[0].Position = FMath::Lerp(Points[0].Position, Points[1].Position, Alpha);
			}
			Points[0].SplineParam = SplineParam0;
		}

		if (Points[Points.Num() - 1].SplineParam > SplineParam1)
		{
			if (Points.Num() > 1)
			{
				double Alpha = (SplineParam1 - Points[Points.Num() - 2].SplineParam) / (Points[Points.Num() - 1].SplineParam - Points[Points.Num() - 2].SplineParam);
				Points[Points.Num() - 1].Position = FMath::Lerp(Points[Points.Num() - 2].Position, Points[Points.Num() - 1].Position, Alpha);
			}
			Points[Points.Num() - 1].SplineParam = SplineParam1;
		}
	}
};

void DrawLaneBorder(FPrimitiveDrawInterface* PDI, const URoadSplineComponent* SplineComp, int SectionIndex, int LaneIndex, double S0, double S1, const FColor& Color1, const FColor& Color2, uint8 DepthPriorityGroup, float Thickness, float DepthBias, bool bScreenSpace)
{
	const auto& Section = SplineComp->GetLaneSection(SectionIndex);

	double StartS, EndS;
	if (LaneIndex == MetaRoad::ZeroLaneIndex)
	{
		StartS = Section.SOffset;
		EndS = Section.SOffsetEnd_Cashed;
	}
	else
	{
		auto& Lane = Section.GetLaneByIndex(LaneIndex);
		StartS = Lane.GetStartOffset();
		EndS = Lane.GetEndOffset();
	}

	const int NumPointPerSegmaent = GetDefault<UMetaRoadSettings>()->NumPointPerSegmaent;
	const int NumPointPerSection = GetDefault<UMetaRoadSettings>()->NumPointPerSection;

	TArray<FSplinePositionLinearApproximation> Points;
	SplineComp->BuildLinearApproximation(
		Points,
		[&](double S)
		{
			return (LaneIndex == MetaRoad::ZeroLaneIndex ? 0 : Section.EvalLaneROffset(LaneIndex, S, 1.0)) + SplineComp->EvalROffset(S);
		},
		StartS, EndS, NumPointPerSegmaent, NumPointPerSection, ESplineCoordinateSpace::World);

	if (!FMath::IsNearlyEqual(S0, StartS) || !FMath::IsNearlyEqual(S1, EndS))
	{
		TrimPoints(SplineComp->SplineCurves.ReparamTable.Eval(S0, 0.0f), SplineComp->SplineCurves.ReparamTable.Eval(S1, 0.0f), Points);
	}

	for (int32 StepIdx = 1; StepIdx < Points.Num(); StepIdx++)
	{
		if (StepIdx % 2)
		{
			PDI->DrawTranslucentLine(Points[StepIdx - 1].Position, Points[StepIdx].Position, Color1, DepthPriorityGroup, Thickness, DepthBias, bScreenSpace);
		}
		else
		{
			PDI->DrawTranslucentLine(Points[StepIdx - 1].Position, Points[StepIdx].Position, Color2, DepthPriorityGroup, Thickness, DepthBias, bScreenSpace);
		}
	}
}

void DrawLaneBorder(FPrimitiveDrawInterface* PDI, const URoadSplineComponent* SplineComp, int SectionIndex, int LaneIndex, const FColor& Color1, const FColor& Color2, uint8 DepthPriorityGroup, float Thickness, float DepthBias, bool bScreenSpace)
{
	const FRoadLaneSection& Section = SplineComp->GetLaneSection(SectionIndex);
	DrawLaneBorder(PDI, SplineComp, SectionIndex, LaneIndex, Section.SOffset, Section.SOffsetEnd_Cashed, Color1, Color2, DepthPriorityGroup, Thickness, DepthBias, bScreenSpace);
}

void DrawSpline(FPrimitiveDrawInterface* PDI, const URoadSplineComponent* SplineComp, double S0, double S1, const FColor& Color, uint8 DepthPriorityGroup, float Thickness, float DepthBias, bool bScreenSpace)
{
	const int NumPointPerSegmaent = GetDefault<UMetaRoadSettings>()->NumPointPerSegmaent;
	const int NumPointPerSection = GetDefault<UMetaRoadSettings>()->NumPointPerSection;

	TArray<FSplinePositionLinearApproximation> Points;
	SplineComp->BuildLinearApproximation(
		Points,
		[&](double S)
		{
			return 0;
		},
		S0, S1, NumPointPerSegmaent, NumPointPerSection, ESplineCoordinateSpace::World);

	for (int32 StepIdx = 1; StepIdx < Points.Num(); StepIdx++)
	{
		PDI->DrawLine(Points[StepIdx - 1].Position, Points[StepIdx].Position, Color, DepthPriorityGroup, Thickness, DepthBias, bScreenSpace);
	}
}

void DrawCrossSpline(FPrimitiveDrawInterface* PDI, const URoadSplineComponent* Spline, float SplineKey, const FColor& Color, uint8 DepthPriorityGroup, float Thickness, float DepthBias, bool bScreenSpace)
{
	int SectionIndex = Spline->FindRoadSectionOnSplineKey(SplineKey);

	if (SectionIndex == INDEX_NONE)
	{
		return;
	}

	int LeftSectionIndex = Spline->GetRoadLayout().FindSideSection(SectionIndex, ERoadLaneSectionSide::Left);
	int RightSectionIndex = Spline->GetRoadLayout().FindSideSection(SectionIndex, ERoadLaneSectionSide::Right);

	if (LeftSectionIndex == INDEX_NONE)
	{
		LeftSectionIndex = 0;
	}

	if (RightSectionIndex == INDEX_NONE)
	{
		RightSectionIndex = 0;
	}

	const float SOffset = Spline->GetDistanceAlongSplineAtSplineInputKey(SplineKey);
	const auto LeftLoc = Spline->EvalLanePoistion(LeftSectionIndex, -Spline->GetLaneSection(LeftSectionIndex).Left.Num(), SOffset, 1.0, ESplineCoordinateSpace::World);
	const auto RightLoc = Spline->EvalLanePoistion(RightSectionIndex, +Spline->GetLaneSection(RightSectionIndex).Right.Num(), SOffset, 1.0, ESplineCoordinateSpace::World);

	PDI->DrawLine(LeftLoc, RightLoc, Color, DepthPriorityGroup, Thickness, DepthBias, bScreenSpace);
}

FLinearColor HSVMul(const FLinearColor& Color, float Saturation, float Brightness)
{
	auto HSV = Color.LinearRGBToHSV();
	HSV.G *= Saturation;
	HSV.B *= Brightness;
	return HSV.HSVToLinearRGB();
}

void DrawDisc(class FPrimitiveDrawInterface* PDI, const FVector& Base, const FVector& XAxis, const FVector& YAxis, FColor Color, double Radius, int32 NumSides, const FMaterialRenderProxy* MaterialRenderProxy, uint8 DepthPriority)
{
	check(NumSides >= 3);

	const float	AngleDelta = 2.0f * UE_PI / NumSides;

	FVector2D TC = FVector2D(0.0f, 0.0f);
	float TCStep = 1.0f / NumSides;

	FVector ZAxis = (XAxis) ^ YAxis;

	FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());

	//Compute vertices for base circle.
	for (int32 SideIndex = 0; SideIndex < NumSides; SideIndex++)
	{
		const FVector Vertex = Base + (XAxis * FMath::Cos(AngleDelta * (SideIndex)) + YAxis * FMath::Sin(AngleDelta * (SideIndex))) * Radius;

		FDynamicMeshVertex MeshVertex;
		MeshVertex.Position = (FVector3f)Vertex;
		MeshVertex.Color = Color;


		FVector Normal = ZAxis;// Vertex - Base;
		Normal.Normalize();

		MeshVertex.TextureCoordinate[0] = FVector2f(TC);
		MeshVertex.TextureCoordinate[0].X += TCStep * SideIndex;

		MeshVertex.SetTangents(
			(FVector3f)-ZAxis,
			FVector3f((-ZAxis) ^ Normal),
			(FVector3f)Normal
		);


		MeshBuilder.AddVertex(MeshVertex); //Add bottom vertex
	}

	//Add top/bottom triangles, in the style of a fan.
	for (int32 SideIndex = 0; SideIndex < NumSides - 1; SideIndex++)
	{
		int32 V0 = 0;
		int32 V1 = SideIndex;
		int32 V2 = (SideIndex + 1);

		MeshBuilder.AddTriangle(V0, V1, V2);
		MeshBuilder.AddTriangle(V0, V2, V1);
	}

	MeshBuilder.Draw(PDI, FMatrix::Identity, MaterialRenderProxy, DepthPriority, false, false);
}



} // DrawUtils

#endif