/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadMeshBuild/RoadSplineEditorComponent.h"
#include "MetaRoadModule.h"
#include "UObject/Package.h"

URoadSplineEditorComponent::~URoadSplineEditorComponent()
{
}

void URoadSplineEditorComponent::CloneFrom(const URoadSplineComponent* Other)
{
	OriginSpline = const_cast<URoadSplineComponent*>(Other);
	OriginSplineName = Other->GetName();
	SplineCurves = Other->SplineCurves;
	DefaultUpVector = Other->DefaultUpVector;
	ReparamStepsPerSegment = Other->ReparamStepsPerSegment;
	bStationaryEndpoints = Other->bStationaryEndpoints;
	RoadLayout = Other->RoadLayout;
	RoadLayout.bDontCreateLaneConnections = true;
	bSkipProceduralGeneration = Other->bSkipProceduralGeneration;
	MaterialPriority = Other->MaterialPriority;
	Bounds = Other->Bounds;
	bComputeFastLocalBounds = true;
	SetClosedLoop(Other->IsClosedLoop(), false);

	SetWorldTransform(Other->GetComponentTransform());
	//UpdateBounds();
	UpdateSpline();
	UpdateRoadLayout();

}

void URoadSplineEditorComponent::Release()
{
	OriginSpline.Reset();
	SplineCurves = {};
	SplinesCurves2d = {};
	RoadLayout = {};
}

void URoadSplineEditorComponent::UpdateSplinesCurves2d()
{
	SplinesCurves2d = SplineCurves;
	for (auto& Pt : SplinesCurves2d.Position.Points)
	{
		Pt.OutVal.Z = 0;
		Pt.ArriveTangent.Z = 0;
		Pt.LeaveTangent.Z = 0;
	}
	SplinesCurves2d.UpdateSpline(IsClosedLoop(), bStationaryEndpoints, ReparamStepsPerSegment, false, 0.0, GetComponentToWorld().GetScale3D());
}

FRoadPosition URoadSplineEditorComponent::UpRayIntersection(const FVector2D& WorldOrigin) const
{
	float Dummy;
	const double Key = SplinesCurves2d.Position.FindNearest(GetComponentToWorld().InverseTransformPosition(FVector(WorldOrigin.X, WorldOrigin.Y, 0.0)), Dummy);
	const FTransform WorldKeyTransform = GetTransformAtSplineInputKey(Key, ESplineCoordinateSpace::World);

	FVector WorldPos = FMath::RayPlaneIntersection(
		FVector{ WorldOrigin.X, WorldOrigin.Y, WorldKeyTransform.GetLocation().Z - 10000.0 },
		FVector{ 0.0, 0.0, -1.0 },
		FPlane(WorldKeyTransform.GetLocation(), WorldKeyTransform.GetRotation().GetUpVector()));

	const FVector LocalPos = WorldKeyTransform.InverseTransformPositionNoScale(WorldPos);

	FRoadPosition Ret;
	Ret.SOffset = GetDistanceAlongSplineAtSplineInputKey(Key);
	Ret.ROffset = LocalPos.Y;
	Ret.Quat = WorldKeyTransform.GetRotation();
	Ret.Location = WorldPos;
	return Ret;
}

FRoadSplineEditorComponentPool& FRoadSplineEditorComponentPool::Get()
{
	static FRoadSplineEditorComponentPool Instance;
	return Instance;
}

TStrongObjectPtr<URoadSplineEditorComponent> FRoadSplineEditorComponentPool::Acquire(const URoadSplineComponent* Source)
{
	TStrongObjectPtr<URoadSplineEditorComponent> Result;
	{
		FScopeLock Lock(&Mutex);
		if (FreeList.Num() > 0)
		{
			Result = MoveTemp(FreeList.Last());
			FreeList.RemoveAt(FreeList.Num() - 1, 1, EAllowShrinking::No);
		}
	}
	if (!Result.IsValid())
	{
		Result = TStrongObjectPtr<URoadSplineEditorComponent>(NewObject<URoadSplineEditorComponent>(GetTransientPackage(), NAME_None, RF_Transient));
		ensureMsgf(++TotalCreated <= 1000, TEXT("FRoadSplineEditorComponentPool: over 1000 components created — possible memory leak"));
	}
	Result->CloneFrom(Source);
	return Result;
}

void FRoadSplineEditorComponentPool::Release(TArray<TStrongObjectPtr<URoadSplineEditorComponent>>&& Items)
{
	FScopeLock Lock(&Mutex);
	FreeList.Append(MoveTemp(Items));
}
