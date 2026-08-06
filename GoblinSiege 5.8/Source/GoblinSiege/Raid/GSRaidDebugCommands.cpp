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
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Core/GSGameMode.h"

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

// ============================================================================== teleport commands
//
// Playtesting one corner of a 3 km hamlet otherwise begins with a two-minute walk from the portal,
// every single time - and a test you have to walk to is a test that quietly stops being run.
//
// All three snap to ground rather than honouring the requested Z literally. That is deliberate and
// it is the lesson from the spawn bug: a position offset applied without asking the world anything
// put the player 122 uu under a house floor. Exact coordinates are what you asked for; standing on
// the floor is what you meant.

namespace GSRaidDebug
{
	/** Ground-snap a requested point and confirm a player capsule fits. Falls back to the raw
	 *  point (loudly) rather than refusing, since a debug teleport into the void is recoverable and
	 *  a teleport that silently does nothing is not. */
	static FVector GroundSnap(UWorld* World, const FVector& Wanted)
	{
		if (!World)
		{
			return Wanted;
		}

		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GSRaidGoto), false);
		if (APawn* P = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			Params.AddIgnoredActor(P);
		}

		// Start only 500 above the asked-for height, not far overhead - a high start finds the roof
		// of whatever building is there and lands you on it.
		const FVector Start = Wanted + FVector(0.f, 0.f, 500.f);
		const FVector End = Wanted - FVector(0.f, 0.f, 5000.f);
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			return Hit.Location + FVector(0.f, 0.f, 106.f); // capsule half-height + margin
		}

		Log(FString::Printf(TEXT("no ground under (%.0f, %.0f, %.0f) - teleporting there anyway"),
			Wanted.X, Wanted.Y, Wanted.Z));
		return Wanted;
	}

	static void TeleportPlayer(UWorld* World, const FVector& Wanted)
	{
		APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
		if (!Pawn)
		{
			Log(TEXT("no player pawn (mid-respawn?)"));
			return;
		}

		const FVector Where = GroundSnap(World, Wanted);
		Pawn->TeleportTo(Where, Pawn->GetActorRotation());
		Log(FString::Printf(TEXT("teleported to (%.0f, %.0f, %.0f)"), Where.X, Where.Y, Where.Z));
	}
}

// ------------------------------------------------------------------------------------ GS.Raid.Goto

static FAutoConsoleCommandWithWorldAndArgs GSRaidGotoCmd(
	TEXT("GS.Raid.Goto"),
	TEXT("Teleport the player to X Y Z, snapped to the ground there. e.g. GS.Raid.Goto -3471 49012 -514"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}
		if (Args.Num() < 3)
		{
			GSRaidDebug::Log(TEXT("usage: GS.Raid.Goto <X> <Y> <Z>"));
			return;
		}
		GSRaidDebug::TeleportPlayer(World, FVector(FCString::Atof(*Args[0]),
			FCString::Atof(*Args[1]), FCString::Atof(*Args[2])));
	}));

// ------------------------------------------------------------------------------- GS.Raid.GotoActor

static FAutoConsoleCommandWithWorldAndArgs GSRaidGotoActorCmd(
	TEXT("GS.Raid.GotoActor"),
	TEXT("Teleport to the first actor whose label contains this text, standing just outside it. "
		 "e.g. GS.Raid.GotoActor GS_Building_01"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.Num() < 1)
		{
			GSRaidDebug::Log(TEXT("usage: GS.Raid.GotoActor <part of the actor name>"));
			return;
		}

		const FString Needle = Args[0];
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (!A || !A->GetName().Contains(Needle))
			{
				continue;
			}

			// Stand back a little, so teleporting to a wall does not embed you in it.
			const FVector Target = A->GetActorLocation() + FVector(300.f, 300.f, 0.f);
			GSRaidDebug::Log(FString::Printf(TEXT("GotoActor matched '%s'"), *A->GetName()));
			GSRaidDebug::TeleportPlayer(World, Target);
			return;
		}
		GSRaidDebug::Log(FString::Printf(TEXT("no actor matching '%s'"), *Needle));
	}));

// --------------------------------------------------------------------------------- GS.Raid.SpawnAt

static FAutoConsoleCommandWithWorldAndArgs GSRaidSpawnAtCmd(
	TEXT("GS.Raid.SpawnAt"),
	TEXT("Make every spawn AND respawn land at X Y Z instead of the runic site. "
		 "'GS.Raid.SpawnAt off' restores normal spawning. Survives PIE restarts."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() >= 1 && (Args[0].Equals(TEXT("off"), ESearchCase::IgnoreCase)
			|| Args[0].Equals(TEXT("clear"), ESearchCase::IgnoreCase)))
		{
			AGSGameMode::DebugSpawnOverride.Reset();
			GSRaidDebug::Log(TEXT("SpawnAt cleared - spawning returns to the runic site"));
			return;
		}

		if (Args.Num() < 3)
		{
			if (AGSGameMode::DebugSpawnOverride.IsSet())
			{
				const FVector V = AGSGameMode::DebugSpawnOverride.GetValue();
				GSRaidDebug::Log(FString::Printf(TEXT("SpawnAt is (%.0f, %.0f, %.0f)"), V.X, V.Y, V.Z));
			}
			else
			{
				GSRaidDebug::Log(TEXT("SpawnAt is off. usage: GS.Raid.SpawnAt <X> <Y> <Z> | off"));
			}
			return;
		}

		// Snap once, here, so the stored value is already standable and every later respawn reuses
		// it without re-tracing.
		const FVector Wanted(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), FCString::Atof(*Args[2]));
		AGSGameMode::DebugSpawnOverride = GSRaidDebug::GroundSnap(World, Wanted);

		const FVector V = AGSGameMode::DebugSpawnOverride.GetValue();
		GSRaidDebug::Log(FString::Printf(
			TEXT("SpawnAt set to (%.0f, %.0f, %.0f) - every spawn lands here until 'GS.Raid.SpawnAt off'"),
			V.X, V.Y, V.Z));
	}));
