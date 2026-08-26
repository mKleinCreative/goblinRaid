// Console commands for world corruption. Written 2026-08-25, ticket #309.
//
// Stage 1 has NO drivers, so these are the only way the scalar moves at all - and they stay the only
// way to inspect a late stage without playing an entire raid to reach it.
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
//
// FAutoConsoleCommandWithWorld, not UFUNCTION(exec) as ACF's UACFCheatManager uses: this project
// standardised on the former (GSDebugCommands.cpp:79) because it works with no PlayerController,
// and a second mechanism for the same job is a second thing to remember.

#include "World/GSCorruptionSubsystem.h"
#include "World/GSCorruptionDirector.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

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
		UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
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
				TEXT("(stage 1 wires no drivers, so that number will not move on its own yet.)")));
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
