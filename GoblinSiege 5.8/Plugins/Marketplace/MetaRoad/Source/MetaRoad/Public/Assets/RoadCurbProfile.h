/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "RoadCurbProfile.generated.h"

class UMaterialInterface;

/**
 * Contains curb profile for the "Build Mesh Tool". Used in FRoadZoneSidewalk. 
 */
UCLASS(BlueprintType, MinimalAPI)
class URoadCurbProfile : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CurblProfile)
	FRuntimeFloatCurve CurbCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CurblProfile)
	float Width = 15;
	
	UPROPERTY(EditAnywhere, NonTransactional, Category = CurblProfile)
	TObjectPtr<UMaterialInterface> DefaultMaterial;
};
