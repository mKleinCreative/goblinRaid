using UnrealBuildTool;

public class GoblinSiege : ModuleRules
{
	public GoblinSiege(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",

			// Gameplay Ability System - drives attributes, abilities, damage/effects, and tags
			// for every weapon kit and race matchup multiplier.
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",

			// AI
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",

			// Destruction & FX
			"GeometryCollectionEngine",
			"FieldSystemEngine",
			"Chaos",
			"ChaosSolverEngine",
			"Niagara",

			// UI
			"UMG",
			"Slate",
			"SlateCore",
			"CommonUI",

			// Networking (co-op-ready scaffolding; see design doc networking notes)
			"OnlineSubsystem",
			"OnlineSubsystemUtils"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Landscape: UGSBurnMaskSubsystem derives the world burn mask's rectangle from the
			// level's terrain bounds, which needs ALandscapeProxy (2026-07-31). Added with the
			// per-field -> world mask re-architecture.
			//
			// PRIVATE, not public (moved 2026-07-31): ALandscapeProxy is named in exactly one .cpp
			// (GSBurnMaskSubsystem.cpp) and in no header, so nothing that depends on GoblinSiege
			// needs Landscape's include paths. A public dependency propagates to every dependent
			// module and to every PCH that includes ours, which is build time paid for nothing.
			//
			// Deliberately NOT accompanied by "Foliage", even though the whole point of that change
			// was to mark painted foliage: UFoliageInstancedStaticMeshComponent derives from
			// UInstancedStaticMeshComponent -> UStaticMeshComponent -> UMeshComponent, all of which
			// live in Engine, so the subsystem's UMeshComponent sweep binds all ~275,000 wheat
			// instances without ever naming a foliage type. One less module dependency for the same
			// result.
			"Landscape"
		});

		// Uncomment if/when Blueprint-exposed async nodes or editor-only utility code is added.
		// PrivateIncludePathModuleNames.AddRange(new string[] { });

		// Required for the flat module layout (no Public/Private split): UnrealBuildTool only
		// adds Public/Private/Internal subfolders (when present) plus the module's *parent*
		// Source/ dir to the include search paths - never the bare module root. Without this,
		// every module-root-relative include (e.g. "Characters/GSCharacterBase.h") fails with
		// C1083. See goblin-siege-build-log-and-mcp-plan.md.
		PublicIncludePaths.Add(ModuleDirectory);
	}
}
