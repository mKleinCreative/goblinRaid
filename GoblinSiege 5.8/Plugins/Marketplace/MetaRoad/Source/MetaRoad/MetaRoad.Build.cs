/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

using System.Collections.Generic;
using UnrealBuildTool;

public class MetaRoad : ModuleRules
{
    public MetaRoad(ReadOnlyTargetRules Target) : base(Target)
    {
        IWYUSupport = IWYUSupport.None;

        // MetaRoad free or pro version
        PublicDefinitions.Add("METAROAD_PRO=0");

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "ZoneGraph",
                "DeveloperSettings",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "InputCore",
                "DeveloperSettings",
                "GeometryCore",
                "GeometryAlgorithms",
                "DynamicMesh",
                "Landscape",
                "Projects",
            }
        );

        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    // UnrealEd: GEditor (settings refresh) + FScopedTransaction (spline edits / CenterSplineOrigins).
                    "UnrealEd",
                }
            );
        }
    }
}
