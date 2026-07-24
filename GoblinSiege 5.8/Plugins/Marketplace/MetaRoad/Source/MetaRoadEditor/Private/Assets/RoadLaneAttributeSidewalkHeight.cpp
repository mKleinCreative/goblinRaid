/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "Assets/RoadLaneAttributeSidewalkHeight.h"
#include "RoadSplineComponent.h"

bool FRoadLaneAttributeSidewalkHeightValue::Interpolate(const UScriptStruct* ValueType, const FRoadLaneAttributeValue* InOther, float AttrAlpha, FRoadLaneAttributeValue* InOut) const
{
	if (ValueType != FRoadLaneAttributeSidewalkHeightValue::StaticStruct())
	{
		return false;
	}

	check(InOther && InOut);

	const FRoadLaneAttributeSidewalkHeightValue& Other = *static_cast<const FRoadLaneAttributeSidewalkHeightValue*>(InOther);
	FRoadLaneAttributeSidewalkHeightValue& Out = *static_cast<FRoadLaneAttributeSidewalkHeightValue*>(InOut);

	Out.Height = FMath::CubicInterp<double>(Height, 0.f, Other.Height, 0.f, AttrAlpha);

	return true;
}


bool URoadLaneAttributeSidewalkHeightDescriptor::CanBeAddedTo(const URoadSplineComponent* Spline, int SectionIndex, int LaneIndex) const
{
	if (!Super::CanBeAddedTo(Spline, SectionIndex, LaneIndex))
	{
		return false;
	}
	return Spline->GetRoadLayout().Sections[SectionIndex].GetLaneByIndex(LaneIndex).RoadZone.GetPtr<FRoadZoneSidewalk>() != nullptr;
}