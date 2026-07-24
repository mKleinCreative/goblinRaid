/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineMeshComponent.h"

class URoadLaneAttributeDescriptor;
class URoadLaneAttributeGenerateDescriptor;

namespace MetaRoad
{
	struct METAROADEDITOR_API FSplineMeshSegments
	{
		FSplineMeshSegments() = default;
		FSplineMeshSegments(const FSplineMeshSegments& Other) = default;
		FSplineMeshSegments(FSplineMeshSegments&& Other) = default;
		
		const FSplineMeshSegments& operator=(const FSplineMeshSegments& Other)
		{
			Segments = Other.Segments;
			return *this;
		}

		const FSplineMeshSegments& operator=(FSplineMeshSegments&& Other)
		{
			Segments = MoveTemp(Other.Segments);
			return *this;
		}
		
		void ApplyTransform(const FTransform& Transform);
		void ApplyTransformInverse(const FTransform& Transform);
		void BuildComponents(AActor* TargetActor, bool bIsPreview) const;
		
		struct FSegment
		{
			bool bAlignWorldUpVector;
			FSplineMeshParams SplineMeshParams;
			const URoadLaneAttributeGenerateDescriptor* Profile;
			float StartLeftWidth  = 0.f;
			float StartRightWidth = 0.f;
			float EndLeftWidth    = 0.f;
			float EndRightWidth   = 0.f;
		};
		
		TArray<FSegment> Segments;
	};
}