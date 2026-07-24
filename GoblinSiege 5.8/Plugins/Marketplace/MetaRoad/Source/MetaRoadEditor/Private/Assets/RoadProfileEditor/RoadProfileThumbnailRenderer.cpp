/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadProfileThumbnailRenderer.h"

#include "Assets/RoadProfile.h"
#include "RoadProfilePreviewBuilder.h"

#include "ThumbnailHelpers.h"
#include "RenderingThread.h" // FlushRenderingCommands (no longer transitively included in UE 5.8)
#include "SceneInterface.h"
#include "SceneView.h"
#include "ShowFlags.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RoadProfileThumbnailRenderer)

// =====================================================================================================
// FRoadProfileThumbnailScene
//
// A FThumbnailPreviewScene (same family as the engine's FStaticMeshThumbnailScene) that renders the road
// mesh generated from a URoadProfile. The mesh is produced by the shared road-mesh pipeline driven
// synchronously through URoadProfilePreviewBuilder (a IRoadMeshBuildHost) — identical layers/materials to
// the level tool and the live preview, but on the calling thread because Draw() cannot await async ticks.
// Each pipeline layer fills its own preview mesh component inside this scene's world, so the whole road
// (surface + sidewalks + curbs + marks + ...) renders together.
// =====================================================================================================
class FRoadProfileThumbnailScene : public FThumbnailPreviewScene
{
public:
	FRoadProfileThumbnailScene();

	/** Regenerate the preview mesh for the given profile (no-op if it matches the last one). */
	void SetProfile(URoadProfile* Profile);

protected:
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin,
	                                     float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;

private:
	TStrongObjectPtr<URoadProfilePreviewBuilder> Builder;
	FBoxSphereBounds MeshBounds = FBoxSphereBounds(FVector::ZeroVector, FVector(1.0), 1.0);
};

FRoadProfileThumbnailScene::FRoadProfileThumbnailScene()
	// No floor plane: the flat road mesh sits at Z~=0 and would z-fight a floor at Z=0, "eating" triangles.
	: FThumbnailPreviewScene(FThumbnailPreviewScene::FConstructionValues().SetCreateFloorPlane(false))
{
	Builder = TStrongObjectPtr<URoadProfilePreviewBuilder>(NewObject<URoadProfilePreviewBuilder>());
	Builder->Initialize(GetWorld());
}

void FRoadProfileThumbnailScene::SetProfile(URoadProfile* Profile)
{
	// Always rebuild for the requested profile — do NOT cache by profile identity. The thumbnail pool
	// (FAssetThumbnailPool) already caches the rendered image and only calls Draw() when it needs a fresh
	// render (e.g. the asset's package was dirtied). Caching by identity here renders a stale mesh after an
	// in-place edit (same UObject) and can show one asset's mesh for another if the cache desyncs.
	if (!Profile || !Builder)
	{
		MeshBounds = FBoxSphereBounds(FVector::ZeroVector, FVector(1.0), 1.0);
		return;
	}

	Builder->BuildSynchronousFromProfile(Profile);
	MeshBounds = FBoxSphereBounds(Builder->GetGeneratedBounds());
}

void FRoadProfileThumbnailScene::GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin,
                                                         float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const
{
	const float HalfFOVRadians = FMath::DegreesToRadians<float>(InFOVDegrees) * 0.5f;
	// Slightly larger than the sphere to compensate for perspective (mirrors FStaticMeshThumbnailScene).
	const float HalfMeshSize = static_cast<float>(MeshBounds.SphereRadius * 1.15);
	const float TargetDistance = HalfMeshSize / FMath::Tan(HalfFOVRadians);

	// The generated mesh is not recentered, so translate the view by its world-space bounds center.
	OutOrigin = -MeshBounds.Origin;
	OutOrbitPitch = -40.0f; // look down at the road from above-front
	OutOrbitYaw = -60.0f;
	OutOrbitZoom = FMath::Max(TargetDistance, 10.0f);
}

// =====================================================================================================
// URoadProfileThumbnailRenderer
// =====================================================================================================

bool URoadProfileThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	return Cast<URoadProfile>(Object) != nullptr;
}

void URoadProfileThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height,
                                         FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	URoadProfile* Profile = Cast<URoadProfile>(Object);
	if (!Profile)
	{
		return;
	}

	if (ThumbnailScene == nullptr || ensure(ThumbnailScene->GetWorld() != nullptr) == false)
	{
		if (ThumbnailScene)
		{
			FlushRenderingCommands();
			delete ThumbnailScene;
		}
		ThumbnailScene = new FRoadProfileThumbnailScene();
	}

	ThumbnailScene->SetProfile(Profile);
	ThumbnailScene->GetScene()->UpdateSpeedTreeWind(0.0);

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(RenderTarget, ThumbnailScene->GetScene(),
		FEngineShowFlags(ESFIM_Game))
		.SetTime(UThumbnailRenderer::GetTime())
		.SetAdditionalViewFamily(bAdditionalViewFamily));

	ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
	ViewFamily.EngineShowFlags.MotionBlur = 0;
	ViewFamily.EngineShowFlags.LOD = 0;

	RenderViewFamily(Canvas, &ViewFamily, ThumbnailScene->CreateView(&ViewFamily, X, Y, Width, Height));
}

void URoadProfileThumbnailRenderer::BeginDestroy()
{
	if (ThumbnailScene)
	{
		delete ThumbnailScene;
		ThumbnailScene = nullptr;
	}

	Super::BeginDestroy();
}
