/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Assets/RoadLaneAttributeDescriptor.h"
#include "RoadLaneAttributePolygoneCustomization.generated.h"

namespace MetaRoad
{
	struct FProceduralPolygon_RoadLane;
}

/**
 * FRoadLaneAttributePolygoneCustomizationValue
 */
USTRUCT(BlueprintType, Blueprintable)
struct METAROADEDITOR_API FRoadLaneAttributePolygoneCustomizationValue : public FRoadLaneAttributeValue
{
	GENERATED_USTRUCT_BODY()


	virtual ~FRoadLaneAttributePolygoneCustomizationValue() {}

	virtual void OnPolygoneCreated(MetaRoad::FProceduralPolygon_RoadLane& Polygone, double SKeyOffset) const {}

	virtual void GetAdditionalSplinePoints(const MetaRoad::FProceduralPolygon_RoadLane& Polygone, double SKeyOffset, double LaneAlpha, TArray<double>& OutSegmants) const {}

	virtual double GetHeightOffset(const MetaRoad::FProceduralPolygon_RoadLane& Polygone, double SKeyOffset, double SOffset, double LaneAlpha, double InHeightOffset) const { return InHeightOffset; }
};

