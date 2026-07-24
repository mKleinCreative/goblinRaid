/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RoadMeshBuild/MetaRoadBuildSettingsBase.h"
#include "MetaRoadTypes.h" // FRoadZoneType (was pulled transitively via the old tool header)
//#include "RoadMeshBuild/SplineMeshOpHelpers.h"
#include "RoadMeshBuild/RoadMeshOpHelper.h"
#include "PhysicsEngine/BodyInstance.h"
#include "ToolPropertySets.generated.h"

// ===================== Triangulation properties (base op) =====================

UENUM(BlueprintType)
enum class ECreateRoadObjectType : uint8
{
	StaticMesh = 0,
	DynamicMesh = 1
};

UENUM()
enum class ERoadOverlapStrategy : uint8
{
	UseMaxZ = 0,
	UseMinZ = 1,
	// Snap every generated vertex onto the landscape / world geometry beneath it via a downward line trace.
	SnapToGround = 2 UMETA(DisplayName = "Snap to Ground"),
};

/**
 * Parameters for controlling the spline triangulation
 */
UCLASS()
class METAROADEDITOR_API UTriangulateRoadToolProperties
	: public UMetaRoadBuildSettingsBase
{
	GENERATED_BODY()
public:

	UTriangulateRoadToolProperties() {}

	// Split the road(s) into several components, placing each road section in a separate component.
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (RebuildAll))
	bool bSplitBySections = false;

	// If the SplitBySections is set, then road sections smaller than the MergeSectionsAreaThreshold will be merged with adjacent ones [m^2].
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (RebuildAll, EditCondition="bSplitBySections"))
	double MergeSectionsAreaThreshold = 100;

	// How far to allow the triangulation boundary can deviate from the spline curve before we add more vertices [cm]
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0.1, ClampMax = 100, RebuildAll), AdvancedDisplay)
	double ErrorTolerance = 5.0;

	// How far to allow the triangulation boundary can deviate from the aidewalk cap curve before we add more vertices [cm]
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0.1, ClampMax = 100, RebuildAll), AdvancedDisplay)
	double SidewalkCapErrorTolerance = 2.0;

	// Minimum length of the spline segment into which it will be divided
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0.1, RebuildAll))
	double MinSegmentLength = 375;

	// Points within this tolerance are merged
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0.001, ClampMax = 100, RebuildAll), AdvancedDisplay)
	double VertexSnapTol = 0.01;

	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0.0001, ClampMax = 10, RebuildAll))
	double UV0Scale = 0.0025;

	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0.0001, ClampMax = 10, RebuildAll))
	double UV1Scale = 0.001;

	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0.0001, ClampMax = 10, RebuildAll))
	double UV2Scale = 0.001;

	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 1, ClampMax = 100, RebuildAll))
	int UVMaxSize = 13;

	// How to determine the height of the road surface if several spline pass over the same surface
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (RebuildAll))
	ERoadOverlapStrategy OverlapStrategy = ERoadOverlapStrategy::UseMaxZ;

	// Radius of computing of road surface height in case of intersection of several spline. See OverlapStrategy
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0, ClampMax = 5000, RebuildAll))
	double OverlapRadius = 500;

	// Collision channel the Snap to Ground trace tests against (landscape & world geometry under the road).
	UPROPERTY(EditAnywhere, Category = Mesh,
		meta = (EditCondition = "OverlapStrategy == ERoadOverlapStrategy::SnapToGround", RebuildAll))
	TEnumAsByte<ECollisionChannel> SnapTraceChannel = ECC_WorldStatic;

	// How far above each vertex the downward Snap to Ground trace starts [cm].
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0, ClampMax = 100000,
		EditCondition = "OverlapStrategy == ERoadOverlapStrategy::SnapToGround", RebuildAll))
	double SnapTraceHeightAbove = 1000;

	// How far below each vertex the downward Snap to Ground trace reaches [cm].
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0, ClampMax = 1000000,
		EditCondition = "OverlapStrategy == ERoadOverlapStrategy::SnapToGround", RebuildAll))
	double SnapTraceDepthBelow = 10000;

	// Vertical offset applied above the hit point [cm] (negative sinks the road into the ground).
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = -1000, ClampMax = 1000,
		EditCondition = "OverlapStrategy == ERoadOverlapStrategy::SnapToGround", RebuildAll))
	double SnapGroundOffset = 0;

	// Trace against complex (per-poly) collision. Off = simple collision (faster).
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (
		EditCondition = "OverlapStrategy == ERoadOverlapStrategy::SnapToGround", RebuildAll), AdvancedDisplay)
	bool bSnapTraceComplex = true;

	// Re-smooth the surface (Z) after snapping, so the road follows the terrain gently instead of
	// capturing every bump. Reuses the existing CotanSmoothing path.
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (
		EditCondition = "OverlapStrategy == ERoadOverlapStrategy::SnapToGround", RebuildAll))
	bool bSmoothAfterSnap = false;

	UPROPERTY(EditAnywhere, Category = Mesh, meta = (RebuildAll))
	bool bSmooth = true;

	/** Smoothing speed */
	//UPROPERTY(EditAnywhere, Category = Mesh, meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	UPROPERTY(meta=(EditCondition = "bSmooth", RebuildAll))
	float SmoothSpeed = 0.1f;

	/** Desired Smoothness. This is not a linear quantity, but larger numbers produce smoother results */
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "100.0", EditCondition = "bSmooth", RebuildAll))
	float Smoothness = 0.5f;

	UPROPERTY(EditAnywhere, Category = Mesh)
	ECreateRoadObjectType ObjectType = ECreateRoadObjectType::StaticMesh;

	/** Assign overrides material anyway, ignoring FRoadZone::OverrideMaterial. Mainly used for debug purposes. */
	UPROPERTY(EditAnywhere, Category = Mesh, AdvancedDisplay)
	bool bForceAssignMaterial = false;
};

