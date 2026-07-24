/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "MetaRoadEditorSettings.h"
#include "Engine/Texture.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Styling/StyleColors.h"
#include "MetaRoadEditorModule.h"
#include "Utils/DrawUtils.h"
#include "Engine/Texture2D.h"
#include "MetaRoadSettings.h" // RoadZoneTypes for GetEditorLaneMatrtial

#define LOCTEXT_NAMESPACE "UMetaRoadEditorSettings"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaRoadEditorSettings)


UMetaRoadEditorSettings::UMetaRoadEditorSettings()
	: GizmoMaterialMaterial(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_Gizmo.M_Gizmo")))
	, UIVertexColorMaterial(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_UIVertexColor.M_UIVertexColor")))
	, UIVertexColorTransperentMaterial(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_UIVertexColorTransperent.M_UIVertexColorTransperent")))
	, UITexColorMaterial(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_UITexColor.M_UITexColor")))

	, LaneTransitionDefaultTex(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Textures/Dir/T_Default.T_Default")))
	, LaneTransitionNoEntryTex(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Textures/Dir/T_NoEntry.T_NoEntry")))
	, LaneTransitionForwardTex(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Textures/Dir/T_Forward.T_Forward")))
	, LaneTransitionLeftTex(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Textures/Dir/T_Left.T_Left")))
	, LaneTransitionRightTex(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Textures/Dir/T_Right.T_Right")))
	, LaneTransitionForwardLeftTex(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Textures/Dir/T_ForwardLeft.T_ForwardLeft")))
	, LaneTransitionForwardRightTex(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Textures/Dir/T_ForwardRight.T_ForwardRight")))
	, LaneTransitionAnyTex(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Textures/Dir/T_Any.T_Any")))

	, DefaultSolidMaterial(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_Solid.M_Solid")))
	, DefaultDirectLaneMaterial(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_DirectLane.M_DirectLane")))
	, DefaultDirectLaneTransparentMaterial(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_DirectLaneTransparent.M_DirectLaneTransparent")))
	, DefaultDirectLaneGridMaterial(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_DirectLaneGrid.M_DirectLaneGrid")))
{

	LaneToZoneGraph.Add(ERoadZoneTypes::Driving, FZoneGraphTagMask(1));

	TileSources.Add(TEXT("Google Satellite Only"), { TEXT("http://mt0.google.com/vt/lyrs=s&hl=en&x={x}&y={y}&z={z}"), ETileMapProjection::WebMercator });
	TileSources.Add(TEXT("Google Roadmap"), { TEXT("http://mt0.google.com/vt/lyrs=m&hl=en&x={x}&y={y}&z={z}"), ETileMapProjection::WebMercator });
	TileSources.Add(TEXT("Google Terrain"), { TEXT("http://mt0.google.com/vt/lyrs=p&hl=en&x={x}&y={y}&z={z}"), ETileMapProjection::WebMercator });
	TileSources.Add(TEXT("Google Altered Roadmap"), { TEXT("http://mt0.google.com/vt/lyrs=r&hl=en&x={x}&y={y}&z={z}"), ETileMapProjection::WebMercator });
	TileSources.Add(TEXT("Google Terrain Only"), { TEXT("http://mt0.google.com/vt/lyrs=t&hl=en&x={x}&y={y}&z={z}"), ETileMapProjection::WebMercator });
	TileSources.Add(TEXT("Google Hybrid"), { TEXT("http://mt0.google.com/vt/lyrs=y&hl=en&x={x}&y={y}&z={z}"), ETileMapProjection::WebMercator });
	TileSources.Add(TEXT("OSM"), { TEXT("https://tile.openstreetmap.org/{z}/{x}/{y}.png"), ETileMapProjection::WebMercator });
	TileSources.Add(TEXT("Yandex Satellite Only"), { TEXT("https://sat01.maps.yandex.net/tiles?l=sat&v=1.22.0&x={x}&y={y}&z={z}&g=Gagari"), ETileMapProjection::WorldMercator });

	LoadaAssets();
}

void UMetaRoadEditorSettings::LoadaAssets()
{
	auto GizmoMaterialMat = GizmoMaterialMaterial.LoadSynchronous();
	check(GizmoMaterialMat);

	check(UIVertexColorMaterial.LoadSynchronous());

	LaneConnectionMaterialDyn = UMaterialInstanceDynamic::Create(GizmoMaterialMat, nullptr);
	LaneConnectionMaterialDyn->SetVectorParameterValue(TEXT("GizmoColor"), FColor(255, 255, 255));

	LaneConnectionSelectedMaterialDyn = UMaterialInstanceDynamic::Create(GizmoMaterialMat, nullptr);
	LaneConnectionSelectedMaterialDyn->SetVectorParameterValue(TEXT("GizmoColor"), FStyleColors::AccentOrange.GetSpecifiedColor());

	auto UITexColorMat = UITexColorMaterial.LoadSynchronous();
	check(UITexColorMat);


	LaneTransitionDefaultMat = UMaterialInstanceDynamic::Create(UITexColorMat, nullptr);
	LaneTransitionNoEntryMat = UMaterialInstanceDynamic::Create(UITexColorMat, nullptr);
	LaneTransitionForwardMat = UMaterialInstanceDynamic::Create(UITexColorMat, nullptr);
	LaneTransitionLeftMat = UMaterialInstanceDynamic::Create(UITexColorMat, nullptr);
	LaneTransitionRightMat = UMaterialInstanceDynamic::Create(UITexColorMat, nullptr);
	LaneTransitionForwardLeftMat = UMaterialInstanceDynamic::Create(UITexColorMat, nullptr);
	LaneTransitionForwardRightMat = UMaterialInstanceDynamic::Create(UITexColorMat, nullptr);
	LaneTransitionAnyMat = UMaterialInstanceDynamic::Create(UITexColorMat, nullptr);

	LaneTransitionDefaultMat->SetTextureParameterValue("Texture", LaneTransitionDefaultTex.LoadSynchronous());
	LaneTransitionNoEntryMat->SetTextureParameterValue("Texture", LaneTransitionNoEntryTex.LoadSynchronous());
	LaneTransitionForwardMat->SetTextureParameterValue("Texture", LaneTransitionForwardTex.LoadSynchronous());
	LaneTransitionLeftMat->SetTextureParameterValue("Texture", LaneTransitionLeftTex.LoadSynchronous());
	LaneTransitionRightMat->SetTextureParameterValue("Texture", LaneTransitionRightTex.LoadSynchronous());
	LaneTransitionForwardLeftMat->SetTextureParameterValue("Texture", LaneTransitionForwardLeftTex.LoadSynchronous());
	LaneTransitionForwardRightMat->SetTextureParameterValue("Texture", LaneTransitionForwardRightTex.LoadSynchronous());
	LaneTransitionAnyMat->SetTextureParameterValue("Texture", LaneTransitionAnyTex.LoadSynchronous());

	// Road-spline schematic preview dynamics (moved from UMetaRoadSettings).
	UMaterialInterface* SolidMat = DefaultSolidMaterial.LoadSynchronous();
	UMaterialInterface* DirectMat = DefaultDirectLaneMaterial.LoadSynchronous();
	UMaterialInterface* DirectTransparentMat = DefaultDirectLaneTransparentMaterial.LoadSynchronous();
	UMaterialInterface* DirectGridMat = DefaultDirectLaneGridMaterial.LoadSynchronous();
	checkf(SolidMat, TEXT("Can't load DefaultSolidMaterial"));
	checkf(DirectMat, TEXT("Can't load DefaultDirectLaneMaterial"));
	checkf(DirectTransparentMat, TEXT("Can't load DefaultDirectLaneTransparentMaterial"));
	checkf(DirectGridMat, TEXT("Can't load DefaultDirectLaneGridMaterial"));

	EmptyLaneMatrtial = UMaterialInstanceDynamic::Create(DirectGridMat, nullptr);
	EmptyLaneMatrtial->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(FColor(87, 35, 35)));

	HiddenLaneMatrtial = UMaterialInstanceDynamic::Create(DirectTransparentMat, nullptr);
	HiddenLaneMatrtial->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(FColor(50, 50, 50)));

	SelectedLaneMatrtial = UMaterialInstanceDynamic::Create(DirectMat, nullptr);
	SelectedLaneMatrtial->SetVectorParameterValue(TEXT("BaseColor"), FStyleColors::AccentOrange.GetSpecifiedColor() * 0.5);

	SplineArrowMatrtial = UMaterialInstanceDynamic::Create(SolidMat, nullptr);
	SplineArrowMatrtial->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(FColor::White));
}

UMaterialInterface* UMetaRoadEditorSettings::GetEditorLaneMatrtial(FRoadZoneType ZoneType) const
{
	if (const FRoadZoneTypeDetails* ZoneTypeDetails = GetDefault<UMetaRoadSettings>()->RoadZoneTypes.Find(ZoneType.GetName()))
	{
		return ZoneTypeDetails->GetEditorMaterial();
	}
	return EmptyLaneMatrtial.Get();
}

FText UMetaRoadEditorSettings::GetSectionText() const
{
	return LOCTEXT("MetaRoadEditorSettings_Section", "MetaRoad Editor");
}

FText UMetaRoadEditorSettings::GetSectionDescription() const
{
	return LOCTEXT("MetaRoadEditorSettings_Description", "MetaRoad Editor Settings");
}



#undef LOCTEXT_NAMESPACE
