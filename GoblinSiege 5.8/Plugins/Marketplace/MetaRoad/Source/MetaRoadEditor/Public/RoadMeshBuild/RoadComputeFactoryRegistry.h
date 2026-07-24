/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"

class IRoadOpCompute;
class IRoadMeshBuildHost;
namespace MetaRoad { class FRoadComputePipeline; }

/**
 * FRoadComputeFactoryRegistry
 *
 * Registry of the road-mesh-layer compute factories (one per mesh layer: surface, decals, sidewalks, curbs,
 * marks, lofting, spline-meshes, road-graph). Each factory is an FRoadComputeFactory delegate that, given a
 * host + pipeline scope, produces a ready-to-tick IRoadOpCompute. Extracted from FMetaRoadEditorModule so the
 * RoadMeshBuild framework owns its own registration and the editor module stays a thin coordinator.
 *
 * Process-wide singleton (Get()); RegisterBuiltinFactories() is called once at editor-module startup and the
 * registry is cleared on shutdown. The pipeline (FRoadComputePipeline) reads GetFactories() when building its
 * layer stack.
 */
class METAROADEDITOR_API FRoadComputeFactoryRegistry
{
public:
	DECLARE_DELEGATE_RetVal_TwoParams(IRoadOpCompute*, FRoadComputeFactory, IRoadMeshBuildHost*, TWeakPtr<MetaRoad::FRoadComputePipeline>);

	static FRoadComputeFactoryRegistry& Get();

	// Register the built-in layer factories (surface/decals/sidewalks/curbs/marks/[lofting]/spline-meshes/road-graph).
	void RegisterBuiltinFactories();

	void Register(FName FactoryName, FRoadComputeFactory&& Factory);
	void Unregister(FName FactoryName);
	const TMap<FName, FRoadComputeFactory>& GetFactories() const { return Factories; }

	void Reset() { Factories.Reset(); }

private:
	TMap<FName, FRoadComputeFactory> Factories;
};
