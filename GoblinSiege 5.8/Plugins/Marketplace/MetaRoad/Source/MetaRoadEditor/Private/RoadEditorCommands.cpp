/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadEditorCommands.h"
#include "MetaRoadEditorStyle.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/UICommandInfo.h"
#include "GenericPlatform/GenericApplication.h"
#include "HAL/Platform.h"
#include "InputCoreTypes.h"
#include "Internationalization/Internationalization.h"
#include "Styling/AppStyle.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"
#include "MetaRoadEditorModule.h"

#define LOCTEXT_NAMESPACE "FRoadEditorCommands"

FName FRoadEditorCommands::RoadContext = TEXT("RoadEditor");

FRoadEditorCommands::FRoadEditorCommands()
	: TCommands<FRoadEditorCommands>(
		FRoadEditorCommands::RoadContext, // Context name for fast lookup
		NSLOCTEXT("Contexts", "RoadEditor", "Road Editor"), // Localized context name for displaying
		NAME_None, //"LevelEditor" // Parent
		FMetaRoadEditorStyle::Get().GetStyleSetName() // Icon Style Set
	)
{
}

void FRoadEditorCommands::RegisterCommands()
{

	UI_COMMAND(RoadSplineMode, "Spline", "Road spline editing mode.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(RoadSectionMode, "Section", "Road section editing mode", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(RoadOffsetMode, "Offset", "Center line offset editing mode", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(RoadLaneWidthMode, "Width", "Road lane width editing mode", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(RoadAttributeMode, "Attribute", "Lane attribute editing mode (pick the attribute type in the tree below)", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(RoadPresetMode, "Preset", "Stage mesh-build settings/presets against the live preview, then Apply To Selected (active when road actors are selected)", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(RoadBakeMode, "Bake", "Generate or clear road mesh assets/actors (Bake / Clear, Selected / All).", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(RoadFbxExportMode, "Export", "Export baked road meshes to FBX files (Export Selected / All).", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(RoadVisibilityMode, "Visibility", "Visibility settings (tile renders, unhide splines).", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(MetaRoadToolsTabButton, "Road", "Meta Road Toolset", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(BeginDrawNewRoad, "Spline", "Create a new RoadSplineComponent", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginIntersectionSolver, "Solver", "Intersection Solver", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginDrawIntersection, "Intersection", "Create new Intersection", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginDrawCrosswalk, "Crosswalk", "Create a new UCrosswalkComponent on the selected actor (actor must contain a URoadSplineComponent)", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginDrawChevronMarking, "Chevron", "Create a new UChevronMarkingComponent on the selected actor (actor must contain a URoadSplineComponent)", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginDrawRoundabout, "Roundabout", "Draw a new circular roundabout RoadSplineComponent", EUserInterfaceActionType::ToggleButton, FInputChord());
	
	UI_COMMAND(TileMapWindowVisibility, "Tiles Visibility", "Whether to visualize UTileMapWindowComponent().", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(HideSelectedSpline, "Hide", "Hide selected road spline.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(UnhideAllSpline, "Unhide All", "Unhide all road splines for current actor.", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(FitWidth, "Fit Lanes Width", "Set the road lanes width for the begin and end of this road spline from predecessor and successor connections.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AttachTo, "Attach To", "Reattach road spline(s) to another actor", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(About, "About", "About MetaRoad plugin.", EUserInterfaceActionType::Button, FInputChord());
}


#undef LOCTEXT_NAMESPACE
