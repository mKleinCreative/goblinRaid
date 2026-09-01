// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class MyProjectTarget : TargetRules
{
	public MyProjectTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("MyProject");
		ExtraModuleNames.Add("GoblinSiege");

		// bUseLoggingInShipping (tried 2026-08-30, to get a readable log out of the packaged Shipping
		// build) is a DEAD END on this install: it requires BuildEnvironment = Unique (UBT refuses a
		// target whose rules differ from UnrealGame's under a Shared build environment), and
		// "Targets with a unique build environment cannot be built with an installed engine" - this
		// is an installed/Rocket engine, not a source build, and that restriction is absolute here.
		// Reverted. If a live-readable Shipping log is needed again, the real options are: package as
		// DevelopmentClient instead of Shipping (keeps logging/console without a Unique build
		// environment, at the cost of Shipping's size/perf), or build the engine from source.
	}
}
