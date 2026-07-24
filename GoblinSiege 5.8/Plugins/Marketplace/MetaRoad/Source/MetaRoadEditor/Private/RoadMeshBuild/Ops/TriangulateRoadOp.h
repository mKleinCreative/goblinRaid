/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "RoadSplineComponent.h"
#include "RoadGraphComponent.h"
#include "CompGeom/Delaunay2.h"
#include "ModelingOperators.h"
#include "Curve/GeneralPolygon2.h"

#include "Utils/OpUtils.h"


#include "RoadMeshBuild/SplineMeshOpHelpers.h"
#include "RoadMeshBuild/GenericDataBackgroundCompute.h"
// These four were previously pulled in transitively via the (now-removed) TriangulateRoadTool.h.
#include "RoadMeshBuild/ToolPropertySets.h" // UTriangulateRoadToolProperties + ERoadOverlapStrategy etc.
#include "RoadMeshBuild/RoadTriangulationData.h"
#include "RoadMeshBuild/RoadComputePipeline.h"
#include "RoadMeshBuild/IRoadMeshBuildHost.h"
//#include "TriangulateRoadOp.generated.h"


class URoadSplineComponent;
class AMetaRoad;

using namespace UE::Geometry;

namespace MetaRoad 
{



/**
 * FRoadTriangulationOp
 */
class FRoadTriangulationOp : public TGenericDataOperator<FRoadTriangulationData>
{
public:
	FRoadTriangulationOp() = default;
	virtual ~FRoadTriangulationOp();

	// Downstream parameters forwarded verbatim into FRoadTriangulationData::Params.
	FRoadTriangulationParams Params;

	// Build-only parameters (used during CalculateResult, not stored in the result).
	ERoadOverlapStrategy OverlapStrategy = ERoadOverlapStrategy::UseMaxZ;
	double OverlapRadius = 500;
	double SidewalkCapToleranceSq = 1.0;
	double VertexSnapTol = 0.01;
	bool bSmooth = true;
	float SmoothSpeed = 0.1f;
	float Smoothness = 0.5f;
	bool bDrawBoundaries = false;

public:
	void SetActorWithRoads(const AMetaRoad* Actor,
	                       const TArray<TWeakObjectPtr<URoadSplineComponent>>& SplineFilter = {});

public:
	virtual void CalculateResult(FProgressCancel* Progress) override;
};

class FDynamicMeshWithMaterialsOperator : public FDynamicMeshOperator
{
public:
	FDynamicMeshWithMaterialsOperator() = default;

	TArray<TPair<FName, TWeakObjectPtr<UMaterialInterface>>> ResultMaterialSlots;
};

/**
 * FDriveSurfaceOp
 */
class FDriveSurfaceOp : public FDynamicMeshWithMaterialsOperator
{
public:
	FDriveSurfaceOp() = default;

	TSharedPtr<FRoadTriangulationData> BaseData;
	FRoadZoneType DriveSurfaceIslandMaterial = ERoadZoneTypes::Driving;
	bool bComputVertexColor = true;
	double VertexColorSmoothRadius = 200;
	FColor DefaultVertexColor = FColor::White;
	FColor EdgeVertexColor = FColor::Black;
	bool bSplitBySections = false;
	double MergeSectionsAreaThreshold = 25 * 100 * 100;

	virtual void CalculateResult(FProgressCancel* Progress) override;
};

/**
 * FDecalsOp
 */
class FDecalsOp : public FDynamicMeshWithMaterialsOperator
{
public:
	FDecalsOp() = default;

	TSharedPtr<FRoadTriangulationData> BaseData;
	//double VScaleFactor = 0.0025;
	double DecalOffset = 3;
	TSet<FRoadZoneType> OverridesMaterials;
	bool bSplitBySections = false;
	double MergeSectionsAreaThreshold = 25 * 100 * 100;

	virtual void CalculateResult(FProgressCancel* Progress) override;
};

/**
 * FLoftingOp
 */
class FLoftingOp : public FDynamicMeshWithMaterialsOperator
{
public:
	FLoftingOp() = default;

	TSharedPtr<FRoadTriangulationData> BaseData;
	TObjectPtr<UMaterialInterface> OverrideMaterial = nullptr;
	double ChordToleranceSq = 1.0;
	double MinSegmentLength = 375.0;

	virtual void CalculateResult(FProgressCancel* Progress) override;
};

/**
 * FSidewalksOp
 */
class FSidewalksOp : public FDynamicMeshWithMaterialsOperator
{
public:
	FSidewalksOp() = default;

	TSharedPtr<FRoadTriangulationData> BaseData;;
	bool bSplitBySections = false;
	double MergeSectionsAreaThreshold = 25 * 100 * 100;

	virtual void CalculateResult(FProgressCancel* Progress) override;
};

/**
 * FCurbsOp
 */
class FCurbsOp : public FDynamicMeshWithMaterialsOperator
{
public:
	FCurbsOp() = default;

	TSharedPtr<FRoadTriangulationData> BaseData;
	double UV0Scale = 0.001;
	TObjectPtr<UMaterialInterface> OverrideMaterial = nullptr;

	/**
	 * Load every sidewalk's soft CurbProfile so CalculateResult (worker thread) can resolve it via
	 * .Get(). Must be called on the game thread during op setup. See RoadComputeFactoryRegistry
	 * "RoadCurbs" factory.
	 */
	void PreloadProfiles();

	virtual void CalculateResult(FProgressCancel* Progress) override;
};


/**
 * FMarksOp
 */
class FMarksOp : public FDynamicMeshWithMaterialsOperator
{
public:
	FMarksOp() = default;

	TSharedPtr<FRoadTriangulationData> BaseData;
	double MarkOffset = 3;
	TObjectPtr<UMaterialInterface> OverrideMaterial = nullptr;

	/**
	 * Load every mark's soft Profile so CalculateResult (worker thread) can resolve it via .Get().
	 * Must be called on the game thread during op setup (TSoftObjectPtr::LoadSynchronous is not
	 * safe on the compute thread). See RoadComputeFactoryRegistry "RoadMarks" factory.
	 */
	void PreloadProfiles();

	virtual void CalculateResult(FProgressCancel* Progress) override;
};


/**
 * FRefSplineOp
 */

class FSplineMeshOp : public MetaRoad::FSplineMeshOperator
{
public:
	FSplineMeshOp();

	TSharedPtr<FRoadTriangulationData> BaseData;
	double ChordToleranceSq = 1.0;
	double MinSegmentLength = 375;
	bool bDrawRefSplines = false;
	
	// Save links to the profile in the game thread
	TSet<const URoadLaneAttributeGenerateDescriptor*> Profiles;

	virtual void CalculateResult(FProgressCancel* Progress) override;
};

/**
 * FGraphOp
 */
class FGraphOp : public UE::Geometry::TGenericDataOperator<FRoadGraphScope>
{
public:
	FGraphOp() = default;

	TSharedPtr<FRoadTriangulationData> BaseData;
	double ZOffset = 0;
	double MaxSquareDistanceFromSpline = 100;
	double MinSegmentLength = 300;

	virtual void CalculateResult(FProgressCancel* Progress) override;
};


} // MetaRoad
