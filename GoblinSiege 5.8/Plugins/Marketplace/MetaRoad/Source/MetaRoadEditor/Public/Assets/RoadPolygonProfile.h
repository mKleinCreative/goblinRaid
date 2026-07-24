/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Engine/EngineTypes.h"
#include "MetaRoadTypes.h"
#include "RoadPolygonProfile.generated.h"

/**
 * FRoadPolygon
 * TODO: Add holes
 */
USTRUCT(BlueprintType)
struct METAROADEDITOR_API FRoadCurvePolygon
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Polygon)
	FInterpCurveVector2D Curve;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Polygon)
	TInstancedStruct<FRoadZone> RoadZone;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Polygon)
	double TextureAngle = 0.0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Polygon)
	double TextureScale = 1.0;

	// Linear chord tolerance (cm): a curve segment is subdivided while its midpoint
	// deviates from the chord by more than this distance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Polygon, meta = (ClampMin = "0.001", UIMin = "0.001"))
	double ChordTolerance = 2.0;

	FColor GetEditorColor() const
	{
		const FRoadZoneTypeDetails* Details = RoadZone.IsValid() ? RoadZone.Get<FRoadZone>().ZoneType.GetDetails() : nullptr;
		return Details ? Details->EditorColor.ToFColor(true) : FColor::White;
	}
};


/**
 * URoadPolygonProfile
 */
UCLASS(BlueprintType, Blueprintable)
class METAROADEDITOR_API URoadPolygonProfile : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Polygon)
	TArray<FRoadCurvePolygon> Polygones;
};