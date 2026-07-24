/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Assets/RoadLaneAttributeDescriptor.h"
#include "RoadLaneAttributeLandscape.generated.h"

 /**
 * FRoadLaneAttributeValueLandscape
 */
USTRUCT(BlueprintType, Blueprintable)
struct METAROADEDITOR_API FRoadLaneAttributeValueLandscape : public FRoadLaneAttributeValue
{
	GENERATED_USTRUCT_BODY()

	virtual ~FRoadLaneAttributeValueLandscape() {}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Landscape, meta = (UIMin = 0, ClampMin = 0, UIMax = 1, ClampMax = 1))
	float LayerFactor = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Landscape, meta = (UIMin = 0, ClampMin = 0))
	float SideFalloff = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Landscape, meta = (UIMin = 0, ClampMin = 0))
	float SideOffset = 0;

	// Z Offset
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Landscape, meta = (UIMin = 0, ClampMin = 0))
	double HeightOffset = 50;

	virtual bool CanInterpolate() const override { return true; }
	virtual bool Interpolate(const UScriptStruct* ValueType, const FRoadLaneAttributeValue* Other, float Alpha, FRoadLaneAttributeValue* Out) const override;
};

/**
 * Used to customize the landscape along the central spline lane.
 */
UCLASS(BlueprintType, DisplayName="Landscape")
class METAROADEDITOR_API URoadLaneAttributeLandscapeDescriptor : public URoadLaneAttributeDescriptor
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadAttribute)
	FRoadLaneAttributeValueLandscape AttributeValueTemplate;

	virtual TConstStructView<FRoadLaneAttributeValue> GetAttributeValueTemplate() const override { return AttributeValueTemplate; }

	virtual bool CanBeAdded() const override { return true; }
	virtual bool CanBeAddedTo(const URoadSplineComponent* Spline, int SectionIndex, int LaneIndex) const
	{
		return Super::CanBeAddedTo(Spline, SectionIndex, LaneIndex) && (LaneIndex == 0);
	}
};


