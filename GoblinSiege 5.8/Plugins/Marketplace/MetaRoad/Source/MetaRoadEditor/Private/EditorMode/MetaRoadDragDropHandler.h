/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "LevelEditorDragDropHandler.h"
#include "MetaRoadDragDropHandler.generated.h"

struct FAssetData;
class FViewport;

/**
 * Custom level-editor drag-and-drop handler that lets the user drag attribute/profile assets from
 * the Content Browser onto the currently selected RoadLane to add a FRoadLaneAttribute, instead of
 * spawning an actor. Handled asset types:
 *   - URoadLaneAttributeDescriptor blueprints (and any URoadLaneAttributeDescriptor subclass)
 *   - URoadMarkProfile    -> URoadLaneAttributeMarkDescriptor           (FRoadLaneMark, replaces existing)
 *   - URoadPolygonProfile -> URoadLaneAttributePolygonDescriptor (FRoadLaneAttributePolygonValue,
 *                            adds a new key at the SOffset nearest the cursor)
 *
 * For every other asset it falls back to the engine default (Super), so normal placement is intact.
 */
UCLASS(Transient)
class UMetaRoadLevelEditorDragDropHandler : public ULevelEditorDragDropHandler
{
	GENERATED_BODY()

public:
	virtual bool PreviewDropObjectsAtCoordinates(int32 MouseX, int32 MouseY, UWorld* World, FViewport* Viewport, const FAssetData& AssetData) override;
	virtual bool PreDropObjectsAtCoordinates(int32 MouseX, int32 MouseY, UWorld* World, FViewport* Viewport, const TArray<UObject*>& DroppedObjects, TArray<AActor*>& OutNewActors) override;
};
