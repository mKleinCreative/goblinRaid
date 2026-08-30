// Console commands for world corruption. Written 2026-08-25, ticket #309.
//
// All five drivers are wired as of #327, so the scalar moves on its own - these stay the way to
// inspect a late stage without playing an entire raid to reach it, and to separate "the driver is
// broken" from "nothing has happened yet".
//
// Every command reports WHAT CHANGED, not what was attempted. That rule is inherited verbatim from
// GSBurnDebugCommands.cpp, which was rewritten after a playtest where the harness printed
// "Ignited <field>" six times against a cell that was already ash. A debug tool that reports success
// it did not achieve is worse than no tool.
//
//   GS.Corruption.Set <0..1>  - latch the world at a level; prints old -> new
//   GS.Corruption.Release     - drop the latch; prints what the drivers were at underneath
//   GS.Corruption.Step        - advance to the next quarter band
//   GS.Corruption.Dump        - the scalar, and what the ENGINE has (two failures here are silent)
//   GS.Corruption.Refresh     - re-run director discovery after streaming or a PCG spawn
//   GS.Corruption.Debug 0|1   - a LIVE on-screen readout, refreshed every frame
//
// FAutoConsoleCommandWithWorld, not UFUNCTION(exec) as ACF's UACFCheatManager uses: this project
// standardised on the former (GSDebugCommands.cpp:79) because it works with no PlayerController,
// and a second mechanism for the same job is a second thing to remember.

#include "World/GSCorruptionSubsystem.h"
#include "World/GSCorruptionDirector.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"

namespace GSCorruptionDebug
{
	static UGSCorruptionSubsystem* GetSubsystem(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		return World->GetSubsystem<UGSCorruptionSubsystem>();
	}

	static void Report(const FString& Message)
	{
		// LogGSCorruption, NOT LogTemp. The first version of this file used LogTemp and the entire
		// dump was invisible in Saved/Logs/MyProject.log while showing perfectly on screen - so the
		// instrument could only be read over someone's shoulder, which defeats the point of having
		// one. Every other line this feature emits already uses the feature category; this was the
		// odd one out, and it broke the exact use it exists for.
		UE_LOG(LogGSCorruption, Log, TEXT("%s"), *Message);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor::Orange, Message);
		}
	}

	static void ReportMissing()
	{
		// Distinguishing "no subsystem" from "corruption is zero" matters: they look identical on
		// screen and have completely different causes.
		Report(TEXT("[GS.Corruption] NO SUBSYSTEM in this world. Not a corruption level of zero - "
					"the subsystem does not exist here (editor/preview world, or ShouldCreateSubsystem "
					"refused)."));
	}
}

static FAutoConsoleCommandWithWorldAndArgs GSCorruptionSetCmd(
	TEXT("GS.Corruption.Set"),
	TEXT("GS.Corruption.Set <0..1> - latch the world at a corruption level."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			UGSCorruptionSubsystem* Sub = GSCorruptionDebug::GetSubsystem(World);
			if (!Sub)
			{
				GSCorruptionDebug::ReportMissing();
				return;
			}

			if (Args.Num() < 1)
			{
				GSCorruptionDebug::Report(FString::Printf(
					TEXT("[GS.Corruption] Set needs a value 0..1. Currently %.2f (%s)."),
					Sub->GetCorruption01(), *Sub->GetStageName()));
				return;
			}

			const float Before = Sub->GetCorruption01();
			const float Requested = FMath::Clamp(FCString::Atof(*Args[0]), 0.f, 1.f);
			Sub->SetCorruptionOverride(Requested);

			// Deliberately reports the TARGET, not a new display value: the follower eases toward it
			// over a second or two, so claiming the world is already there would be the exact lie
			// this file's header is about.
			GSCorruptionDebug::Report(FString::Printf(
				TEXT("[GS.Corruption] target %.2f -> %.2f (display is %.2f and easing there)."),
				Before, Requested, Sub->GetCorruption01()));
		}));

