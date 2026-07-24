/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Containers/Map.h"
#include "Framework/Commands/Commands.h"
#include "Templates/SharedPointer.h"

class FName;
class FUICommandInfo;


class FRoadEditorCommands : public TCommands<FRoadEditorCommands>
{

public:
	FRoadEditorCommands();
	virtual void RegisterCommands() override;

public:
	static FName RoadContext;

	// Edit sub-modes
	TSharedPtr<FUICommandInfo> RoadSplineMode;
	TSharedPtr<FUICommandInfo> RoadSectionMode;
	TSharedPtr<FUICommandInfo> RoadOffsetMode;
	TSharedPtr<FUICommandInfo> RoadLaneWidthMode;
	TSharedPtr<FUICommandInfo> RoadAttributeMode;
	TSharedPtr<FUICommandInfo> RoadPresetMode;

	// Bake sub-mode (its panel hosts Bake/Clear Selected/All buttons)
	TSharedPtr<FUICommandInfo> RoadBakeMode;

	// FBX Export sub-mode (its panel hosts the export settings + Export Selected/All buttons)
	TSharedPtr<FUICommandInfo> RoadFbxExportMode;

	// Misk > Visibility sub-mode (its panel hosts UMetaRoadVisibilitySettings)
	TSharedPtr<FUICommandInfo> RoadVisibilityMode;

	//Misks
	TSharedPtr<FUICommandInfo> TileMapWindowVisibility;
	//TSharedPtr<FUICommandInfo> UnhideAllRoadSplines;
	TSharedPtr<FUICommandInfo> HideSelectedSpline;
	TSharedPtr<FUICommandInfo> UnhideAllSpline;
	TSharedPtr<FUICommandInfo> About;
	TSharedPtr<FUICommandInfo> FitWidth;
	TSharedPtr<FUICommandInfo> AttachTo;

	// Tools
	TSharedPtr<FUICommandInfo> MetaRoadToolsTabButton;
	TSharedPtr<FUICommandInfo> BeginDrawNewRoad;
	TSharedPtr<FUICommandInfo> BeginIntersectionSolver;
	TSharedPtr<FUICommandInfo> BeginDrawIntersection;
	TSharedPtr<FUICommandInfo> BeginDrawCrosswalk;
	TSharedPtr<FUICommandInfo> BeginDrawChevronMarking;
	TSharedPtr<FUICommandInfo> BeginDrawRoundabout;


};