// -------------------------------------------------------------------------------------------------------------

UINTERFACE()
class METAROADEDITOR_API UInteractiveToolPropertyMaterialInterface : public UInterface
{
	GENERATED_BODY()
};

class METAROADEDITOR_API IInteractiveToolPropertyMaterialInterface
{
	GENERATED_BODY()

public:
	virtual TMap<FName, TObjectPtr<UMaterialInterface>> GetMaterialsMap() const = 0;
};

// -------------------------------------------------------------------------------------------------------------

UINTERFACE()
class METAROADEDITOR_API UInteractiveToolPropertyPhysInterface : public UInterface
{
	GENERATED_BODY()
};

class METAROADEDITOR_API IInteractiveToolPropertyPhysInterface
{
	GENERATED_BODY()

public:
	virtual const FBodyInstance&  GetBodyInstance() const = 0;
};

// -------------------------------------------------------------------------------------------------------------

/**
 * URoadSurfaceToolProperties 
 */
UCLASS()
class METAROADEDITOR_API URoadSurfaceToolProperties 
	: public UMetaRoadBuildSettingsBase
	, public IInteractiveToolPropertyMaterialInterface
	, public IInteractiveToolPropertyPhysInterface
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, Category = DriveSurface, meta = (RebuildDriveSurface))
	bool bBuild = true;

	UPROPERTY(EditAnywhere, Category = DriveSurface, NonTransactional, meta = (EditCondition = "bBuild", RebuildDriveSurface))
	TMap<FRoadZoneType, TObjectPtr<UMaterialInterface>> OverrideMaterials;

	UPROPERTY(EditAnywhere, Category = DriveSurface, NonTransactional, meta = (EditCondition = "bBuild", RebuildDriveSurface))
	FRoadZoneType DriveSurfaceIslandMaterial = ERoadZoneTypes::Driving;

	UPROPERTY(EditAnywhere, Category = DriveSurface, NonTransactional, meta = (EditCondition = "bBuild", RebuildDriveSurface))
	FColor DefaultVertexColor = FColor::White;

	UPROPERTY(EditAnywhere, Category = DriveSurface, NonTransactional, meta = (EditCondition = "bBuild", RebuildDriveSurface))
	FColor EdgeVertexColor = FColor::Black;

	UPROPERTY(EditAnywhere, Category = DriveSurface, NonTransactional, meta = (EditCondition = "bBuild", RebuildDriveSurface))
	bool bComputVertexColor = true;

	UPROPERTY(EditAnywhere, Category = DriveSurface, NonTransactional, meta = (EditCondition = "bBuild && bComputVertexColor", RebuildDriveSurface))
	double VertexColorSmoothRadius = 200;

	UPROPERTY(EditAnywhere, Category = DriveSurface, NonTransactional, meta = (EditCondition = "bBuild", RebuildDriveSurface))
	FBodyInstance BodyInstance;

	virtual TMap<FName, TObjectPtr<UMaterialInterface>> GetMaterialsMap() const override;
	virtual const FBodyInstance& GetBodyInstance() const override { return BodyInstance; }
};

