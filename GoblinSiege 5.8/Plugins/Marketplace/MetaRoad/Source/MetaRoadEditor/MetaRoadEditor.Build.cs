/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

using System.IO;
using UnrealBuildTool;

public class MetaRoadEditor : ModuleRules
{
	public MetaRoadEditor(ReadOnlyTargetRules Target) : base(Target)
	{
        IWYUSupport = IWYUSupport.None;

        PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ComponentVisualizers",
				"MetaRoad",
				"InteractiveToolsFramework",
				"GeometryCore",
				"GeometryFramework",
				"GeometryAlgorithms",
				"DynamicMesh",
				"MeshConversion",
				"MeshDescription",
				"StaticMeshDescription",
				"ModelingComponents",
				"ModelingOperators",
				"ModelingOperatorsEditorOnly",
				"MeshModelingToolsExp",
				"ModelingToolsEditorMode",
				"SceneOutliner",
				"MeshModelingTools",
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
				"UnrealEd",
				"LevelEditor",
				"InputCore",
				"EditorFramework",
				"Kismet",
				"Projects",
				"ApplicationCore",
				"DetailCustomizations",
				"EditorInteractiveToolsFramework",
				"ModelingComponentsEditorOnly",
				"StructUtilsEditor",
				"PropertyEditor",
				"ImageCore",
				"HTTP",
				"RHI",
				"RenderCore",
				"CurveEditor",
                "ZoneGraph",
                "ToolMenus",
                "ToolPresetAsset",
                "ToolPresetEditor",
                "ModelingEditorUI",
                "ModelingUI",
                "ModelingComponentsEditorOnly",
                "WidgetRegistration",
                "StatusBar",
                "ToolWidgets",
                "EditorConfig",
                "AssetTools",
                "ContentBrowser",
                "ContentBrowserData",
                "KismetWidgets",
                "Blutility",
                "AssetDefinition",
                "EngineAssetDefinitions",
                "SceneOutliner",
                "Landscape",
                "Foliage",
                "DesktopPlatform",
                "AdvancedPreviewScene",
            }
		);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "Nanosvg");

		// FBX export: UnFbx::FFbxExporter lives in a private UnrealEd header, and including it pulls in the
		// FBX SDK headers — add the private include path + the FBX third-party dependency.
		PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source", "Editor", "UnrealEd", "Private"));
		AddEngineThirdPartyPrivateStaticDependencies(Target, "FBX");
	}
}
