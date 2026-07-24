/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/TextureDefines.h"
#include "Engine/DeveloperSettings.h"
#include "RoadSplineComponent.h"
#include "MetaRoadTypes.h"
#include "MetaRoadSettings.generated.h"

UCLASS(config = Plugins, defaultconfig, meta=(DisplayName="Meta Road"))
class METAROAD_API UMetaRoadSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMetaRoadSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());


	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("MetaRoad"); }

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
#endif

	UPROPERTY(EditAnywhere, config, Category = Base)
	TMap<FName, FRoadZoneTypeDetails> RoadZoneTypes;

#if WITH_EDITORONLY_DATA

	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay, meta = (ClampMin = "2", ClampMax = "100"))
	int NumPointPerSegmaent = 20;

	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay, meta = (ClampMin = "2", ClampMax = "100"))
	int NumPointPerSection = 20;
#endif

protected:
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif
};
