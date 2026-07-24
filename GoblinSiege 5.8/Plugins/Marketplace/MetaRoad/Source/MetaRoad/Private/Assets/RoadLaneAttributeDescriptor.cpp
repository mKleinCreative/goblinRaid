/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "Assets/RoadLaneAttributeDescriptor.h"
#include "RoadSplineComponent.h"

#if WITH_EDITOR

bool URoadLaneAttributeDescriptor::CanBeAddedTo(const URoadSplineComponent* Spline, int SectionIndex, int LaneIndex) const 
{ 
	if (!IsValid(Spline))
	{
		return false;
	}

	if (!Spline->GetRoadLayout().Sections.IsValidIndex(SectionIndex))
	{
		return false;
	}

	return Spline->GetRoadLayout().Sections[SectionIndex].CheckLaneIndex(LaneIndex) || LaneIndex == MetaRoad::ZeroLaneIndex;
}

#endif

