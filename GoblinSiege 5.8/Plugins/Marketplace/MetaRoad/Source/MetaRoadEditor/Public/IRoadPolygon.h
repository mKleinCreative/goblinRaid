/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MetaRoadTypes.h"
#include "IRoadPolygon.generated.h"

/** Polygon data for prcedure generation */
USTRUCT(BlueprintType)
struct FRoadPolygonData
{
	GENERATED_BODY()

	/** In World space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadPolygonData)
	TArray<FVector> Vertices{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadPolygonData)
	TInstancedStruct<FRoadZone> RoadZone{};

	/** UV coordinates angle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadPolygonData)
	double TextureAngle = 0.0;

	/** UV coordinates scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadPolygonData)
	double TextureScale = 1.0;
};


UINTERFACE(MinimalAPI, Blueprintable)
class URoadPolygonInterfcae : public UInterface
{
    GENERATED_BODY()
};

/**
 * A actor component inherited from this interface will participate in the procedural generation of the road surface.
 */
class METAROADEDITOR_API IRoadPolygonInterfcae
{
    GENERATED_BODY()

public:

	virtual bool GeneratePolygones(TArray<FRoadPolygonData>& OutPolygons) const
	{
		return ReceiveGeneratePolygones(OutPolygons);
	}

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Road Polygon", meta = (DisplayName = "GeneratePolygones"))
	bool ReceiveGeneratePolygones(TArray<FRoadPolygonData>& OutPolygons) const;
};