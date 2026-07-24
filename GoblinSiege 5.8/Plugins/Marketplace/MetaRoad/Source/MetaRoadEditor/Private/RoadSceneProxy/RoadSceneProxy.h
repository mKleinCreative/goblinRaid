/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RoadSplineComponent.h"
#include "VertexFactory.h"
#include "PrimitiveSceneProxy.h"
#include "LocalVertexFactory.h"
#include "DynamicMeshBuilder.h"
#include "RoadHitProxies.h"   // HRoadSplineVisProxy / HRoadLaneVisProxy hierarchy (used by CreateHitProxies)

namespace MetaRoad
{
	struct FTriProxy;
	struct FLaneProxy;
}


class METAROADEDITOR_API FRoadSplineSceneProxy final : public FPrimitiveSceneProxy
{

public:
	FRoadSplineSceneProxy(URoadSplineComponent* Component);
	FRoadSplineSceneProxy(const FRoadSplineSceneProxy& Component) = delete;
	virtual ~FRoadSplineSceneProxy();

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual bool CanBeOccluded() const override;
	virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }
	virtual SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	uint32 GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }

	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;

private:
	URoadSplineComponent* RoadSpline = nullptr;
	TArray<TSharedPtr<MetaRoad::FLaneProxy>> LanesProxies;
	TSharedPtr<MetaRoad::FTriProxy> TriProxy;
	FMaterialRelevance MaterialRelevance{};
	bool bIsMultiRoad;
	// True only in the main level-editor world, where the editor-mode / selection visibility
	// restriction applies. Elsewhere (preview scenes, PIE, …) the spline always renders.
	bool bIsEditableLevelWorld = false;
	//TArray<TPair<FVector, FVector>> ArrawLines;
};

