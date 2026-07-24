/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "MetaRoadTypes.h"
#include "Assets/RoadLaneAttributeMark.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"

#include "MetaRoadPresetBase_DEPRECATED.generated.h"


USTRUCT(BlueprintType, Blueprintable)
struct FRoadLaneMaterialProfile
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, NonTransactional, Category = MaterialProfile)
	TObjectPtr<UMaterialInterface> DefaultMaterial;
};

USTRUCT(BlueprintType, Blueprintable)
struct FSurfaceProfile: public FRoadLaneMaterialProfile
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaterialProfile)
	int Priority = 0;

	UPROPERTY(EditAnywhere, NonTransactional, Category = MaterialProfile)
	TObjectPtr<UMaterialInterface> DecaltMaterial;
};

USTRUCT(BlueprintType, Blueprintable)
struct FCurblProfile : public FRoadLaneMaterialProfile
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CurblProfile)
	FRuntimeFloatCurve CurbCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CurblProfile)
	float Width = 15;
};


/**
 * UMetaRoadPresetBase
 */
UCLASS()
class METAROAD_API UMetaRoadPresetBase : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Preset)
	TMap<FName, FCurblProfile> CurbProfiles;

	UPROPERTY(EditAnywhere, Category = Preset, Meta = (ExcludeBaseStruct))
	TMap<FName, TInstancedStruct<FRoadLaneMarkProfile>> LaneMarkProfiles;

	UPROPERTY(EditAnywhere, Category = MaterialProfiles)
	TMap<FName, FSurfaceProfile> DriveableMaterialProfiles;

	//UPROPERTY(EditAnywhere, Category = MaterialProfiles)
	//TMap<FName, FRoadLaneMaterialProfile> DecalMaterialProfiles;

	UPROPERTY(EditAnywhere, Category = MaterialProfiles)
	TMap<FName, FSurfaceProfile> SidewalkMaterialProfiles;

	UPROPERTY(EditAnywhere, Category = MaterialProfiles)
	TMap<FName, FRoadLaneMaterialProfile> LaneMarkMaterialProfiles;


public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FName("MetaRoadPreset"), FPackageName::GetShortFName(GetOutermost()->GetName()));
	}
};