/**
 * URoadDecalToolProperties
 */
UCLASS()
class METAROADEDITOR_API URoadDecalToolProperties 
	: public UMetaRoadBuildSettingsBase
	, public IInteractiveToolPropertyMaterialInterface
	, public IInteractiveToolPropertyPhysInterface
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Decales, meta = (RebuildDecals))
	bool bBuild = true;

	UPROPERTY(EditAnywhere, Category = Decales, NonTransactional, meta = (EditCondition = "bBuild", RebuildDecals))
	TMap<FRoadZoneType, TObjectPtr<UMaterialInterface>> OverrideMaterials;

	// [cm]
	UPROPERTY(EditAnywhere, Category = Decales, meta = (ClampMin = 0.0, ClampMax = 100, EditCondition = "bBuild", RebuildDecals))
	double DecalOffset = 3;

	UPROPERTY(EditAnywhere, Category = Decales, NonTransactional, meta = (EditCondition = "bBuild", RebuildDecals))
	FBodyInstance BodyInstance;

	virtual TMap<FName, TObjectPtr<UMaterialInterface>> GetMaterialsMap() const override;
	virtual const FBodyInstance& GetBodyInstance() const override { return BodyInstance; }
};

/**
 * URoadSidewalkToolProperties
 */
UCLASS()
class METAROADEDITOR_API URoadSidewalkToolProperties
	: public UMetaRoadBuildSettingsBase
	, public IInteractiveToolPropertyMaterialInterface
	, public IInteractiveToolPropertyPhysInterface
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Sidewalks, meta = (RebuildSidewalks))
	bool bBuild = true;

	UPROPERTY(EditAnywhere, Category = Sidewalks, NonTransactional, meta = (EditCondition = "bBuild", RebuildSidewalks))
	TMap<FRoadZoneType, TObjectPtr<UMaterialInterface>> OverrideMaterials;

	UPROPERTY(EditAnywhere, Category = Sidewalks, NonTransactional, meta = (EditCondition = "bBuild", RebuildSidewalks))
	FBodyInstance BodyInstance;

	virtual TMap<FName, TObjectPtr<UMaterialInterface>> GetMaterialsMap() const override;
	virtual const FBodyInstance& GetBodyInstance() const override { return BodyInstance; }
};

/**
 * URoadCertbToolProperties
 */
UCLASS()
class METAROADEDITOR_API URoadCertbToolProperties
	: public UMetaRoadBuildSettingsBase
	, public IInteractiveToolPropertyMaterialInterface
	, public IInteractiveToolPropertyPhysInterface
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Curbs, meta = (RebuildCurbs), meta = (RebuildCurbs))
	bool bBuild = true;

	// Override default curb material
	UPROPERTY(EditAnywhere, Category = Curbs, NonTransactional, meta = (EditCondition = "bBuild", RebuildCurbs))
	TObjectPtr<UMaterialInterface> OverrideMaterial;

	UPROPERTY(EditAnywhere, Category = Curbs, meta = (ClampMin = 0.0, ClampMax = 100, EditCondition = "bBuild", RebuildCurbs))
	double CurbsUV0Scale = 0.002;

	UPROPERTY(EditAnywhere, Category = Curbs, NonTransactional, meta = (EditCondition = "bBuild", RebuildCurbs))
	FBodyInstance BodyInstance;

	virtual TMap<FName, TObjectPtr<UMaterialInterface>> GetMaterialsMap() const override;
	virtual const FBodyInstance& GetBodyInstance() const override { return BodyInstance; }
};

