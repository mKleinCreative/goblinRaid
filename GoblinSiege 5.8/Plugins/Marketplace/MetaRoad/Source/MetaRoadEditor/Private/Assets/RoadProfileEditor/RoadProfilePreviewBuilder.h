/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RoadMeshBuild/IRoadMeshBuildHost.h"
#include "RoadProfilePreviewBuilder.generated.h"

class URoadProfile;
class URoadSplineComponent;
class AMetaRoad;
class UMetaRoadBuildSettings;
class UWorld;
class UMaterialInterface;

namespace MetaRoad { class FRoadComputePipeline; }

/** What the Road Profile preview viewport displays. */
enum class ERoadProfilePreviewMode : uint8
{
	RoadGraph,      // only the road spline, no generated mesh
	GeneratedMesh,  // only the generated mesh
};

/**
 * URoadProfilePreviewBuilder
 *
 * Drives the road-mesh compute pipeline (FRoadComputePipeline) inside the Road Profile asset
 * editor's preview scene, decoupled from the interactive UTriangulateRoadTool via IRoadMeshBuildHost.
 *
 * Builds a short straight road from the edited URoadProfile and runs the same background-compute
 * preview path the tool uses; the resulting UDynamicMeshComponent renders in the preview viewport.
 * No assets are written to disk (GenerateAssets()/Shutdown() are never called).
 */
UCLASS()
class URoadProfilePreviewBuilder : public UObject, public IRoadMeshBuildHost
{
	GENERATED_BODY()

public:
	/** Broadcast whenever a preview mesh layer updated — bound by the viewport client to redraw. */
	DECLARE_MULTICAST_DELEGATE(FOnPreviewUpdated);
	FOnPreviewUpdated OnPreviewUpdated;

	/** Broadcast whenever the selected lane changes — bound by the editor to refresh the Details tab. */
	DECLARE_MULTICAST_DELEGATE(FOnSelectionChanged);
	FOnSelectionChanged OnSelectionChanged;

	/** The (transient) build settings holder on the preview actor — used by presets for capture/apply.
	 *  Not serialized (the preview actor is transient). */
	UMetaRoadBuildSettings* GetBuildSettings() const;

	/** Set the preview-scene world and create the default triangulation properties. */
	void Initialize(UWorld* InWorld);

	/** (Re)apply the profile to the preview spline and (re)build the mesh. Safe to call repeatedly. */
	void BuildFromProfile(URoadProfile* Profile);

	/** Synchronous variant of BuildFromProfile: runs the whole pipeline on the calling thread (no
	 *  background threading / ticks). Used by URoadProfileThumbnailRenderer, whose Draw() is synchronous. */
	void BuildSynchronousFromProfile(URoadProfile* Profile);

	/** World-space bounds of all generated preview mesh layers (for thumbnail camera framing). */
	FBox GetGeneratedBounds() const;

	/** Drive the compute pipeline one frame. Call from the viewport client's Tick. */
	void Tick(float DeltaTime);

	/** Switch what the viewport shows: the road spline (RoadGraph) or the generated mesh. */
	void SetPreviewMode(ERoadProfilePreviewMode Mode);

	// --- Lane selection (only meaningful in RoadGraph mode) ---
	// NoSelection = nothing; MetaRoad::ZeroLaneIndex (0) = center/reference; +-N = a Left/Right lane.
	// NOTE: do NOT use INDEX_NONE (-1) as the "nothing" sentinel — it collides with the first left lane (-1).
	static constexpr int32 NoSelection = MIN_int32;

	void SetSelectedLane(int32 LaneIndex);
	int32 GetSelectedLane() const { return SelectedLaneIndex; }

	URoadSplineComponent* GetPreviewSpline() const { return PreviewSpline; }
	URoadProfile* GetCurrentProfile() const { return CurrentProfile.Get(); }

	// IRoadMeshBuildHost
	virtual UWorld* GetTargetWorld() const override;
	virtual void NotifyMeshUpdated() override;
	virtual UMaterialInterface* GetWorkingMaterial() override { return nullptr; }

	// UObject
	virtual void BeginDestroy() override;

private:
	void EnsurePreviewActor();

	/** Common build setup: validate, store the profile, ensure the preview actor, apply the profile to the
	 *  spline. Returns false if the preview can't be built. Shared by Build*FromProfile. */
	bool PrepareSpline(URoadProfile* Profile);

	/** Create the compute pipelines from the preview actor if they don't exist yet. */
	void EnsurePipelines();

	/** Apply the current PreviewMode to the spline + generated-mesh visibility. */
	void ApplyPreviewMode();

	/** Push SelectedLaneIndex to the preview spline so its scene proxy highlights the lane. */
	void ApplySelectionToSpline();

	/** Straight preview reference-line length, cm. */
	static constexpr double PreviewLengthCm = 2000.0;

	ERoadProfilePreviewMode PreviewMode = ERoadProfilePreviewMode::RoadGraph;

	int32 SelectedLaneIndex = NoSelection;

	/** Last profile applied; used to (re)build the mesh when switching back to Generated Mesh mode. */
	UPROPERTY()
	TWeakObjectPtr<URoadProfile> CurrentProfile;

	UPROPERTY()
	TWeakObjectPtr<UWorld> PreviewWorld;

	UPROPERTY()
	TObjectPtr<AMetaRoad> PreviewActor;

	UPROPERTY()
	TObjectPtr<URoadSplineComponent> PreviewSpline;
};
