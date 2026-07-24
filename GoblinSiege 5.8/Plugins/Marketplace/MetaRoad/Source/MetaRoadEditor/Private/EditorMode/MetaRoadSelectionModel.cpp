/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "EditorMode/MetaRoadSelectionModel.h"
#include "ComponentVisualizers/RoadSectionComponentVisualizer.h"
#include "ComponentVisualizers/RoadOffsetComponentVisualizer.h"
#include "ComponentVisualizers/RoadSplineComponentVisualizer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaRoadSelectionModel)

URoadSectionComponentVisualizerSelectionState* UMetaRoadSelectionModel::GetSectionState()
{
	if (!SectionState)
	{
		SectionState = NewObject<URoadSectionComponentVisualizerSelectionState>(
			this, TEXT("SectionSelectionState"), RF_Transactional);
	}
	return SectionState;
}

URoadOffsetComponentVisualizerSelectionState* UMetaRoadSelectionModel::GetOffsetState()
{
	if (!OffsetState)
	{
		OffsetState = NewObject<URoadOffsetComponentVisualizerSelectionState>(
			this, TEXT("OffsetSelectionState"), RF_Transactional);
	}
	return OffsetState;
}

URoadSplineComponentVisualizerSelectionState* UMetaRoadSelectionModel::GetSplineState()
{
	if (!SplineState)
	{
		SplineState = NewObject<URoadSplineComponentVisualizerSelectionState>(
			this, TEXT("SplineSelectionState"), RF_Transactional);
	}
	return SplineState;
}
