/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RoadSplineComponent.h"

class FPrimitiveDrawInterface;
class FMaterialRenderProxy;

#if WITH_EDITOR

struct FMetaRoadColors
{
	static METAROAD_API const FColor EmptyColor;
	static METAROAD_API const FColor SelectedColor;
	static METAROAD_API const FColor ReadOnlyColor;
	static METAROAD_API const FColor ErrColor;
	static METAROAD_API const FColor RestrictedColor;

	static METAROAD_API const FColor SplineColor;
	static METAROAD_API const FColor CrossSplineColor;
	static METAROAD_API const FColor TangentColor;
	static METAROAD_API const FColor AccentColorHi;
	static METAROAD_API const FColor AccentColorLow;
};



namespace DrawUtils
{
	METAROAD_API void TrimPoints(double SplineParam0, double SplineParam1, TArray<FSplinePositionLinearApproximation>& Points);

	METAROAD_API void  DrawLaneBorder(FPrimitiveDrawInterface* PDI, const URoadSplineComponent* SplineComp, int SectionIndex, int LaneIndex, double S0, double S1, const FColor& Color1, const FColor& Color2, uint8 DepthPriorityGroup, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false);
	METAROAD_API void  DrawLaneBorder(FPrimitiveDrawInterface* PDI, const URoadSplineComponent* SplineComp, int SectionIndex, int LaneIndex, const FColor& Color1, const FColor& Color2, uint8 DepthPriorityGroup, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false);

	METAROAD_API void  DrawSpline(FPrimitiveDrawInterface* PDI, const URoadSplineComponent* SplineComp, double S0, double S1, const FColor& Color, uint8 DepthPriorityGroup, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false);

	METAROAD_API void  DrawCrossSpline(FPrimitiveDrawInterface* PDI, const URoadSplineComponent* Spline, float SplineKey, const FColor& Color, uint8 DepthPriorityGroup, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false);

	METAROAD_API FLinearColor HSVMul(const FLinearColor& Color, float Saturation, float Brightness);

	METAROAD_API void DrawDisc(class FPrimitiveDrawInterface* PDI, const FVector& Base, const FVector& XAxis, const FVector& YAxis, FColor Color, double Radius, int32 NumSides, const FMaterialRenderProxy* MaterialRenderProxy, uint8 DepthPriority);

	inline FLinearColor MakeLowAccent(const FLinearColor& Color, float Saturation = 0.5, float Brightness = 0.5)
	{ 
		return HSVMul(Color, Saturation, Brightness);
	}



} // MetaRoad

#endif