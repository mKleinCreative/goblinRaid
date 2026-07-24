/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "Assets/RoadLaneAttributeLandscape.h"
#include "Textures/SlateIcon.h"

#define LOCTEXT_NAMESPACE "RoadLaneAttributeLandscape"

bool FRoadLaneAttributeValueLandscape::Interpolate(const UScriptStruct* ValueType, const FRoadLaneAttributeValue* InOther, float Alpha, FRoadLaneAttributeValue* InOut) const
{
	if (ValueType != FRoadLaneAttributeValueLandscape::StaticStruct())
	{
		return false;
	}

	check(InOther && InOut);

	const FRoadLaneAttributeValueLandscape& Other = *static_cast<const FRoadLaneAttributeValueLandscape*>(InOther);
	FRoadLaneAttributeValueLandscape& Out = *static_cast<FRoadLaneAttributeValueLandscape*>(InOut);

	Out.LayerFactor = FMath::CubicInterp<float>(LayerFactor, 0.f, Other.LayerFactor, 0.f, Alpha);
	Out.SideFalloff = FMath::CubicInterp<float>(SideFalloff, 0.f, Other.SideFalloff, 0.f, Alpha);
	Out.SideOffset = FMath::CubicInterp<float>(SideOffset, 0.f, Other.SideOffset, 0.f, Alpha);
	Out.HeightOffset = FMath::CubicInterp<float>(HeightOffset, 0.f, Other.HeightOffset, 0.f, Alpha);
	
	return true;
}

#undef LOCTEXT_NAMESPACE