/*
* Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
* Email: ivzhuk7@gmail.com
*/

#include "MetaRoadActor.h"
#include "RoadSplineComponent.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaRoadActor)

#define LOCTEXT_NAMESPACE "MetaRoadActor"

//////////////////////////////////////////////////////////////////////////
// AMetaRoad

AMetaRoad::AMetaRoad(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

#if WITH_EDITOR
void AMetaRoad::CenterSplineOrigins()
{
	TArray<URoadSplineComponent*> Splines;
	GetComponents<URoadSplineComponent>(Splines);
	if (Splines.IsEmpty())
	{
		return;
	}

	FBox CombinedBox(EForceInit::ForceInitToZero);
	for (URoadSplineComponent* Spline : Splines)
	{
		CombinedBox += Spline->CalcBounds(Spline->GetComponentTransform()).GetBox();
	}
	const FVector Center = CombinedBox.GetCenter();

	FScopedTransaction Transaction(LOCTEXT("CenterSplineOrigins", "Center Spline Origins"));
	for (URoadSplineComponent* Spline : Splines)
	{
		Spline->Modify();

		const FTransform OldTransform = Spline->GetComponentTransform();
		const FVector OffsetLocal = OldTransform.InverseTransformVector(
			Spline->GetComponentLocation() - Center);

		for (FInterpCurvePoint<FVector>& Point : Spline->SplineCurves.Position.Points)
		{
			Point.OutVal += OffsetLocal;
		}

		Spline->SetWorldLocation(Center, false, nullptr, ETeleportType::TeleportPhysics);
		Spline->UpdateSpline();
		Spline->MarkRenderStateDirty();
	}
}
#endif

#undef LOCTEXT_NAMESPACE


