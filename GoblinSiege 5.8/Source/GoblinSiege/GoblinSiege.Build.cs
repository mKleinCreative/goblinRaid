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

			// Ascent Combat Framework - the AI half (#142, ACF migration Phase 0).
			//
			// Michael's ruling 2026-08-12: the project adopts ACF rather than hand-building
			// parallel systems. The horde's order board in #141 turned out to be a re-implementation
			// of UACFCommandsManagerComponent + UACFGroupAIComponent::SendCommandToCompanions, and
			// the patrol director still on AGENT_STATE's NEXT list is UACFAIPatrolComponent.
			//
			// Only AIFramework is named. Its own PublicDependencyModuleNames carry the rest
			// transitively - AscentCombatFramework (AACFCharacter), AscentCoreInterfaces,
			// AscentTargetingSystem, AscentGASRuntime, ActionsSystem, InventorySystem, AscentTeams,
			// SmartObjectsModule - so listing them here would be noise that hides which one we
			// actually reach for.
			//
			// PUBLIC rather than private: the migration puts ACF types in our HEADERS
			// (AGSAIControllerBase : AACFAIController, and later AGSCharacterBase : AACFCharacter),
			// so every dependent needs the include paths. That is the opposite of the Landscape /
			// AnimGraphRuntime reasoning below, and deliberately so.
			"AIFramework",

			// AscentCombatFramework is named EXPLICITLY, and the paragraph above is wrong about why
			// it did not need to be (#166, 2026-08-17). A transitive dependency gives you the INCLUDE
			// PATHS, so `#include "Components/ACFDestructableComponent.h"` compiles happily - and then
			// the link fails with LNK2019 on every ACF symbol you actually called. The distinction is
			// invisible until the first time you call into ACF rather than merely deriving from a type
			// it re-exports.
			//
			// Reached for directly: UACFDestructableComponent (Chaos destruction, #165) and
			// FACFDamageEvent. Add the next ACF module here the moment you CALL into it.
			"AscentCombatFramework",

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
			"Landscape",

			// AnimGraphRuntime: GS.Anim.Snapshot reports each pawn's movement Direction, and it must
			// report the number the ANIM GRAPH sees, not a lookalike. UKismetAnimationLibrary::
			// CalculateDirection is the exact node both AnimBPs call to drive their blendspace
			// Direction axis, so calling it here makes the diagnostic agree with the thing being
			// diagnosed by construction. Re-deriving the maths locally would compile without this
			// dependency and would be a second implementation of the one value the instrument exists
			// to report - i.e. an instrument that can drift away from the truth it is checking, which
			// AGENT_STATE records as worse than no instrument at all ("a lying instrument is worse
			// than dead code", the climb accumulator that read 0.000 for 1285 ticks).
			//
			// PRIVATE for the same reason as Landscape above: named in one .cpp, in no header.
			"AnimGraphRuntime"
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
