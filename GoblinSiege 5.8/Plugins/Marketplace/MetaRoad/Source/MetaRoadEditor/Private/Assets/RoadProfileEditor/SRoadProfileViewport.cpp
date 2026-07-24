/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "SRoadProfileViewport.h"
#include "RoadProfilePreviewBuilder.h"
#include "AdvancedPreviewScene.h"
#include "EditorViewportClient.h"
#include "RoadSplineComponent.h"
#include "RoadSceneProxy/RoadSceneProxy.h"
#include "Utils/DrawUtils.h"

// -----------------------------------------------------------------------------------------------------
// FRoadProfileViewportClient
// -----------------------------------------------------------------------------------------------------

class FRoadProfileViewportClient : public FEditorViewportClient
{
public:
	FRoadProfileViewportClient(FAdvancedPreviewScene* InPreviewScene, const TSharedRef<SRoadProfileViewport>& InViewport)
		: FEditorViewportClient(nullptr, InPreviewScene, StaticCastSharedRef<SEditorViewport>(InViewport))
		, AdvancedScene(InPreviewScene)
	{
		SetRealtime(true);

		// Top-down view: camera directly above the world-centered road, looking straight down (-Z).
		SetViewLocation(FVector(0.0, 0.0, 3000.0));
		SetViewRotation(FRotator(-90.0, 0.0, 0.0));
		SetViewLocationForOrbiting(FVector::ZeroVector);

		EngineShowFlags.SetGrid(true);
		// FEditorViewportClient's ctor sets DrawHelper.bDrawGrid=false ("rely on show flags"), but the
		// show-flag grid only renders in the level editor — preview scenes need DrawHelper to draw it.
		DrawHelper.bDrawGrid = true;
		bSetListenerPosition = false;
	}

	void SetBuilder(URoadProfilePreviewBuilder* InBuilder)
	{
		Builder = InBuilder;
		bPendingFrame = true;
	}

	/** Position the camera (keeping the top-down rotation) so the whole preview road is in view. */
	void FrameRoad()
	{
		// World-centered straight road (~20 m along X) plus generous lateral margin for wide profiles.
		const FBox RoadBox(FVector(-1100.0, -1600.0, -50.0), FVector(1100.0, 1600.0, 50.0));
		FocusViewportOnBox(RoadBox, /*bInstant=*/true);
	}

	virtual void Tick(float DeltaSeconds) override
	{
		FEditorViewportClient::Tick(DeltaSeconds);
		if (AdvancedScene)
		{
			AdvancedScene->Tick(DeltaSeconds);
		}
		if (URoadProfilePreviewBuilder* B = Builder.Get())
		{
			B->Tick(DeltaSeconds);
		}

		// Frame once the viewport has a valid size (FocusViewportOnBox needs the aspect ratio).
		if (bPendingFrame && Viewport && Viewport->GetSizeXY().X > 0 && Viewport->GetSizeXY().Y > 0)
		{
			FrameRoad();
			bPendingFrame = false;
		}
	}

	// Left-click selects the clicked lane (or clears the selection); right-click selects it and
	// requests a context menu. Lane hit proxies come from the road spline's scene proxy.
	virtual void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override
	{
		URoadProfilePreviewBuilder* B = Builder.Get();
		if (!B)
		{
			FEditorViewportClient::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);
			return;
		}

		int32 LaneIndex = URoadProfilePreviewBuilder::NoSelection;
		if (HitProxy && HitProxy->IsA(HRoadLaneVisProxy::StaticGetType()))
		{
			LaneIndex = static_cast<HRoadLaneVisProxy*>(HitProxy)->LaneIndex;
		}

		if (Key == EKeys::LeftMouseButton)
		{
			B->SetSelectedLane(LaneIndex);
			Invalidate();
		}
		else if (Key == EKeys::RightMouseButton && LaneIndex != URoadProfilePreviewBuilder::NoSelection)
		{
			B->SetSelectedLane(LaneIndex);
			Invalidate();
			OnContextMenuRequested.ExecuteIfBound(LaneIndex);
		}
		else
		{
			FEditorViewportClient::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);
		}
	}

	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override
	{
		FEditorViewportClient::Draw(View, PDI);

		URoadProfilePreviewBuilder* B = Builder.Get();
		if (!B)
		{
			return;
		}
		// Real lanes are highlighted by the spline's scene proxy (via SetSelectedLane). The center
		// reference line has no lane mesh, so draw its border here when it is selected.
		if (B->GetSelectedLane() == MetaRoad::ZeroLaneIndex)
		{
			if (URoadSplineComponent* Spline = B->GetPreviewSpline())
			{
				DrawUtils::DrawLaneBorder(PDI, Spline, /*SectionIndex=*/0, MetaRoad::ZeroLaneIndex,
					FMetaRoadColors::SelectedColor, FMetaRoadColors::SelectedColor,
					SDPG_Foreground, 4.0f, 0.0f, /*bScreenSpace=*/true);
			}
		}
	}

	void SetContextMenuHandler(FOnLaneContextMenu Handler) { OnContextMenuRequested = Handler; }

private:
	FAdvancedPreviewScene* AdvancedScene = nullptr;
	TWeakObjectPtr<URoadProfilePreviewBuilder> Builder;
	bool bPendingFrame = false;
	FOnLaneContextMenu OnContextMenuRequested;
};

// -----------------------------------------------------------------------------------------------------
// SRoadProfileViewport
// -----------------------------------------------------------------------------------------------------

void SRoadProfileViewport::Construct(const FArguments& InArgs)
{
	PreviewScene = MakeShared<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues());
	// Hide the advanced-preview ground plane (the big plane at Z=0) — the road is what we preview.
	PreviewScene->SetFloorVisibility(false);
	SEditorViewport::Construct(SEditorViewport::FArguments());
}

SRoadProfileViewport::~SRoadProfileViewport()
{
}

TSharedRef<FEditorViewportClient> SRoadProfileViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShared<FRoadProfileViewportClient>(PreviewScene.Get(), SharedThis(this));
	return ViewportClient.ToSharedRef();
}

UWorld* SRoadProfileViewport::GetPreviewWorld() const
{
	return PreviewScene.IsValid() ? PreviewScene->GetWorld() : nullptr;
}

void SRoadProfileViewport::SetPreviewBuilder(URoadProfilePreviewBuilder* Builder)
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->SetBuilder(Builder);
	}
	if (Builder)
	{
		Builder->OnPreviewUpdated.AddSP(SharedThis(this), &SRoadProfileViewport::OnBuilderUpdated);
	}
}

void SRoadProfileViewport::SetContextMenuHandler(FOnLaneContextMenu Handler)
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->SetContextMenuHandler(Handler);
	}
}

void SRoadProfileViewport::OnBuilderUpdated()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->Invalidate();
	}
}
