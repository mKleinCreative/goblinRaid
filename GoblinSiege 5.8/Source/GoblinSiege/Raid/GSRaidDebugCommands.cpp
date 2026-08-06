// GS.Raid.* - the instruments for driving a raid to each of its three endings on demand.
// Written 2026-08-06.
//
// WHY: two of the three raid endings had never been run. Extracted was verified the day the loop
// was written, because you can burn three objectives in two minutes. LeftBehind needs a 30-minute
// clock plus a 90-second collapse window to elapse honestly, and OutOfLives needs five deaths - and
// there was no way to die on demand at all, because damage here is a GameplayEffect and
// UGameplayStatics::ApplyDamage therefore does nothing (six calls, HP stayed at 100).
//
// So both lose paths sat written, shipped, and unexercised. A lose condition nobody has run is a
// lose condition nobody knows works, and these will need re-running every time the raid loop
// changes - which is what makes this worth being a permanent instrument rather than a one-off
// script.
//
// Every command drives the REAL transition. None of them set a result directly: ExpireClock only
// moves the counter and lets TickRaidClock decide the phase; Kill only zeroes the Health attribute
// and lets the normal death chain run. Testing a shortcut would prove nothing about the path a
// player actually takes.

#include "Core/GSGameState.h"
#include "Core/GSPlayerState.h"
#include "Characters/GSCharacterBase.h"
#include "Raid/GSRaidDirector.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace GSRaidDebug
{
	static void Log(const FString& Message)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GoblinSiege] %s"), *Message);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor::Yellow, Message);
		}
	}

	static AGSPlayerState* GetPlayerState(UWorld* World)
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
		return PC ? PC->GetPlayerState<AGSPlayerState>() : nullptr;
	}
}

// ---------------------------------------------------------------------------------- GS.Raid.Status

static FAutoConsoleCommandWithWorld GSRaidStatusCmd(
	TEXT("GS.Raid.Status"),
	TEXT("Print the raid clock, alarm, lives and per-type objective progress."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		if (!World)
		{
			return;
		}

		if (const AGSGameState* GS = World->GetGameState<AGSGameState>())
		{
			GSRaidDebug::Log(FString::Printf(
				TEXT("clock=%d  raid=%.0fs  collapse=%.0fs  alarm=%d"),
				static_cast<int32>(GS->GetRaidClockPhase()), GS->GetRaidSecondsRemaining(),
				GS->GetCollapseSecondsRemaining(), static_cast<int32>(GS->GetAlarmPhase())));
		}

		if (const AGSPlayerState* PS = GSRaidDebug::GetPlayerState(World))
		{
			GSRaidDebug::Log(FString::Printf(TEXT("lives=%d"), PS->GetLives()));
		}

		if (const UGSRaidDirector* Director = UGSRaidDirector::Get(World))
		{
			GSRaidDebug::Log(FString::Printf(TEXT("types %d/%d  objectives complete=%s  raid ended=%s"),
				Director->GetCompletedTypeCount(), Director->GetRequiredTypeCount(),
				Director->AreObjectivesComplete() ? TEXT("yes") : TEXT("no"),
				Director->HasRaidEnded() ? TEXT("yes") : TEXT("no")));
		}
	}));

// ----------------------------------------------------------------------------- GS.Raid.ExpireClock

static FAutoConsoleCommandWithWorld GSRaidExpireClockCmd(
	TEXT("GS.Raid.ExpireClock"),
	TEXT("Fast-forward the raid clock to the edge of its current phase. Run repeatedly to walk "
		 "Running -> FinalWarning -> Collapsing -> Expired (which ends the raid as LeftBehind)."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		if (!World)
		{
			return;
		}

		if (AGSGameState* GS = World->GetGameState<AGSGameState>())
		{
			GS->DebugExpireClock();
		}
		else
		{
			GSRaidDebug::Log(TEXT("GS.Raid.ExpireClock: no AGSGameState - are you in PIE?"));
		}
	}));

// ------------------------------------------------------------------------------------ GS.Raid.Kill

static FAutoConsoleCommandWithWorld GSRaidKillCmd(
	TEXT("GS.Raid.Kill"),
	TEXT("Kill the player outright by zeroing the Health attribute. Spends a life; five of these "
		 "ends the raid as OutOfLives."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		if (!World)
		{
			return;
		}

		if (AGSCharacterBase* Pawn = Cast<AGSCharacterBase>(UGameplayStatics::GetPlayerPawn(World, 0)))
		{
			Pawn->DebugKill();
		}
		else
		{
			// Mid-respawn is the normal reason: RespawnDelaySeconds is 4s and the controller has no
			// pawn during it.
			GSRaidDebug::Log(TEXT("GS.Raid.Kill: no player pawn (mid-respawn?)"));
		}
	}));

// -------------------------------------------------------------------------------- GS.Raid.SetLives

static FAutoConsoleCommandWithWorldAndArgs GSRaidSetLivesCmd(
	TEXT("GS.Raid.SetLives"),
	TEXT("Set the player's remaining lives. 'GS.Raid.SetLives 1' then GS.Raid.Kill reaches "
		 "OutOfLives in one death instead of five."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		const int32 NewLives = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1;
		if (AGSPlayerState* PS = GSRaidDebug::GetPlayerState(World))
		{
			PS->DebugSetLives(NewLives);
		}
		else
		{
			GSRaidDebug::Log(TEXT("GS.Raid.SetLives: no AGSPlayerState"));
		}
	}));