static FAutoConsoleCommandWithWorld GSCorruptionReleaseCmd(
	TEXT("GS.Corruption.Release"),
	TEXT("Drop the console latch and hand the world back to the drivers."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld* World)
		{
			UGSCorruptionSubsystem* Sub = GSCorruptionDebug::GetSubsystem(World);
			if (!Sub)
			{
				GSCorruptionDebug::ReportMissing();
				return;
			}

			const bool bWas = Sub->IsOverridden();
			Sub->ReleaseCorruptionOverride();

			GSCorruptionDebug::Report(FString::Printf(
				TEXT("[GS.Corruption] override %s. Drivers underneath are at %.2f. %s"),
				bWas ? TEXT("RELEASED") : TEXT("was not latched - nothing changed"),
				Sub->GetCorruptionTarget01(),
				TEXT("(the drivers are live, so that number moves on its own from here.)")));
		}));

static FAutoConsoleCommandWithWorld GSCorruptionStepCmd(
	TEXT("GS.Corruption.Step"),
	TEXT("Advance to the next quarter band - inspect any stage without playing to it."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld* World)
		{
			UGSCorruptionSubsystem* Sub = GSCorruptionDebug::GetSubsystem(World);
			if (!Sub)
			{
				GSCorruptionDebug::ReportMissing();
				return;
			}

			const float Before = Sub->GetCorruption01();
			Sub->StepCorruptionStage();
			GSCorruptionDebug::Report(FString::Printf(
				TEXT("[GS.Corruption] step: %.2f -> target %.2f."),
				Before, Sub->GetCorruptionTarget01()));
		}));

static FAutoConsoleCommandWithWorld GSCorruptionDumpCmd(
	TEXT("GS.Corruption.Dump"),
	TEXT("The scalar, and what the engine actually has. Reports silent failures."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld* World)
		{
			UGSCorruptionSubsystem* Sub = GSCorruptionDebug::GetSubsystem(World);
			if (!Sub)
			{
				GSCorruptionDebug::ReportMissing();
				return;
			}

			// Printed line by line so the on-screen overlay does not collapse it into one blob.
			const FString State = Sub->DescribeState();
			TArray<FString> Lines;
			State.ParseIntoArrayLines(Lines);
			for (const FString& Line : Lines)
			{
				GSCorruptionDebug::Report(Line);
			}
		}));

static FAutoConsoleCommandWithWorld GSCorruptionRefreshCmd(
	TEXT("GS.Corruption.Refresh"),
	TEXT("Re-run the director's actor discovery after streaming or a PCG spawn."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld* World)
		{
			UGSCorruptionSubsystem* Sub = GSCorruptionDebug::GetSubsystem(World);
			if (!Sub)
			{
				GSCorruptionDebug::ReportMissing();
				return;
			}

			Sub->RefreshOutputs();
			GSCorruptionDebug::Report(TEXT("[GS.Corruption] discovery re-run:"));

			if (const AGSCorruptionDirector* D = Sub->GetDirector())
			{
				TArray<FString> Lines;
				D->DescribeOutputs().ParseIntoArrayLines(Lines);
				for (const FString& Line : Lines)
				{
					GSCorruptionDebug::Report(Line);
				}
			}
			else
			{
				GSCorruptionDebug::Report(
					TEXT("[GS.Corruption]   still NO DIRECTOR - nothing is being driven."));
			}
		}));

// ---------------------------------------------------------------------------------------------
// GS.Corruption.Debug - the live readout.
//
// Specified in the original plan and skipped in stage 1, which cost real time: every check of this
// feature so far has meant typing GS.Corruption.Dump repeatedly and comparing snapshots by eye,
// and a value that MOVES is exactly the thing a snapshot cannot show. Dump answers "what is it now";
// this answers "what is it doing".
//
// A console COMMAND that registers a ticker, not a cvar polled from somewhere: a cvar would need
// something already ticking to read it, and the alternative - a static ticker registered at load -
// depends on static-init order in a game module. Registering on demand has neither problem, and
// unregistering on 0 means the disabled cost is exactly zero rather than a per-frame branch.
// ---------------------------------------------------------------------------------------------

namespace GSCorruptionDebug
{
	static FTSTicker::FDelegateHandle DebugTickerHandle;

