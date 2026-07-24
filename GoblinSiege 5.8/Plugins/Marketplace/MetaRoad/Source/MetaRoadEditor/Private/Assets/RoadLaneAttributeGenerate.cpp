/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "Assets/RoadLaneAttributeGenerate.h"
#include "Components/SplineMeshComponent.h"
#include "Utils/AssetUtils.h"
#include "Utils/DrawUtils.h"
#include "RoadSplineComponent.h"
#include "MetaRoadSettings.h"
#include "Engine/World.h"

static float SmoothStep(float A, float B, float X)
{
	if (X < A)
	{
		return 0.0f;
	}
	else if (X >= B)
	{
		return 1.0f;
	}
	const float InterpFraction = (X - A) / (B - A);
	return InterpFraction * InterpFraction * (3.0f - 2.0f * InterpFraction);
}

static FVector SplineEvalPos(const FVector& StartPos, const FVector& StartTangent, const FVector& EndPos, const FVector& EndTangent, float A)
{
	const float A2 = A * A;
	const float A3 = A2 * A;

	return (((2 * A3) - (3 * A2) + 1) * StartPos) + ((A3 - (2 * A2) + A) * StartTangent) + ((A3 - A2) * EndTangent) + (((-2 * A3) + (3 * A2)) * EndPos);
}

static FVector SplineEvalPos(const FReferenceSplineMeshParams& Params, float A)
{
	// TODO: these don't need to be doubles!
	const FVector StartPos = FVector(Params.StartPos);
	const FVector StartTangent = FVector(Params.StartTangent);
	const FVector EndPos = FVector(Params.EndPos);
	const FVector EndTangent = FVector(Params.EndTangent);

	return SplineEvalPos(StartPos, StartTangent, EndPos, EndTangent, A);
}

static FVector SplineEvalTangent(const FVector& StartPos, const FVector& StartTangent, const FVector& EndPos, const FVector& EndTangent, const float A)
{
	const FVector C = (6 * StartPos) + (3 * StartTangent) + (3 * EndTangent) - (6 * EndPos);
	const FVector D = (-6 * StartPos) - (4 * StartTangent) - (2 * EndTangent) + (6 * EndPos);
	const FVector E = StartTangent;

	const float A2 = A * A;

	return (C * A2) + (D * A) + E;
}

static FVector SplineEvalTangent(const FReferenceSplineMeshParams& Params, const float A)
{
	// TODO: these don't need to be doubles!
	const FVector StartPos = FVector(Params.StartPos);
	const FVector StartTangent = FVector(Params.StartTangent);
	const FVector EndPos = FVector(Params.EndPos);
	const FVector EndTangent = FVector(Params.EndTangent);

	return SplineEvalTangent(StartPos, StartTangent, EndPos, EndTangent, A);
}

static FVector SplineEvalDir(const FReferenceSplineMeshParams& Params, const float A)
{
	return SplineEvalTangent(Params, A).GetSafeNormal();
}

FReferenceSplineMeshParams::FReferenceSplineMeshParams(const FSplineMeshParams& Other)
{
	StartPos = Other.StartPos;
	StartTangent = Other.StartTangent;
	StartScale = Other.StartScale;
	StartRoll = Other.StartRoll;
	EndRoll = Other.EndRoll;
	StartOffset = Other.StartOffset;
	EndPos = Other.EndPos;
	EndScale = Other.EndScale;
	EndTangent = Other.EndTangent;
	EndOffset = Other.EndOffset;
}

FReferenceSplineMeshParams::operator FSplineMeshParams() const
{
	FSplineMeshParams Other;
	Other.StartPos = StartPos;
	Other.StartTangent = StartTangent;
	Other.StartScale = StartScale;
	Other.StartRoll = StartRoll;
	Other.EndRoll = EndRoll;
	Other.StartOffset = StartOffset;
	Other.EndPos = EndPos;
	Other.EndScale = EndScale;
	Other.EndTangent = EndTangent;
	Other.EndOffset = EndOffset;
	return Other;
}

double FRoadLaneAttributeGenerateValue::GetCenterLaneROffset(double InLeftR, double InRightR) const
{
	if (Alignment == ERoadLaneAlignment::Auto)
	{
		return (InLeftR + InRightR) * 0.5;
	}
	else
	{
		// Fixed: map Alpha across the road width (base returns 0.0, so call the shared helper directly).
		return MetaRoad::MapCenterLaneROffset(GetKeyAlpha(), InLeftR, InRightR);
	}
}

bool FRoadLaneAttributeGenerateValue::SetAlphaFromROffset(double AnchorR, double LaneInnerR, double LaneOuterR, bool bIsZeroLane)
{
	Alignment = ERoadLaneAlignment::Fixed;
	return FRoadLaneAttributeValue::SetAlphaFromROffset(AnchorR, LaneInnerR, LaneOuterR, bIsZeroLane);
}

