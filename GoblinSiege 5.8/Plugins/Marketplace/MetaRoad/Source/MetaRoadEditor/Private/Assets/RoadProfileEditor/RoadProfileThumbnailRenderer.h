/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "RoadProfileThumbnailRenderer.generated.h"

class FRoadProfileThumbnailScene;
class FCanvas;
class FRenderTarget;

/**
 * URoadProfileThumbnailRenderer
 *
 * Renders the asset thumbnail for URoadProfile as the generated road mesh, mirroring the engine's
 * UStaticMeshThumbnailRenderer pattern (an FThumbnailPreviewScene rendered to the thumbnail canvas).
 * The road mesh is generated synchronously from the profile (triangulation + drive-surface op).
 */
UCLASS()
class URoadProfileThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_BODY()

public:
	virtual bool CanVisualizeAsset(UObject* Object) override;
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height,
	                  FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily) override;
	virtual void BeginDestroy() override;

private:
	FRoadProfileThumbnailScene* ThumbnailScene = nullptr;
};