	/** The PIE/game world, since a ticker has no world of its own. */
	static UWorld* FindGameWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* W = Context.World();
			if (W && W->IsGameWorld())
			{
				return W;
			}
		}
		return nullptr;
	}

	static FColor StageColour(int32 Stage)
	{
		switch (Stage)
		{
			case 0:  return FColor(140, 200, 140);   // Quiet   - green
			case 1:  return FColor(220, 200, 120);   // Scarred - straw
			case 2:  return FColor(230, 140, 60);    // Burning - ember
			default: return FColor(200, 60, 40);     // Mordor  - blood
		}
	}

	static FString Bar(float Value01, int32 Width)
	{
		// ASCII only. This project has been bitten by non-ASCII in tooling before, and a debug bar
		// is not worth finding out whether the log encoding agrees with the viewport font.
		const int32 Filled = FMath::Clamp(FMath::RoundToInt(Value01 * Width), 0, Width);
		return FString::ChrN(Filled, TEXT('#')) + FString::ChrN(Width - Filled, TEXT('.'));
	}

	static bool TickDebugOverlay(float /*DeltaSeconds*/)
	{
		UWorld* World = FindGameWorld();
		UGSCorruptionSubsystem* Sub = World ? World->GetSubsystem<UGSCorruptionSubsystem>() : nullptr;
		if (!Sub || !GEngine)
		{
			// Keep ticking - PIE may not have started yet, and silently unregistering here would make
			// the overlay "not work" for anyone who enabled it before pressing Play.
			return true;
		}

		const float Display = Sub->GetCorruption01();
		const float Target = Sub->GetCorruptionTarget01();
		const int32 Stage = Sub->GetCorruptionStage();

		// Stable keys so each line REPLACES itself every frame instead of stacking. -1 would append.
		static const int32 KeyBase = 0x605B11;

		GEngine->AddOnScreenDebugMessage(KeyBase, 0.f, StageColour(Stage),
			FString::Printf(TEXT("CORRUPTION  [%s] %.2f  ->%.2f  %s%s"),
				*Bar(Display, 24), Display, Target, *Sub->GetStageName(),
				Sub->IsOverridden() ? TEXT("  (OVERRIDDEN)") : TEXT("")));

		// The per-term working, reusing DescribeState so there is ONE definition of what a term is.
		// A second, prettier copy of the same maths is a second thing to keep true.
		TArray<FString> Lines;
		Sub->DescribeState().ParseIntoArrayLines(Lines);
		int32 Key = KeyBase + 1;
		for (const FString& Line : Lines)
		{
			if (Line.Contains(TEXT("x ")) || Line.Contains(TEXT("of the term")))
			{
				GEngine->AddOnScreenDebugMessage(Key++, 0.f, FColor(170, 170, 170), Line);
			}
		}

		return true;
	}
}

static FAutoConsoleCommandWithWorldAndArgs GSCorruptionDebugCmd(
	TEXT("GS.Corruption.Debug"),
	TEXT("GS.Corruption.Debug 0|1 - live on-screen corruption bar and per-term readout."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			const bool bEnable = (Args.Num() < 1) || (FCString::Atoi(*Args[0]) != 0);

			if (bEnable && !GSCorruptionDebug::DebugTickerHandle.IsValid())
			{
				GSCorruptionDebug::DebugTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
					FTickerDelegate::CreateStatic(&GSCorruptionDebug::TickDebugOverlay), 0.f);
				GSCorruptionDebug::Report(TEXT("[GS.Corruption] live overlay ON."));
			}
			else if (!bEnable && GSCorruptionDebug::DebugTickerHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(GSCorruptionDebug::DebugTickerHandle);
				GSCorruptionDebug::DebugTickerHandle.Reset();

				// Deliberately NOT GEngine->ClearOnScreenDebugMessages() - that wipes every system's
				// on-screen output, not ours, and turning off one overlay has no business blanking
				// somebody else's. Our lines are drawn with a 0s lifetime and re-added each frame, so
				// they disappear on their own the moment this ticker stops.
				GSCorruptionDebug::Report(TEXT("[GS.Corruption] live overlay OFF."));
			}
			else
			{
				// Reports the STATE, not the request - the rule this whole file is built on.
				GSCorruptionDebug::Report(FString::Printf(TEXT("[GS.Corruption] live overlay already %s."),
					bEnable ? TEXT("ON") : TEXT("OFF")));
			}
		}));
