/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "SEditorViewport.h"

class FAdvancedPreviewScene;
class FRoadProfileViewportClient;
class URoadProfilePreviewBuilder;

/** Fired when the user right-clicks a lane in the preview (LaneIndex; ZeroLaneIndex = center). */
DECLARE_DELEGATE_OneParam(FOnLaneContextMenu, int32 /*LaneIndex*/);

/**
 * SRoadProfileViewport
 *
 * 3D preview viewport for the Road Profile asset editor. Hosts an FAdvancedPreviewScene whose world
 * is used by URoadProfilePreviewBuilder to build a short preview road; the generated dynamic-mesh
 * preview renders here. Mirrors the standard SEditorViewport / FEditorViewportClient / preview-scene
 * trio used by engine asset-editor viewports.
 */
class SRoadProfileViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SRoadProfileViewport) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SRoadProfileViewport() override;

	/** World the preview meshes are placed in (the advanced preview scene's world). */
	UWorld* GetPreviewWorld() const;

	/** Attach the builder so the viewport client can tick it and redraw on updates. */
	void SetPreviewBuilder(URoadProfilePreviewBuilder* Builder);

	/** Bind a handler invoked when the user right-clicks a lane (to show a context menu). */
	void SetContextMenuHandler(FOnLaneContextMenu Handler);

protected:
	// SEditorViewport
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	// Note: MakeViewportToolbar() is intentionally NOT overridden. The base implementation already
	// returns nullptr (no toolbar) on both UE 5.6 and 5.7, and in 5.7 it was made 'final' so any
	// override fails to compile.

private:
	void OnBuilderUpdated();

	TSharedPtr<FAdvancedPreviewScene> PreviewScene;
	TSharedPtr<FRoadProfileViewportClient> ViewportClient;
};