bool FRoadLaneAttributeGenerateValue::Interpolate(const UScriptStruct* ValueType, const FRoadLaneAttributeValue* InOther, float AttrAlpha, FRoadLaneAttributeValue* InOut) const
{
	if (ValueType != FRoadLaneAttributeGenerateValue::StaticStruct())
	{
		return false;
	}

	check(InOther && InOut);

	const FRoadLaneAttributeGenerateValue& Other = *static_cast<const FRoadLaneAttributeGenerateValue*>(InOther);
	FRoadLaneAttributeGenerateValue& Out = *static_cast<FRoadLaneAttributeGenerateValue*>(InOut);

	Out.Alpha           = FMath::CubicInterp<float>(Alpha, 0.f, Other.Alpha, 0.f, AttrAlpha);
	Out.Scale           = FMath::CubicInterp<FVector2D>(Scale, FVector2D::ZeroVector, Other.Scale, FVector2D::ZeroVector, AttrAlpha);
	Out.Offset          = FMath::CubicInterp<FVector2D>(Offset, FVector2D::ZeroVector, Other.Offset, FVector2D::ZeroVector, AttrAlpha);
	Out.Roll            = FMath::CubicInterp<float>(Roll, 0.f, Other.Roll, 0.f, AttrAlpha);
	Out.Alignment   = Alignment;   // stepped
	Out.OverrideLeftWidth  = OverrideLeftWidth;  // stepped
	Out.OverrideRightWidth = OverrideRightWidth; // stepped
	Out.LeftWidth          = LeftWidth;          // stepped
	Out.RightWidth         = RightWidth;         // stepped

	return true;
}

FTransform URoadLaneAttributeGenerateDescriptor::CalcSliceTransformAtSplineOffset(const FReferenceSplineMeshParams& SplineParams, const float Alpha, const float MinT, const float MaxT)
{
	static const FVector SplineUpDir = FVector::UpVector;
	static const bool bSmoothInterpRollScale = false;

	// Apply hermite interp to Alpha if desired
	const float HermiteAlpha = bSmoothInterpRollScale ? SmoothStep(0.0, 1.0, Alpha) : Alpha;

	// Then find the point and direction of the spline at this point along
	FVector SplinePos;
	FVector SplineDir;

	// Use linear extrapolation
	if (Alpha < MinT)
	{
		const FVector StartTangent(SplineEvalTangent(SplineParams, MinT));
		SplinePos = SplineEvalPos(SplineParams, MinT) + (StartTangent * (Alpha - MinT));
		SplineDir = StartTangent.GetSafeNormal();
	}
	else if (Alpha > MaxT)
	{
		const FVector EndTangent(SplineEvalTangent(SplineParams, MaxT));
		SplinePos = SplineEvalPos(SplineParams, MaxT) + (EndTangent * (Alpha - MaxT));
		SplineDir = EndTangent.GetSafeNormal();
	}
	else
	{
		SplinePos = SplineEvalPos(SplineParams, Alpha);
		SplineDir = SplineEvalDir(SplineParams, Alpha);
	}

	// Find scale at this point along spline
	const FVector2D UseScale = FMath::Lerp(FVector2D(SplineParams.StartScale), FVector2D(SplineParams.EndScale), HermiteAlpha);

	if (SplineParams.bAlignWorldUpVector)
	{
		FVector SplineDir2D = FVector(SplineDir.X, SplineDir.Y, 0.0).GetSafeNormal();
		return FTransform(
			FRotationMatrix::MakeFromXZ(SplineDir2D, SplineUpDir).ToQuat(),
			SplinePos,
			FVector(1, UseScale.X, UseScale.Y)
		);
	}

	// Find base frenet frame
	const FVector BaseXVec = (SplineUpDir ^ SplineDir).GetSafeNormal();
	const FVector BaseYVec = (SplineDir ^ BaseXVec).GetSafeNormal();

	// Offset the spline by the desired amount
	const FVector2D SliceOffset = FMath::Lerp(SplineParams.StartOffset, SplineParams.EndOffset, HermiteAlpha);
	SplinePos += SliceOffset.X * BaseXVec;
	SplinePos += SliceOffset.Y * BaseYVec;

	// Apply roll to frame around spline
	const float UseRoll = FMath::Lerp(SplineParams.StartRoll, SplineParams.EndRoll, HermiteAlpha);
	const float CosAng = FMath::Cos(UseRoll);
	const float SinAng = FMath::Sin(UseRoll);
	const FVector XVec = (CosAng * BaseXVec) - (SinAng * BaseYVec);
	const FVector YVec = (CosAng * BaseYVec) + (SinAng * BaseXVec);



	// Build overall transform
	FTransform SliceTransform;
	//switch (ForwardAxis)
	//{
	//case ESplineMeshAxis::X:
	SliceTransform = FTransform(SplineDir, XVec, YVec, SplinePos);
	SliceTransform.SetScale3D(FVector(1, UseScale.X, UseScale.Y));
	//	break;
	//case ESplineMeshAxis::Y:
	//	SliceTransform = FTransform(FVector(YVec), FVector(SplineDir), FVector(XVec), FVector(SplinePos));
	//	SliceTransform.SetScale3D(FVector(UseScale.Y, 1, UseScale.X));
	//	break;
	//case ESplineMeshAxis::Z:
	//	SliceTransform = FTransform(FVector(XVec), FVector(YVec), FVector(SplineDir), FVector(SplinePos));
	//	SliceTransform.SetScale3D(FVector(UseScale.X, UseScale.Y, 1));
	//	break;
	//default:
	//	check(0);
	//	break;
	//}

	return SliceTransform;
}

