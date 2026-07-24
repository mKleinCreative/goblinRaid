/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RoadSectionComponentVisualizer.h"


class  FRoadWidthComponentVisualizer : public FRoadSectionComponentVisualizer
{
public:
	FRoadWidthComponentVisualizer();
	virtual ~FRoadWidthComponentVisualizer();

	virtual void OnRegister() override;
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	virtual void TrackingStarted(FEditorViewportClient* InViewportClient) override;
	virtual void TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove) override;
	virtual FString GetReferencerName() const override { return GetReferencerNameStatic(); }
	static FString GetReferencerNameStatic() { return TEXT("FRoadWidthComponentVisualizer"); }

protected:
	virtual void GenerateChildContextMenuSections(FMenuBuilder& InMenuBuilder) const;
	virtual void ConfigureKeyOverlay() override;

	void OnAddWidthKey();
	void OnDeleteWidthKey();
};
