using UnrealBuildTool;

public class GoblinSiegeEditor : ModuleRules
{
	public GoblinSiegeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",

			// Bulk fracture generation (#393). Named explicitly, not left to a transitive include -
			// GoblinSiege.Build.cs already documents the trap (#166/#280/#390): a module reached only
			// through another module's PublicDependencyModuleNames gives you the include paths and
			// then fails to LINK the moment you actually call into it.
			//
			// GeometryCollectionEngine: FGeometryCollectionEngineConversion::
			// ConvertStaticMeshToGeometryCollection (StaticMesh -> FGeometryCollection) and
			// UGeometryCollection itself.
			"GeometryCollectionEngine",
			// Chaos: FGeometryCollection's actual home (confirmed via header search, 2026-08-31 -
			// it is NOT in GeometryCollectionEngine despite the module name suggesting otherwise).
			"Chaos",
			// FractureEngine (Engine/Plugins/Experimental/Fracture, enabled in MyProject.uproject for
			// this purpose): FFractureEngineFracturing::UniformFracture, the same call
			// DF_GS_HouseFracture's FUniformFractureDataflowNode wraps for the one hand-made
			// GC_MERGED_House_Small_03 - this reaches the identical algorithm without needing a human
			// to drive the Dataflow graph editor UI per mesh.
			"FractureEngine",
			// PlanarCut (Engine/Plugins/Experimental/PlanarCutPlugin, also enabled): FractureEngine's
			// own FractureEngineFracturing.h #includes PlanarCut.h directly for FNoiseSettings, part
			// of FUniformFractureSettings.
			"PlanarCut",
			// DataflowCore (Engine/Source/Runtime/Dataflow/Core - a base runtime module, not gated by
			// the Dataflow plugin's Enabled state): FDataflowTransformSelection, which
			// UniformFracture's second parameter requires.
			"DataflowCore",

			"AssetRegistry",
		});
	}
}