FVector2D URoadLaneAttributeGenerateDescriptor::CalcWidthsAtSplineOffset(const FReferenceSplineMeshParams& SplineParams, const float Alpha, const float MinT, const float MaxT)
{
	const float T = FMath::Clamp((MaxT > MinT) ? (Alpha - MinT) / (MaxT - MinT) : 0.f, 0.f, 1.f);
	return FVector2D(
		FMath::Lerp(SplineParams.StartLeftWidth,  SplineParams.EndLeftWidth,  T),
		FMath::Lerp(SplineParams.StartRightWidth, SplineParams.EndRightWidth, T));
}

void URoadLaneAttributeSplineMeshDescriptor::GenerateAsset(const FReferenceSplineMeshParams& SplineMeshParams, AActor* TargetActor, bool bIsPreview) const
{
	if (StaticMesh)
	{
		USplineMeshComponent* NewComponent = NewObject<USplineMeshComponent>(TargetActor, *AssetUtils::GenerateValidComponentName(GetDisplayName().ToString(), TargetActor), bIsPreview ? RF_NoFlags : RF_Transactional);
		NewComponent->SetupAttachment(TargetActor->GetRootComponent());
		TargetActor->AddInstanceComponent(NewComponent);
		NewComponent->SetMobility(EComponentMobility::Static);
		NewComponent->OnComponentCreated();
		if (bIsPreview)
		{
			NewComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		}
		else
		{
			NewComponent->SetCollisionEnabled(BodyInstance.GetCollisionEnabled());
			NewComponent->SetCollisionObjectType(BodyInstance.GetObjectType());
			NewComponent->SetCollisionResponseToChannels(BodyInstance.GetResponseToChannels());
		}
		NewComponent->RegisterComponent();
		NewComponent->SplineParams = SplineMeshParams;
		NewComponent->SetStaticMesh(StaticMesh.Get());
		NewComponent->UpdateRenderStateAndCollision();
	}
}

void URoadLaneAttributeComponentTemplateDescriptor::GenerateAsset(const FReferenceSplineMeshParams& SplineMeshParams, AActor* TargetActor, bool bIsPreview) const
{
	if (IsValid(ComponentTemplate))
	{
		const FTransform Transform = URoadLaneAttributeGenerateDescriptor::CalcSliceTransformAtSplineOffset(SplineMeshParams, ComponentToSegmentAlign);
		USceneComponent* NewComponent = NewObject<USceneComponent>(TargetActor, ComponentTemplate.Get(), *AssetUtils::GenerateValidComponentName(GetDisplayName().ToString(), TargetActor), bIsPreview ? RF_NoFlags : RF_Transactional);
		NewComponent->SetupAttachment(TargetActor->GetRootComponent());
		TargetActor->AddInstanceComponent(NewComponent);
		NewComponent->SetMobility(EComponentMobility::Static);
		NewComponent->OnComponentCreated();
		if (bIsPreview)
		{
			if (UPrimitiveComponent* MewPrimitiveComponent = Cast<UPrimitiveComponent>(NewComponent))
			{
				MewPrimitiveComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			}
		}
		NewComponent->RegisterComponent();
		if (USplineMeshComponent* NewSplineComponent = Cast<USplineMeshComponent>(NewComponent))
		{
			NewSplineComponent->SplineParams = SplineMeshParams;
			NewSplineComponent->UpdateRenderStateAndCollision();
		}
		NewComponent->SetRelativeTransform(Transform);
	}
}

void URoadLaneAttributeActortTemplateDescriptor::GenerateAsset(const FReferenceSplineMeshParams& SplineMeshParams, AActor* TargetActor, bool bIsPreview) const
{
	if (IsValid(Actor))
	{
		const FTransform Transform = URoadLaneAttributeGenerateDescriptor::CalcSliceTransformAtSplineOffset(SplineMeshParams, ActorToSegmentAlign);
		auto NewActor = TargetActor->GetWorld()->SpawnActor(Actor.Get(), &Transform);
		NewActor->AttachToActor(TargetActor, FAttachmentTransformRules::KeepRelativeTransform);
	}
}