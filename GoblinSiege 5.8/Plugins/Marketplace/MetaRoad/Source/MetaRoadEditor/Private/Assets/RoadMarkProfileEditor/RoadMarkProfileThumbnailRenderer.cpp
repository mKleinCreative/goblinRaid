/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadMarkProfileThumbnailRenderer.h"
#include "Math/BoxSphereBounds.h"
#include "Misc/App.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Engine.h"
#include "Assets/RoadMarkProfile.h"
#include "GlobalRenderResources.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(RoadMarkProfileThumbnailRenderer)

URoadMarkProfileThumbnailRenderer::URoadMarkProfileThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URoadMarkProfileThumbnailRenderer::DrawSolidLane(FCanvas* Canvas, int32 X, int32 Y, uint32 Width, uint32 Height, const FRoadLaneMarkProfileSolid& Profile, float LeftOffset) const
{
	const float CmToPixel = Height * Scale;
	const float MarkWidth = Profile.Width * CmToPixel * MarkWidtScale;

	Canvas->DrawTile(
		Width * 0.5 - MarkWidth * 0.5 + LeftOffset * CmToPixel * MarkWidtScale, Y,
		MarkWidth, Height,
		0.0f, 0.0f, 1.0f, 1.0f,
		Profile.VertexColor,
		GWhiteTexture,
		false);
}

void URoadMarkProfileThumbnailRenderer::DrawBrokedLane(FCanvas* Canvas, int32 X, int32 Y, uint32 Width, uint32 Height, const FRoadLaneMarkProfileBroked& Profile, float LeftOffset) const
{
	const float CmToPixel = Height * Scale;
	const float MarkWidth = Profile.Width * CmToPixel * MarkWidtScale;

	int Num = int(Height / (Profile.Long * CmToPixel + Profile.Gap * CmToPixel) + 0.5);
	if (Num % 2)
	{
		++Num;
	}

	float YOffset = -((Profile.Long + Profile.Gap) * Num * CmToPixel - Profile.Gap * CmToPixel - Height) * 0.5;

	for (int i = 0; i < Num; ++i)
	{

		Canvas->DrawTile(
			Width * 0.5 - MarkWidth * 0.5 + LeftOffset * CmToPixel * MarkWidtScale, (double)YOffset,
			MarkWidth, Profile.Long* CmToPixel,
			0.0f, 0.0f, 1.0f, 1.0f,
			Profile.VertexColor,
			GWhiteTexture,
			false);



		YOffset += (Profile.Long + Profile.Gap) * CmToPixel;
	}
	
}

bool URoadMarkProfileThumbnailRenderer::DrawSolidOrBrokedLane(FCanvas* Canvas, int32 X, int32 Y, uint32 Width, uint32 Height, const TInstancedStruct<FRoadLaneMarkProfile>& Profile, float LeftOffset) const
{
	if (auto* AsSolid = Profile.GetPtr<FRoadLaneMarkProfileSolid>())
	{
		DrawSolidLane(Canvas, X, Y, Width, Height, *AsSolid, LeftOffset);
		return true;
	}
	else if (auto* AsBroked = Profile.GetPtr<FRoadLaneMarkProfileBroked>())
	{
		DrawBrokedLane(Canvas, X, Y, Width, Height, *AsBroked, LeftOffset);
		return true;
	}
	return false;
}


void URoadMarkProfileThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	//FCanvasBoxItem BoxItem(FVector2D(X, Y), FVector2D(Width, Height));
	//BoxItem.SetColor(FColor::Red);

	//Canvas->DrawItem(BoxItem);

	
	Canvas->DrawTile(
		X, Y, Width, Height,
		0.0f, 0.0f, 1.0f, 1.0f,
		FLinearColor::White * 0.0105f,
		GWhiteTexture,
		false);
		

	const float RoadWidth = Width * 0.5;

	Canvas->DrawTile(
		X + Width * 0.5 - RoadWidth * 0.5, 0.0f,
		Y + RoadWidth, Height,
		0.0f, 0.0f, 1.0f, 1.0f,
		FLinearColor::White * 0.2f,
		GWhiteTexture,
		false);



	if (URoadMarkProfile* RoadMarkProfile = Cast<URoadMarkProfile>(Object))
	{
		if (!DrawSolidOrBrokedLane(Canvas, X, Y, Width, Height, RoadMarkProfile->LaneMarkProfiles, 0.0))
		{
			if (auto* AsDouble = RoadMarkProfile->LaneMarkProfiles.GetPtr<FRoadLaneMarkProfileDouble>())
			{
				DrawSolidOrBrokedLane(Canvas, X, Y, Width, Height, AsDouble->Left, -AsDouble->Gap * 0.5);
				DrawSolidOrBrokedLane(Canvas, X, Y, Width, Height, AsDouble->Right, +AsDouble->Gap * 0.5);
			}
		}

		/*
		const double DeltaTime = FApp::GetCurrentTime() - GStartTime;
		const double TotalDuration = Flipbook->GetTotalDuration();
		const float PlayTime = (TotalDuration > 0.0f) ? FMath::Fmod(DeltaTime, TotalDuration) : 0.0f;

		if (UPaperSprite* Sprite = Flipbook->GetSpriteAtTime(PlayTime))
		{
			FBoxSphereBounds FlipbookBounds = Flipbook->GetRenderBounds();
			DrawFrame(Sprite, X, Y, Width, Height, RenderTarget, Canvas, &FlipbookBounds);
			return;
		}
		else
		{
			// Fallback for empty frames or newly created flipbooks
			DrawGrid(X, Y, Width, Height, Canvas);
		}

		if (TotalDuration == 0.0f)
		{
			// Warning text for no frames
			const FText ErrorText = NSLOCTEXT("FlipbookEditorApp", "ThumbnailWarningNoFrames", "No frames");
			FCanvasTextItem TextItem(FVector2D(5.0f, 5.0f), ErrorText, GEngine->GetLargeFont(), FLinearColor::Red);
			TextItem.EnableShadow(FLinearColor::Black);
			TextItem.Scale = FVector2D(Width / 128.0f, Height / 128.0f);
			TextItem.Draw(Canvas);
		}
		*/
	}

}


