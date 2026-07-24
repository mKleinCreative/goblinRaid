/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "Assets/RoadLaneAttributeMark.h"
#include "RoadMarkProfileThumbnailRenderer.generated.h"

class FCanvas;
class FRenderTarget;

UCLASS()
class URoadMarkProfileThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_UCLASS_BODY()

	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas, bool bAdditionalViewFamily) override;

	void DrawSolidLane(FCanvas* Canvas, int32 X, int32 Y, uint32 Width, uint32 Height, const FRoadLaneMarkProfileSolid& Profile, float LeftOffset) const ;
	void DrawBrokedLane(FCanvas* Canvas, int32 X, int32 Y, uint32 Width, uint32 Height, const FRoadLaneMarkProfileBroked& Profile, float LeftOffset) const;
	bool DrawSolidOrBrokedLane(FCanvas* Canvas, int32 X, int32 Y, uint32 Width, uint32 Height, const TInstancedStruct<FRoadLaneMarkProfile>& Profile, float LeftOffset) const;

	const float Scale = 0.00078125f;
	const float MarkWidtScale = 3.0;
};
