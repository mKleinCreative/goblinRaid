/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "RoadSplineComponent.h"
#include "BoxTypes.h"
#include "RoadSplineEditorComponent.generated.h"

UCLASS()
class METAROADEDITOR_API URoadSplineEditorComponent : public URoadSplineComponent
{
	GENERATED_BODY()

private:
	FSplineCurves SplinesCurves2d;
	UE::Geometry::FAxisAlignedBox2d SplineBounds{}; // X - SOffset, Y - ROffset

	TWeakObjectPtr<URoadSplineComponent> OriginSpline;

	FString OriginSplineName;

public:

	UE::Geometry::FAxisAlignedBox2d& GetSplineBounds() { return SplineBounds; }
	const UE::Geometry::FAxisAlignedBox2d GetSplineBounds() const { return SplineBounds; }

	URoadSplineComponent* GetOriginSpline() const { return OriginSpline.Get(); }
	const FString& GetOriginSplineName() const { return OriginSplineName; }

	void CloneFrom(const URoadSplineComponent* Other);
	void Release();

	void UpdateSplinesCurves2d();

	FRoadPosition UpRayIntersection(const FVector2D& WorldOrigin) const;

	virtual ~URoadSplineEditorComponent();
};

class METAROADEDITOR_API FRoadSplineEditorComponentPool
{
public:
	static FRoadSplineEditorComponentPool& Get();

	// Game thread only
	TStrongObjectPtr<URoadSplineEditorComponent> Acquire(const URoadSplineComponent* Source);

	// Thread-safe — no UObject access, only pointer moves under mutex
	void Release(TArray<TStrongObjectPtr<URoadSplineEditorComponent>>&& Items);

private:
	FRoadSplineEditorComponentPool() = default;
	FCriticalSection Mutex;
	TArray<TStrongObjectPtr<URoadSplineEditorComponent>> FreeList;
	int32 TotalCreated = 0;
};
