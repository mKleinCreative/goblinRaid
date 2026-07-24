/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/TextureDefines.h"
#include "ZoneGraphTypes.h"
#include "MetaRoadTypes.h"
#include "RoadSplineComponent.h" // ERoadLandscapeBlendMode
#include "MetaRoadEditorSettings.generated.h"

USTRUCT()
struct FZoneGraphLaneSetup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = TileMapSource)
	bool bSidewalk{};

	UPROPERTY(EditAnywhere, config, Category = TileMapSource)
	FZoneGraphTagInfo Tag{};
};

UENUM()
enum class ETileMapProjection: uint8
{
	/** EPSG:3857 - Spherical Mercator, also known as WGS84 Web Mercator or WGS84/Pseudo-Mercator */
	WebMercator,

	/** EPSG:3395 - True Elliptical Mercator WGS84 */
	WorldMercator,
};

USTRUCT()
struct FTileMapSource
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = TileMapSource)
	FString URL;

	UPROPERTY(EditAnywhere, config, Category = TileMapSource)
	ETileMapProjection Projection = ETileMapProjection::WebMercator;
};

UCLASS(config = Plugins, defaultconfig, meta=(DisplayName="Meta Road Editor"))
class METAROADEDITOR_API UMetaRoadEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMetaRoadEditorSettings();

	//~ Begin UDeveloperSettings interface
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("MetaRoadEditor"); }

	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
	//~ End UDeveloperSettings interface

public:

	// Determine which types of road lanes need to be exported to ZoneGraph
	UPROPERTY(EditAnywhere, config, Category = ZoneGraph)
	TMap<FRoadZoneType, FZoneGraphTagMask> LaneToZoneGraph;


	/** 
	 * Determines whether the ZoneGraphDelegates::OnZoneGraphDataBuildDone delegate should be called after the graphs are created. 
	 * If true, the OnZoneGraphDataBuildDone delegate will be called twice in a row: 
	 * - the first call is the default one, after processing the UZoneShapeComponent, 
	 * - the second call is an additional call after processing the URoadGraphDataComponent. 
	 * Unfortunately, the current ZoneGraph plugin API does not allow a single call to the OnZoneGraphDataBuildDone delegate for both component types. 
	 * I hope this will become possible in future versions of ZoneGraph plugin.
	 */
	UPROPERTY(EditAnywhere, config, Category = ZoneGraph)
	bool bCallZoneGraphDataBuildDone = true;

	/** Height blending algorithm in case of intersection of two or more road splines on a landscape. */
	UPROPERTY(EditAnywhere, config, Category = Landscape)
	ERoadLandscapeBlendMode LandscapeBlendMode = ERoadLandscapeBlendMode::Maximum;

	UPROPERTY(EditAnywhere, config, Category = TileMapSources)
	TMap<FName, FTileMapSource> TileSources;

	UPROPERTY()
	TSoftObjectPtr<UMaterialInterface> GizmoMaterialMaterial;

	UPROPERTY()
	TSoftObjectPtr<UMaterialInterface> UIVertexColorMaterial;

	UPROPERTY()
	TSoftObjectPtr<UMaterialInterface> UIVertexColorTransperentMaterial;

	UPROPERTY()
	TSoftObjectPtr<UMaterialInterface> UITexColorMaterial;

	UPROPERTY()
	TSoftObjectPtr<UTexture2D> LaneTransitionDefaultTex;

	UPROPERTY()
	TSoftObjectPtr<UTexture2D> LaneTransitionNoEntryTex;

	UPROPERTY()
	TSoftObjectPtr<UTexture2D> LaneTransitionForwardTex;

	UPROPERTY()
	TSoftObjectPtr<UTexture2D> LaneTransitionLeftTex;

	UPROPERTY()
	TSoftObjectPtr<UTexture2D> LaneTransitionRightTex;

	UPROPERTY()
	TSoftObjectPtr<UTexture2D> LaneTransitionForwardLeftTex;

	UPROPERTY()
	TSoftObjectPtr<UTexture2D> LaneTransitionForwardRightTex;

	UPROPERTY()
	TSoftObjectPtr<UTexture2D> LaneTransitionAnyTex;

	/** The size adjustment to apply to spline line thickness which increases the spline's hit tolerance. */
	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay, meta = (ClampMin = "0.00"))
	double CenterSplineLineThicknessAdjustment = 4.0f;

	/** The scale to apply to spline tangent lengths */
	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay, meta = (ClampMin = "0.00"))
	double SplineTangentScale = 0.5f;

	/** The size adjustment to apply to selected spline points (in screen space units). */
	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay, meta = (ClampMin = "-5.0", ClampMax = "20.0"))
	double SelectedSplinePointSizeAdjustment = 0.0f;

	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay, meta = (ClampMin = "-5.0", ClampMax = "20.0"))
	double SplineTangentHandleSizeAdjustment = 0.0f;

	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay, meta = (ClampMin = "100.0", ClampMax = "100000.0"))
	double RoadConnectionsMaxViewDistance = 30000.0;

	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay, meta = (ClampMin = "100.0", ClampMax = "200000.0"))
	double RoadConnectionMaxViewOrthoWidth = 50000.0;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	TObjectPtr<UMaterialInstanceDynamic> LaneConnectionMaterialDyn;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	TObjectPtr<UMaterialInstanceDynamic> LaneConnectionSelectedMaterialDyn;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	TObjectPtr<UMaterialInstanceDynamic> LaneTransitionDefaultMat;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	TObjectPtr<UMaterialInstanceDynamic> LaneTransitionNoEntryMat;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	TObjectPtr<UMaterialInstanceDynamic> LaneTransitionForwardMat;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	TObjectPtr<UMaterialInstanceDynamic> LaneTransitionLeftMat;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	TObjectPtr<UMaterialInstanceDynamic> LaneTransitionRightMat;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	TObjectPtr<UMaterialInstanceDynamic> LaneTransitionForwardLeftMat;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	TObjectPtr<UMaterialInstanceDynamic> LaneTransitionForwardRightMat;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	TObjectPtr<UMaterialInstanceDynamic> LaneTransitionAnyMat;


	// --- Road-spline schematic preview (moved from UMetaRoadSettings) ---
	// Source materials for the preview dynamics below; the dynamics are created in LoadaAssets().
	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay)
	TSoftObjectPtr<UMaterialInterface> DefaultSolidMaterial;

	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay)
	TSoftObjectPtr<UMaterialInterface> DefaultDirectLaneMaterial;

	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay)
	TSoftObjectPtr<UMaterialInterface> DefaultDirectLaneTransparentMaterial;

	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay)
	TSoftObjectPtr<UMaterialInterface> DefaultDirectLaneGridMaterial;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	TObjectPtr<UMaterialInstanceDynamic> EmptyLaneMatrtial;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	TObjectPtr<UMaterialInstanceDynamic> HiddenLaneMatrtial;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	TObjectPtr<UMaterialInstanceDynamic> SelectedLaneMatrtial;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	TObjectPtr<UMaterialInstanceDynamic> SplineArrowMatrtial;

	/** Editor pre-visualization material for a road zone (moved from UMetaRoadSettings).
	 *  Reads RoadZoneTypes from the runtime UMetaRoadSettings; falls back to EmptyLaneMatrtial. */
	UMaterialInterface* GetEditorLaneMatrtial(FRoadZoneType ZoneType) const;

	void LoadaAssets();
};

