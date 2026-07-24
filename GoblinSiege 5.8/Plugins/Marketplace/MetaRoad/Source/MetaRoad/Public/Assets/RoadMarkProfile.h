/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "StructUtils/InstancedStruct.h"
#include "RoadMarkProfile.generated.h"

/**
 * Contains road mark profile for the "Build Mesh Tool".
 */
UCLASS(BlueprintType, MinimalAPI)
class URoadMarkProfile : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = MarkProfile, Meta = (ExcludeBaseStruct))
	TInstancedStruct<struct FRoadLaneMarkProfile> LaneMarkProfiles;
};