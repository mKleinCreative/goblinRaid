// One switch for "show me what a player would see".
//
// No header on purpose - this file registers console commands and exports nothing. There is a
// sibling of this pattern at Destruction/GSBurnDebugCommands.cpp. Worth knowing: a headerless
// .cpp is invisible to any inventory that lists *.h, which is how 543 lines of existing debug
// tooling went unnoticed for a fortnight.
//
//   GS.PlayerView 1   -> hide every debug overlay this project draws
//   GS.PlayerView 0   -> put them all back
//
// It is a wrapper, not a new system: it just sets the individual cvars, so any of them can still
// be driven on its own when you are debugging one thing in isolation.

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

namespace
{
	/** Every debug toggle in the project, with the value that means "quiet". */
	struct FGSDebugToggle
	{
		const TCHAR* Name;
		int32 QuietValue;
		int32 LoudValue;
	};

	static const FGSDebugToggle GSDebugToggles[] =
	{
		{ TEXT("GS.Combat.Debug"),    0, 1 },  // melee trace spheres, hit markers, combo stage text
		{ TEXT("GS.Combat.LogDamage"), 0, 2 }, // per-hit damage breakdown (2 = also on screen)
		{ TEXT("GS.Burn.Debug"),      0, 1 },  // fire grid overlay and per-character HP readout
		// Added 2026-08-04 with the aim framework. Note what this does NOT hide: the arc RIBBON is a
		// shipping feature drawn with real spline meshes, and a player view is exactly where it
		// belongs. This only silences the raw predicted-path debug lines drawn alongside it.
		{ TEXT("GS.Aim.Debug"),       0, 1 },  // raw PredictProjectilePath lines behind the arc ribbon
		// These two existed before today and were simply never listed here, so GS.PlayerView 1 left
		// them talking. Found while adding the line above.
		{ TEXT("GS.Combat.LogHitReact"), 0, 1 }, // per-flinch selection log
		{ TEXT("GS.Interact.Debug"),  0, 1 },  // focus traces and channel progress readout
	};

	void GSSetPlayerView(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		// Default to the useful direction: bare "GS.PlayerView" means "make it clean".
		const bool bPlayerView = (Args.Num() == 0) || (FCString::Atoi(*Args[0]) != 0);

		int32 Applied = 0;
		for (const FGSDebugToggle& Toggle : GSDebugToggles)
		{
			if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Toggle.Name))
			{
				CVar->Set(bPlayerView ? Toggle.QuietValue : Toggle.LoudValue, ECVF_SetByConsole);
				++Applied;
			}
			// A missing cvar is not an error: these live in different modules and a given one may
			// simply not have been touched this session, so its static has not registered yet.
		}

		// Clear anything already on screen, or the last frame's messages linger for their duration
		// and it looks like the switch did not work.
		if (bPlayerView && GEngine)
		{
			GEngine->ClearOnScreenDebugMessages();
		}

		Ar.Logf(TEXT("GS.PlayerView %d - %d/%d debug channels set (%s)"),
			bPlayerView ? 1 : 0, Applied, UE_ARRAY_COUNT(GSDebugToggles),
			bPlayerView ? TEXT("clean") : TEXT("verbose"));
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GSPlayerViewCmd(
	TEXT("GS.PlayerView"),
	TEXT("GS.PlayerView [0|1] - 1 (default) hides every GoblinSiege debug overlay so you can judge "
		 "the game as a player would; 0 turns them all back on."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&GSSetPlayerView));
