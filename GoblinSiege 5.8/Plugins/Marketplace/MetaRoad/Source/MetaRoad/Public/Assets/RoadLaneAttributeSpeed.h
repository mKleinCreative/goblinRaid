/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Assets/RoadLaneAttributeDescriptor.h"
#include "RoadLaneAttributeSpeed.generated.h"

 /**
 * Used to stores information about road lane speed limits. Can be used for traffic generation purposes.
 */
USTRUCT(BlueprintType, Blueprintable)
struct METAROAD_API FRoadLaneAttributeValueSpeed : public FRoadLaneAttributeValue
{
	GENERATED_USTRUCT_BODY()

	virtual ~FRoadLaneAttributeValueSpeed() {}

	// Maximum allowed speed [meters/second]
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeKey)
	double MaxSpeed = 15;
};

/**
 * Used to stores information about road lane speed limits. Can be used for traffic generation purposes. 
 */
UCLASS(BlueprintType, DisplayName = "Speed")
class METAROAD_API URoadLaneAttributeSpeedDescriptor : public URoadLaneAttributeDescriptor
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadAttribute)
	FRoadLaneAttributeValueSpeed AttributeValueTemplate;

	virtual TConstStructView<FRoadLaneAttributeValue> GetAttributeValueTemplate() const override { return AttributeValueTemplate; }

#if WITH_EDITOR
	virtual bool CanBeAdded() const override { return true; }
#endif
};


