/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "MetaRoadEditorStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"

#define EDITOR_IMAGE_BRUSH_SVG( RelativePath, ... ) FSlateVectorImageBrush(FPaths::EngineContentDir() / "Editor/Slate" / RelativePath + TEXT(".svg"), __VA_ARGS__ )
#define EDITOR_IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush(FPaths::EngineContentDir() / "Editor/Slate" / RelativePath + TEXT(".png"), __VA_ARGS__ )

FMetaRoadEditorStyle::FMetaRoadEditorStyle() :
    FSlateStyleSet("MetaRoadEditor")
{
	SetParentStyleName(FAppStyle::GetAppStyleSetName());

	const FVector2D Icon16(16.0f, 16.0f);
	const FVector2D Icon20(20.0f, 20.0f);
	const FVector2D Icon24(24.0f, 24.0f);
	const FVector2D Icon64(64.0f, 64.0f);

	// Mode-palette tile icon size, shared by both the "Edit" and "Create" categories so all
	// tiles look uniform (the tile renders the brush at its ImageSize).
	const FVector2D PaletteIcon = Icon20;

	const FSlateColor DefaultForeground(FLinearColor(0.72f, 0.72f, 0.72f, 1.f));
	const FSlateColor LogoColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.f));

	FSlateStyleSet::SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));
	FSlateStyleSet::SetContentRoot(IPluginManager::Get().FindPlugin(TEXT("MetaRoad"))->GetBaseDir() / TEXT("Resources"));

	FTextBlockStyle NormalText = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");

	// ---- Registration helpers (reduce the repetitive Set(...) boilerplate) -------------------
	// Mode-palette tile from a plugin SVG: "RoadEditor.<Cmd>" + a 16px ".Small" variant.
	auto SetToolIcon = [this, &PaletteIcon, &Icon16](const FString& Cmd, const FString& IconPath)
	{
		Set(FName(*(TEXT("RoadEditor.") + Cmd)), new IMAGE_BRUSH_SVG(IconPath, PaletteIcon));
		Set(FName(*(TEXT("RoadEditor.") + Cmd + TEXT(".Small"))), new IMAGE_BRUSH_SVG(IconPath, Icon16));
	};
	// Same, but from an engine ("Starship") SVG.
	auto SetCoreToolIcon = [this, &PaletteIcon, &Icon16](const FString& Cmd, const FString& IconPath)
	{
		Set(FName(*(TEXT("RoadEditor.") + Cmd)), new CORE_IMAGE_BRUSH_SVG(IconPath, PaletteIcon));
		Set(FName(*(TEXT("RoadEditor.") + Cmd + TEXT(".Small"))), new CORE_IMAGE_BRUSH_SVG(IconPath, Icon16));
	};
	// Edit sub-mode tile: tinted plugin SVG, no ".Small" variant.
	auto SetModeIcon = [this, &PaletteIcon, &DefaultForeground](const FString& Mode, const FString& IconPath)
	{
		Set(FName(*(TEXT("RoadEditor.") + Mode)), new IMAGE_BRUSH_SVG(IconPath, PaletteIcon, DefaultForeground));
	};
	// Asset icon + thumbnail from a plugin SVG: "ClassIcon.<Class>" (16) + "ClassThumbnail.<Class>" (64).
	auto SetClassIcon = [this, &Icon16, &Icon64](const FString& Class, const FString& IconPath)
	{
		Set(FName(*(TEXT("ClassIcon.") + Class)), new IMAGE_BRUSH_SVG(IconPath, Icon16));
		Set(FName(*(TEXT("ClassThumbnail.") + Class)), new IMAGE_BRUSH_SVG(IconPath, Icon64));
	};

	// ---- Logo --------------------------------------------------------------------------------
	Set("MetaRoadLogo.Image", new IMAGE_BRUSH_SVG("Icons/Logo", Icon64, LogoColor));
	Set("MetaRoadLogo.Text", FTextBlockStyle(NormalText)
		.SetFont(DEFAULT_FONT("Bold", 32))
		.SetColorAndOpacity(LogoColor));

	// ---- Edit sub-mode tiles -----------------------------------------------------------------
	SetModeIcon("RoadSplineMode",        "Icons/RoadSpline");
	SetModeIcon("RoadSectionMode",       "Icons/RoadLaneSections");
	SetModeIcon("RoadOffsetMode",        "Icons/RoadOffset");
	SetModeIcon("RoadLaneWidthMode",     "Icons/RoadLaneWidth");
	SetModeIcon("RoadLaneMarkMode",      "Icons/RoadLaneMark");
	SetModeIcon("RoadLaneSpeedMode",     "Icons/RoadLaneSpeed");
	SetModeIcon("RoadLaneLandscapeMode", "Icons/LandscapeBrush");
	SetModeIcon("RoadLaneBuildMode",     "Icons/RoadLaneBuild");
	SetModeIcon("RoadAttributeMode",     "Icons/RoadLaneBuild");
	// Preset Edit sub-mode uses the engine "Favorite" (star) icon.
	SetCoreToolIcon("RoadPresetMode",    "Starship/Common/Favorite");

	// Bake sub-mode tile
	SetCoreToolIcon("RoadBakeMode",      "../Editor/Slate/Starship/Common/CookContent");

	// FBX Export sub-mode tile (plugin SVG, untinted so the FBX logo keeps its look)
	SetToolIcon("RoadFbxExportMode", "Icons/FBX");

	// Misk > Visibility sub-mode tile
	SetCoreToolIcon("RoadVisibilityMode", "Starship/Common/visible");

	// ---- Create tiles + utility actions ------------------------------------------------------
	SetToolIcon("MetaRoadToolsTabButton", "Icons/Road");
	SetToolIcon("BeginDrawNewRoad",       "Icons/RoadSpline");
	SetToolIcon("BeginDrawNewPoly",       "Icons/Polygon");
	SetToolIcon("BeginDrawIntersection",  "Icons/Intersection");
	SetToolIcon("BeginDrawCrosswalk",     "Icons/PedestrianCrossing");
	SetToolIcon("BeginDrawRoundabout",    "Icons/Roundabout");
	SetToolIcon("BeginDrawChevronMarking","Icons/Chevrons");
	SetToolIcon("FitWidth",               "Icons/RoadLaneWidth");
	SetCoreToolIcon("HideSelectedSpline", "Starship/Common/hidden");
	SetCoreToolIcon("UnhideAllSpline",    "Starship/Common/visible");
	SetCoreToolIcon("AttachTo",           "Starship/Common/Export");

	Set("RoadEditor.TileMapWindowVisibility", new IMAGE_BRUSH_SVG("Icons/TileRenderer", Icon24, DefaultForeground));
	Set("RoadEditor.About", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Info", Icon24, DefaultForeground));

	// Road Profile editor preview-mode dropdown (Edit / Preview).
	Set("RoadProfileEditor.EditMode", new IMAGE_BRUSH_SVG("Icons/RoadLaneBuild", Icon16));
	Set("RoadProfileEditor.PreviewMode", new IMAGE_BRUSH_SVG("Icons/Road", Icon16));

	// ---- Component-visualizer command icons (16px) -------------------------------------------
	Set("RoadOffsetComponentVisualize.AddKey", new CORE_IMAGE_BRUSH_SVG("Starship/Common/plus", Icon16));
	Set("RoadOffsetComponentVisualize.DeleteKey", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Delete", Icon16));

	Set("RoadAttributeComponentVisualizerCommands.CreateAttribute", new CORE_IMAGE_BRUSH_SVG("Starship/Common/plus", Icon16));
	Set("RoadAttributeComponentVisualizerCommands.DeleteAttribute", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Delete", Icon16));
	Set("RoadAttributeComponentVisualizerCommands.AddAttributeKey", new CORE_IMAGE_BRUSH_SVG("Starship/Common/plus", Icon16));
	Set("RoadAttributeComponentVisualizerCommands.DeleteAttributeKey", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Delete", Icon16));

	Set("RoadSectionComponentVisualizer.SplitFullSection", new IMAGE_BRUSH_SVG("Icons/SplitFullSection", Icon16));
	Set("RoadSectionComponentVisualizer.SplitSideSection", new IMAGE_BRUSH_SVG("Icons/SplitLeftSection", Icon16));
	Set("RoadSectionComponentVisualizer.SplitLeftSection", new IMAGE_BRUSH_SVG("Icons/SplitLeftSection", Icon16));
	Set("RoadSectionComponentVisualizer.SplitRightSection", new IMAGE_BRUSH_SVG("Icons/SplitRightSection", Icon16));
	Set("RoadSectionComponentVisualizer.DeleteSection", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Delete", Icon16));
	Set("RoadSectionComponentVisualizer.CreatProfile", new IMAGE_BRUSH_SVG("Icons/Road", Icon16));
	Set("RoadSectionComponentVisualizer.AddLaneToLeft", new IMAGE_BRUSH_SVG("Icons/AddLeft", Icon16));
	Set("RoadSectionComponentVisualizer.AddLaneToRight", new IMAGE_BRUSH_SVG("Icons/AddRight", Icon16));
	Set("RoadSectionComponentVisualizer.DeleteLane", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Delete", Icon16));
	Set("RoadSectionComponentVisualizer.ReverseLane", new IMAGE_BRUSH_SVG("Icons/Reverse", Icon16));

	Set("IntersectionDrawToolCommands.LaneTransitionDefault", new IMAGE_BRUSH_SVG("Icons/DirDefault", Icon16));
	Set("IntersectionDrawToolCommands.LaneTransitionNoEntry", new IMAGE_BRUSH_SVG("Icons/DirNoEntry", Icon16));
	Set("IntersectionDrawToolCommands.LaneTransitionForward", new IMAGE_BRUSH_SVG("Icons/DirForward", Icon16));
	Set("IntersectionDrawToolCommands.LaneTransitionLeft", new IMAGE_BRUSH_SVG("Icons/DirLeft", Icon16));
	Set("IntersectionDrawToolCommands.LaneTransitionRight", new IMAGE_BRUSH_SVG("Icons/DirRight", Icon16));
	Set("IntersectionDrawToolCommands.LaneTransitionForwardLeft", new IMAGE_BRUSH_SVG("Icons/DirForwardLeft", Icon16));
	Set("IntersectionDrawToolCommands.LaneTransitionForwardRight", new IMAGE_BRUSH_SVG("Icons/DirForwardRight", Icon16));
	Set("IntersectionDrawToolCommands.LaneTransitionForwardLeftRight", new IMAGE_BRUSH_SVG("Icons/DirAny", Icon16));

	Set("RoadWidthComponentVisualizerCommands.AddWidthKey", new CORE_IMAGE_BRUSH_SVG("Starship/Common/plus", Icon16));
	Set("RoadWidthComponentVisualizerCommands.DeleteWidthKey", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Delete", Icon16));

	//TODO: add icons for DriveSplineComponentVisualizer

	// ---- Misc social icons -------------------------------------------------------------------
	Set("Icons.YouTube", new IMAGE_BRUSH_SVG("Icons/YouTube", Icon16));
	Set("Icons.Discord", new IMAGE_BRUSH_SVG("Icons/Discord", Icon16));
	Set("Icons.Email", new IMAGE_BRUSH_SVG("Icons/Email", Icon16));

	// ---- Asset class icons + thumbnails ------------------------------------------------------
	SetClassIcon("MetaRoad",                                  "Icons/Road");
	SetClassIcon("RoadSplineComponent",                       "Icons/RoadSpline");
	SetClassIcon("ChevronMarkingComponent",                   "Icons/Chevrons");
	SetClassIcon("CrosswalkComponent",                        "Icons/PedestrianCrossing");
	SetClassIcon("RoadMarkProfile",                           "Icons/RoadLaneMark");
	SetClassIcon("RoadCurbProfile",                           "Icons/Curb");
	SetClassIcon("RoadProfile",                               "Icons/Road");
	SetClassIcon("RoadPolygonProfile",                        "Icons/DirForwardRight");
	SetClassIcon("RoadLaneAttributeDescriptor",               "Icons/RoadLaneBuild");
	SetClassIcon("RoadLaneAttributeMark",                     "Icons/RoadLaneMark");
	SetClassIcon("RoadLaneAttributeSpeed",                    "Icons/RoadLaneSpeed");
	SetClassIcon("RoadLaneAttributeSidewalkHeightDescriptor", "Icons/Curb");
	SetClassIcon("RoadLaneAttributeCurbCutDescriptor",        "Icons/CurbCut");
	SetClassIcon("RoadLaneAttributePolygonDescriptor",        "Icons/DirForwardRight");
	SetClassIcon("RoadLaneAttributeLoftingDescriptor",        "Icons/Bridg");
	SetClassIcon("RoadLaneAttributeLandscape",                "Icons/LandscapeBrush");
	SetClassIcon("RoadLaneAttributeSplineMesh",               "Icons/Fence");
	SetClassIcon("RoadLaneAttributeComponentTemplate",        "Icons/Cone");
	SetClassIcon("RoadLaneAttributeActortTemplate",           "Icons/Cones");
}

void FMetaRoadEditorStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}


void FMetaRoadEditorStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}


FMetaRoadEditorStyle& FMetaRoadEditorStyle::Get()
{
	static FMetaRoadEditorStyle Instance;
	return Instance;
}

#undef EDITOR_IMAGE_BRUSH_SVG
#undef EDITOR_IMAGE_BRUSH
