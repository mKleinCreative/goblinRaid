/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MetaRoadBlueprintLibrary.generated.h"

class AMetaRoad;
class URoadBuildPreset;

UCLASS()
class METAROADEDITOR_API UMetaRoadBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * One-shot migration: converts legacy road build-mesh presets — stored in the engine tool-preset
	 * collections under the old key "TriangulateRoadTool" — into first-class URoadBuildPreset assets.
	 * Non-destructive: a new "<Label>_Migrated" asset is created next to the source collection asset
	 * (or under FallbackPath for the engine default collection); old presets are left untouched.
	 * Logs each migration + a summary to LogMetaRoad.
	 *
	 * @return  Array of newly created URoadBuildPreset assets.
	 */
	UFUNCTION(BlueprintCallable, Category = "MetaRoad|Utils")
	static TArray<URoadBuildPreset*> MigrateLegacyBuildPresets(const FString& FallbackPath = TEXT("/Game/MetaRoad/BuildPresets"));

	/**
	 * Finds every AActor in the level that owns a URoadSplineComponent but is not
	 * already an AMetaRoad, spawns a replacement AMetaRoad at the same
	 * transform, transfers all instance components, preserves all road connections,
	 * copies the actor label and folder path, then destroys the original actor.
	 * This function is mainly needed for migration to MetaRoad v2.6.0
	 *
	 * @return  Array of newly created AMetaRoad instances.
	 */
	UFUNCTION(BlueprintCallable, Category = "MetaRoad|Utils", meta = (WorldContext = "WorldContextObject"))
	static TArray<AMetaRoad*> ConvertActorsToMetaRoadActors(UObject* WorldContextObject);

	/**
	 * Saves the given AMetaRoad as a Blueprint class to a user-chosen Content Browser path (modal save
	 * dialog + FKismetEditorUtilities::CreateBlueprintFromActor). Inter-spline connections are preserved
	 * via the connection-GUID snapshot (see URoadSplineComponent::RefreshConnectionGuids / PostDuplicate).
	 */
	UFUNCTION(BlueprintCallable, Category = "MetaRoad|Utils")
	static void SaveAsTemplate(AMetaRoad* Road);
};
