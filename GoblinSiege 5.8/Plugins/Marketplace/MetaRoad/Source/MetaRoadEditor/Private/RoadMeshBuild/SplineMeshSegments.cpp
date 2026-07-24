/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadMeshBuild/SplineMeshSegments.h"
#include "Utils/AssetUtils.h"
#include "Assets/RoadLaneAttributeGenerate.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"

using namespace MetaRoad;

void FSplineMeshSegments::ApplyTransform(const FTransform& Transform)
{
	for (auto& Segment : Segments)
	{
		Segment.SplineMeshParams.StartPos = Transform.TransformPosition(Segment.SplineMeshParams.StartPos);
		Segment.SplineMeshParams.EndPos = Transform.TransformPosition(Segment.SplineMeshParams.EndPos);
		Segment.SplineMeshParams.StartTangent = Transform.TransformVector(Segment.SplineMeshParams.StartTangent);
		Segment.SplineMeshParams.EndTangent = Transform.TransformVector(Segment.SplineMeshParams.EndTangent);
	}
}

void FSplineMeshSegments::ApplyTransformInverse(const FTransform& Transform)
{
	for (auto& Segment : Segments)
	{
		Segment.SplineMeshParams.StartPos = Transform.InverseTransformPosition(Segment.SplineMeshParams.StartPos);
		Segment.SplineMeshParams.EndPos = Transform.InverseTransformPosition(Segment.SplineMeshParams.EndPos);
		Segment.SplineMeshParams.StartTangent = Transform.InverseTransformVector(Segment.SplineMeshParams.StartTangent);
		Segment.SplineMeshParams.EndTangent = Transform.InverseTransformVector(Segment.SplineMeshParams.EndTangent);
	}
}

void FSplineMeshSegments::BuildComponents(AActor* TargetActor, bool bIsPreview) const
{
	for (auto& Segment : Segments)
	{
		FReferenceSplineMeshParams Params(Segment.SplineMeshParams);
		Params.bAlignWorldUpVector = Segment.bAlignWorldUpVector;
		Params.StartLeftWidth      = Segment.StartLeftWidth;
		Params.StartRightWidth     = Segment.StartRightWidth;
		Params.EndLeftWidth        = Segment.EndLeftWidth;
		Params.EndRightWidth       = Segment.EndRightWidth;

		if (Segment.Profile)
		{
			Segment.Profile->GenerateAsset(Params, TargetActor, bIsPreview);
		}
	}
}