/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Assets/RoadLaneAttributePolygoneCustomization.h"
#include "Components/SplineMeshComponent.h"
#include "RoadLaneAttributeSidewalkHeight.generated.h"



/**
 * Used to set the height of the sidewalk. 
 * This attribute doesn't change the topology of the generated mesh (does not create new triangles and vertices), 
 * but only changes the height of the generated vertices. This means that it doesn't make sense to place keys too closely on a road lane.
 */
USTRUCT(BlueprintType, Blueprintable)
struct METAROADEDITOR_API FRoadLaneAttributeSidewalkHeightValue : public FRoadLaneAttributeValue
{
	GENERATED_USTRUCT_BODY()

	virtual ~FRoadLaneAttributeSidewalkHeightValue() {}

	FRoadLaneAttributeSidewalkHeightValue() = default;
	FRoadLaneAttributeSidewalkHeightValue(double Height)
		: Height(Height)
	{
	}

	UPROPERTY(EditAnywhere, Category = AttributeKey, meta = (UIMin = -100, ClampMin = -100, UIMax = 100, ClampMax = 100))
	double Height = 15;

	virtual bool Interpolate(const UScriptStruct* ValueType, const FRoadLaneAttributeValue* Other, float Alpha, FRoadLaneAttributeValue* Out) const override;
	virtual bool CanInterpolate() const { return true; }
};


/**
 * Used to set the height of the sidewalk.
 * This attribute doesn't change the topology of the generated mesh (does not create new triangles and vertices),
 * but only changes the height of the generated vertices. This means that it doesn't make sense to place keys too closely on a road lane.
 */
UCLASS(BlueprintType, Blueprintable, DisplayName = "Sidewalk Height")
class METAROADEDITOR_API URoadLaneAttributeSidewalkHeightDescriptor : public URoadLaneAttributeDescriptor
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadAttribute)
	FRoadLaneAttributeSidewalkHeightValue AttributeValueTemplate{};

	virtual TConstStructView<FRoadLaneAttributeValue> GetAttributeValueTemplate() const override { return AttributeValueTemplate; }
	virtual bool CanBeAddedTo(const URoadSplineComponent* Spline, int SectionIndex, int LaneIndex) const;

};

