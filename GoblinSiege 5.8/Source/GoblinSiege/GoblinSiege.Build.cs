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

		PrivateDependencyModuleNames.AddRange(new string[] { });

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
