/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MetaRoadVisibilitySettings.generated.h"

/**
 * Visibility settings/actions for the Meta Road editor mode's "Visibility" tile (Misk palette), shown via
 * an IDetailsView. Holds editor-only visibility toggles, debug-draw toggles and CallInEditor buttons.
 *
 * A single config-backed instance (the CDO, see Get()) edited in the panel and read globally by the build
 * pipeline / mode render (bDrawBoundaries, bShowWireframe).
 */
UCLASS(config = EditorPerProjectUserSettings)
class UMetaRoadVisibilitySettings : public UObject
{
	GENERATED_BODY()

public:
	/** Whether UTileMapWindowComponent tile renders are visible in the editor. */
	UPROPERTY(EditAnywhere, Category = "Meta Road")
	bool bTilesVisibility = true;

	/** Draw the triangulation boundary debug lines over the live preview (Preview view mode). */
	UPROPERTY(EditAnywhere, config, Category = "Debug")
	bool bDrawBoundaries = false;

	/** Show the generated road meshes as wireframe in the live preview. */
	UPROPERTY(EditAnywhere, config, Category = "Debug")
	bool bShowWireframe = false;

	/** Make all hidden road splines visible again, on every actor in the level. */
	UFUNCTION(CallInEditor, Category = "Meta Road")
	void UnhideAllSplines();

	/** Refresh editable properties from the current module/editor state. */
	void SyncFromModule();

	/** The single shared instance (CDO), persisted to the editor config. */
	static UMetaRoadVisibilitySettings* Get() { return GetMutableDefault<UMetaRoadVisibilitySettings>(); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
