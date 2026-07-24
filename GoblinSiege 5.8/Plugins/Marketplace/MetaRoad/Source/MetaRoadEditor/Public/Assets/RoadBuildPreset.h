/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RoadBuildPreset.generated.h"

class UMetaRoadBuildSettings;

/**
 * URoadBuildPreset
 *
 * A reusable, first-class snapshot of road mesh-build settings (a UMetaRoadBuildSettings — triangulation
 * params + every layer's settings). Applied to a road actor's build settings via the mode's Preset panel
 * or the Road Profile editor. Replaces the old engine tool-preset collection storage (which keyed presets
 * under a non-existent tool); parallels URoadProfile as a Content Browser asset.
 */
UCLASS(BlueprintType)
class METAROADEDITOR_API URoadBuildPreset : public UObject
{
	GENERATED_BODY()

public:
	/** The saved build settings (instanced sub-object). Apply copies these into a target road's settings. */
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Road Build Preset")
	TObjectPtr<UMetaRoadBuildSettings> Settings;

	/** Optional user-facing label/notes (shown in the preset picker). */
	UPROPERTY(EditAnywhere, Category = "Road Build Preset")
	FText DisplayName;

	UPROPERTY(EditAnywhere, Category = "Road Build Preset", meta = (MultiLine = true))
	FText Description;
};