/**
 * URoadMarkToolProperties
 */
UCLASS()
class METAROADEDITOR_API URoadMarkToolProperties
	: public UMetaRoadBuildSettingsBase
	, public IInteractiveToolPropertyMaterialInterface
	, public IInteractiveToolPropertyPhysInterface
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Marks, meta = (RebuildMarks))
	bool bBuild = true;

	UPROPERTY(EditAnywhere, Category = Marks, NonTransactional, meta = (EditCondition = "bBuild", RebuildMarks))
	TObjectPtr<UMaterialInterface> OverrideMaterial;

	// [cm]
	UPROPERTY(EditAnywhere, Category = Marks, meta = (ClampMin = 0.0, ClampMax = 100, EditCondition = "bBuild", RebuildMarks))
	double MarkOffset = 3;

	UPROPERTY(EditAnywhere, Category = Marks, NonTransactional, meta = (EditCondition = "bBuild", RebuildMarks))
	FBodyInstance BodyInstance;

	virtual TMap<FName, TObjectPtr<UMaterialInterface>> GetMaterialsMap() const override;
	virtual const FBodyInstance& GetBodyInstance() const override { return BodyInstance; }
};

/**
 * ULoftingToolProperties
 */
UCLASS()
class METAROADEDITOR_API ULoftingToolProperties
	: public UMetaRoadBuildSettingsBase
	, public IInteractiveToolPropertyMaterialInterface
	, public IInteractiveToolPropertyPhysInterface
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(EditAnywhere, Category = Lofting, meta = (RebuildLofting))
	bool bBuild = true;

	UPROPERTY(EditAnywhere, Category = Lofting, NonTransactional, meta = (EditCondition = "bBuild", RebuildLofting))
	TObjectPtr<UMaterialInterface> OverrideMaterial;

	UPROPERTY(EditAnywhere, Category = Lofting, NonTransactional, meta = (EditCondition = "bBuild", RebuildLofting))
	FBodyInstance BodyInstance;

	virtual TMap<FName, TObjectPtr<UMaterialInterface>> GetMaterialsMap() const override;
	virtual const FBodyInstance& GetBodyInstance() const override { return BodyInstance; }
};

/**
 * URoadAttributesToolProperties
 */
UCLASS()
class METAROADEDITOR_API URoadAttributesToolProperties
	: public UMetaRoadBuildSettingsBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Attributes, meta = (RebuildAttributes))
	bool bBuild = true;

	/** Draw debug referance splines for  the spline meshes*/
	UPROPERTY(EditAnywhere, Category = Attributes, meta = (EditCondition = "bBuild", RebuildAttributes))
	bool bDrawRefSplines = false;
};

/**
 * URoadGraphToolProperties
 */
UCLASS()
class METAROADEDITOR_API URoadGraphToolProperties
	: public UMetaRoadBuildSettingsBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = RoadGraph, meta = (RebuildAttributes))
	bool bBuild = true;

	UPROPERTY(EditAnywhere, Category = RoadGraph, meta = (EditCondition = "bBuild", RebuildAttributes))
	double ZOffset = 0;

	UPROPERTY(EditAnywhere, Category = RoadGraph, meta = (EditCondition = "bBuild", RebuildAttributes))
	double MaxSquareDistanceFromSpline = 100;

	UPROPERTY(EditAnywhere, Category = RoadGraph, meta = (EditCondition = "bBuild", RebuildAttributes))
	double MinSegmentLength = 300;
};
