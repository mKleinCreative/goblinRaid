/*
* Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
* Email: ivzhuk7@gmail.com
*/


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "Engine/Scene.h"

#include "MetaRoadActor.generated.h"

/**
 * AMetaRoad
 */
UCLASS(Blueprintable, PrioritizeCategories = ("Meta Road"), HideCategories = (Physics, Navigation, Collision, HLOD, Mobile, RayTracing, Input, Networking, Replication))
class METAROAD_API AMetaRoad : public AActor
{
	GENERATED_UCLASS_BODY()

public:

	/** If true, the previously generated actor and its disk assets are destroyed before each new generation. */
	UPROPERTY(EditAnywhere, Category = "Meta Road", meta = (DisplayName = "Replace on Regenerate"))
	bool bReplaceOnRegenerate = true;

	/** Reference to the actor produced by the last TriangulateRoadTool Accept. Updated automatically. */
	UPROPERTY(VisibleInstanceOnly, Category = "Meta Road")
	TObjectPtr<AActor> LastGeneratedActor = nullptr;

#if WITH_EDITORONLY_DATA
	/** Per-actor road-mesh build settings (a UMetaRoadBuildSettings from the editor module — typed as
	 *  UObject here because that type lives in the editor module). Editor-only: serialized with the
	 *  level in the editor, stripped from cooked/shipping builds. Edited in this actor's Details. */
	UPROPERTY(EditAnywhere, Instanced, Category = "Meta Road", meta = (DisplayName = "Build Settings"))
	TObjectPtr<UObject> BuildSettings;
#endif

#if WITH_EDITOR
	/** Move every URoadSplineComponent's local origin to the center of the combined bounding box,
	 *  shifting spline control points so world-space geometry is unchanged. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Utils", meta = (DisplayName = "Center Spline Origins"))
	void CenterSplineOrigins();
#endif

};
