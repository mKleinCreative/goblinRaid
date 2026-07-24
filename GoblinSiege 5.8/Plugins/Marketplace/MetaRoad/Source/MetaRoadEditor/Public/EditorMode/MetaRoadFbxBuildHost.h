/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RoadMeshBuild/IRoadMeshBuildHost.h"
#include "MetaRoadFbxBuildHost.generated.h"

class UWorld;
class UMaterialInterface;
class AMetaRoad;

/**
 * UMetaRoadFbxBuildHost
 *
 * One-shot synchronous IRoadMeshBuildHost used by the FBX "From scratch" export: it regenerates a road's
 * meshes in RAM (no disk assets, no level mutation) and materializes them onto a freshly-spawned transient
 * actor, which the exporter hands to FFbxExporter and then destroys.
 *
 * WantsTransientMeshOutput() returns true, so GenerateAssets() routes the layer UStaticMeshes to the transient
 * package (see AssetUtils::CreateStaticMeshAsset). Spline-mesh layers reference pre-authored profile assets
 * and are unaffected.
 */
UCLASS(Transient)
class UMetaRoadFbxBuildHost : public UObject, public IRoadMeshBuildHost
{
	GENERATED_BODY()

public:
	/** Build the road's meshes synchronously in RAM and materialize them onto a new transient actor spawned at
	 *  the road's world transform (so the components sit at their level world positions; the exporter applies the
	 *  pivot afterwards). Returns the actor (caller exports then destroys it) or null on failure. */
	AActor* BuildTransientExportActor(UWorld* InWorld, AMetaRoad* Road);

	// IRoadMeshBuildHost
	virtual UWorld* GetTargetWorld() const override { return World.Get(); }
	virtual void NotifyMeshUpdated() override {}
	virtual UMaterialInterface* GetWorkingMaterial() override { return nullptr; }
	virtual bool WantsTransientMeshOutput() const override { return true; }

private:
	UPROPERTY()
	TWeakObjectPtr<UWorld> World;
};
