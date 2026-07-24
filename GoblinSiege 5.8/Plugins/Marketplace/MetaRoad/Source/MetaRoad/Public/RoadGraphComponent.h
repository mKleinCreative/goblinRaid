/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RoadSplineComponent.h"
#include "RoadGraphComponent.generated.h"

class URoadSplineComponent;

USTRUCT()
struct FRoadGrapPoint
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector Location{};

	UPROPERTY()
	FQuat Quat{};

	UPROPERTY()
	double SOffset{};

	UPROPERTY()
	double Width{};
};

USTRUCT()
struct FRoadGraphLane
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FRoadGrapPoint> Points;

	UPROPERTY()
	TArray<FVector> InnerBorder;

	UPROPERTY()
	TArray<FVector> OuterBorder;

	UPROPERTY()
	FBox Bounds{};

	UPROPERTY()
	FRoadZoneType Type;

	UPROPERTY()
	bool bIsForward{};

	UPROPERTY()
	FZoneGraphTagMask ZoneTags{};
};

USTRUCT()
struct FRoadGraphSection
{
	GENERATED_USTRUCT_BODY()

	//UPROPERTY()
	//int SectionIndex = INDEX_NONE;

	UPROPERTY()
	TArray<FRoadGraphLane> LeftLanes;

	UPROPERTY()
	TArray<FRoadGraphLane> RightLanes;

	UPROPERTY()
	int BoundaryIndex = -1;

};

USTRUCT()
struct FRoadGraphSpline
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TSoftObjectPtr<URoadSplineComponent> RoadSpline;
	
	UPROPERTY()
	TArray<FRoadGraphSection> Sections;

};

USTRUCT()
struct FRoadGraphBoundary
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FVector> Points;
};


USTRUCT()
struct FRoadGraphScope
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FRoadGraphSpline> Splines;

	UPROPERTY()
	FBoxSphereBounds Bounds{};

	UPROPERTY()
	TArray<FRoadGraphBoundary> Boundaries;
};


/** 
 * URoadGraphComponent
 */
UCLASS(BlueprintType, Blueprintable)
class METAROAD_API URoadGraphDataComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY()
	FRoadGraphScope RoadGraph{};

#if WITH_EDITOR
	uint32 GetShapeHash();
#endif

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRegisterComponentChanged, URoadGraphDataComponent*);

	static FOnRegisterComponentChanged OnComponentRegistredDelegate;
	static FOnRegisterComponentChanged OnComponentUnregistredDelegate;


protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	
};
