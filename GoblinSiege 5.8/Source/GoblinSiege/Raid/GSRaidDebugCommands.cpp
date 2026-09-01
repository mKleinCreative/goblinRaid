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
#include "Raid/GSWarren.h"
#include "Raid/GSScoreSubsystem.h"
#include "Horde/GSHordeSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Core/GSGameMode.h"
#include "Destruction/GSBuildingObjective.h"
#include "Destruction/GSBurnObjectiveBase.h"
#include "Destruction/GSFlammableComponent.h"
#include "Destruction/GSBurnFXComponent.h"
#include "Destruction/GSTopplableComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UObjectIterator.h"

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

	/**
	 * The world the GAME is running in, whatever world the console handed us.
	 *
	 * FAutoConsoleCommandWithWorld passes the world the command was typed in, and the editor's
	 * Output Log console is not the PIE world - so every one of these commands silently did nothing
	 * when run from there: no pawn to teleport, no GameState to read, and (worst) no log line saying
	 * why. Four attempts at GS.Raid.GotoActor produced zero output for exactly this reason.
	 *
	 * Resolving PIE/Game explicitly means it does not matter which console you use.
	 */
	static UWorld* GameWorld(UWorld* Fallback)
	{
		if (GEngine)
		{
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				if ((Ctx.WorldType == EWorldType::PIE || Ctx.WorldType == EWorldType::Game) && Ctx.World())
				{
					return Ctx.World();
				}
			}
		}
		return Fallback;
	}

	static AGSPlayerState* GetPlayerState(UWorld* World)
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
		return PC ? PC->GetPlayerState<AGSPlayerState>() : nullptr;
	}
}

// --------------------------------------------------------------------------------- GS.Warren.Status
//
// The readout the Warren ships with, per the project's own rule that a system without a runtime
// instrument gets debugged blind. Three questions it answers that look identical in play and have
// completely different causes: is there a Warren at all, is the horde using it, and has anything
// ever banked through it.

static FAutoConsoleCommandWithWorld GSWarrenStatusCmd(
	TEXT("GS.Warren.Status"),
	TEXT("List every Warren, what each is serving, and what has banked through it."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* InWorld)
	{
		UWorld* World = GSRaidDebug::GameWorld(InWorld);
		if (!World)
		{
			return;
		}

		int32 Count = 0;
		for (TActorIterator<AGSWarren> It(World); It; ++It)
		{
			if (const AGSWarren* Warren = *It)
			{
				GSRaidDebug::Log(Warren->DescribeStatus());
				++Count;
			}
		}

		if (Count == 0)
		{
			// The single most likely reason a correctly built Warren appears to do nothing, and it is
			// indistinguishable in play from a broken one. Same warning shape FindArrivalTransform
			// gives for a level with no arrival markers.
			GSRaidDebug::Log(TEXT("No AGSWarren in this level. The horde falls back to ")
				TEXT("Marker.HordeArrival, respawn falls back to the runic site, and nothing banks loot."));
		}

		// The authoritative total, which is NOT the sum of the per-Warren tallies once the runic site
		// becomes the second banking consumer.
		if (const UGSScoreSubsystem* Score = World->GetSubsystem<UGSScoreSubsystem>())
		{
			GSRaidDebug::Log(FString::Printf(TEXT("score: loot=%d (source seen: %s)  deeds=%d"),
				Score->GetLoot(), Score->HasLootSource() ? TEXT("yes") : TEXT("no"), Score->GetDeeds()));
		}

		if (const UGSHordeSubsystem* Horde = UGSHordeSubsystem::Get(World))
		{
			GSRaidDebug::Log(FString::Printf(TEXT("horde: reserve=%d  active=%d/%d  summonable now=%d"),
				Horde->GetReserveRemaining(), Horde->GetActiveCount(), Horde->GetActiveCap(),
				Horde->GetSummonableNow()));
		}
	}));

// ---------------------------------------------------------------------------------- GS.Raid.Status

static FAutoConsoleCommandWithWorld GSRaidStatusCmd(
	TEXT("GS.Raid.Status"),
	TEXT("Print the raid clock, alarm, lives and per-type objective progress."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* InWorld)
	{
		UWorld* World = GSRaidDebug::GameWorld(InWorld);
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
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* InWorld)
	{
		UWorld* World = GSRaidDebug::GameWorld(InWorld);
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
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* InWorld)
	{
		UWorld* World = GSRaidDebug::GameWorld(InWorld);
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
		[](const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = GSRaidDebug::GameWorld(InWorld);
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
		const FRotator Facing = Pawn->GetActorRotation();

		// CHECK THE RETURN VALUE. TeleportTo refuses when the destination capsule is blocked, and the
		// first version of this ignored that and logged success regardless - so the command cheerfully
		// reported "teleported to (-18154, 73815, 2642)" while the pawn had not moved a centimetre.
		// A debug tool that lies about what it did is worse than one that does nothing, because you
		// spend the next hour debugging the wrong thing.
		bool bMoved = Pawn->TeleportTo(Where, Facing);

		// Blocked at ground level is normal near a wall or under eaves. Try progressively higher
		// before giving up - falling a short way is fine, being stuck is not.
		if (!bMoved)
		{
			for (const float Up : { 200.f, 500.f, 1000.f })
			{
				if (Pawn->TeleportTo(Where + FVector(0.f, 0.f, Up), Facing))
				{
					bMoved = true;
					Log(FString::Printf(TEXT("ground was blocked; dropped in from %.0f uu up"), Up));
					break;
				}
			}
		}

		// Last resort: move it regardless. Clipping into a wall is recoverable and visible; silently
		// not moving is neither.
		if (!bMoved)
		{
			Pawn->SetActorLocation(Where, false, nullptr, ETeleportType::TeleportPhysics);
			Log(TEXT("every teleport attempt was blocked - forced the move; you may be inside geometry"));
		}

		const FVector Actual = Pawn->GetActorLocation();
		Log(FString::Printf(TEXT("%s -> now at (%.0f, %.0f, %.0f)"),
			bMoved ? TEXT("teleported") : TEXT("FORCED"), Actual.X, Actual.Y, Actual.Z));
	}
}

// ------------------------------------------------------------------------------------ GS.Raid.Goto

static FAutoConsoleCommandWithWorldAndArgs GSRaidGotoCmd(
	TEXT("GS.Raid.Goto"),
	TEXT("Teleport the player to X Y Z, snapped to the ground there. e.g. GS.Raid.Goto -3471 49012 -514"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = GSRaidDebug::GameWorld(InWorld);
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
		[](const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = GSRaidDebug::GameWorld(InWorld);
		if (!World || Args.Num() < 1)
		{
			GSRaidDebug::Log(TEXT("usage: GS.Raid.GotoActor <part of the actor name>"));
			return;
		}

		const FString Needle = Args[0];
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			// GetActorNameOrLabel: the editor label in editor builds, the object name otherwise.
			// Matching GetName() alone was why 'GS_Building_00' never matched anything - at
			// runtime that actor is called GSBuildingObjective_81.
			if (!A || !A->GetActorNameOrLabel().Contains(Needle))
			{
				continue;
			}

			// Stand back a little, so teleporting to a wall does not embed you in it.
			const FVector Target = A->GetActorLocation() + FVector(300.f, 300.f, 0.f);
			GSRaidDebug::Log(FString::Printf(TEXT("GotoActor matched '%s'"), *A->GetActorNameOrLabel()));
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
		[](const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = GSRaidDebug::GameWorld(InWorld);
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

// ============================================================================== building commands

namespace GSRaidDebug
{
	/** The building nearest the player, or the Nth largest if the player has no pawn. */
	static AGSBuildingObjective* PickBuilding(UWorld* World, int32 Index)
	{
		TArray<AGSBuildingObjective*> All;
		for (TActorIterator<AGSBuildingObjective> It(World); It; ++It)
		{
			if (*It)
			{
				All.Add(*It);
			}
		}
		if (All.Num() == 0)
		{
			Log(TEXT("no AGSBuildingObjective in this level"));
			return nullptr;
		}

		// Biggest first: a 54-piece house is a far better place to judge fire than a 13-piece shed.
		All.Sort([](const AGSBuildingObjective& A, const AGSBuildingObjective& B)
		{
			return A.GetPieceCount() > B.GetPieceCount();
		});
		return All[FMath::Clamp(Index, 0, All.Num() - 1)];
	}
}

// ---------------------------------------------------------------------------- GS.Raid.GotoBuilding

static FAutoConsoleCommandWithWorldAndArgs GSRaidGotoBuildingCmd(
	TEXT("GS.Raid.GotoBuilding"),
	TEXT("Teleport to a burnable building, standing back far enough to see the whole thing. "
		 "Optional index, 0 = biggest. Buildings are sorted largest first."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = GSRaidDebug::GameWorld(InWorld);
		if (!World)
		{
			return;
		}

		const int32 Index = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
		AGSBuildingObjective* B = GSRaidDebug::PickBuilding(World, Index);
		if (!B)
		{
			return;
		}

		const FVector L = B->GetActorLocation();
		GSRaidDebug::Log(FString::Printf(TEXT("%s: %d pieces, alight=%s, %.0f%% burnt"),
			*B->GetActorNameOrLabel(), B->GetPieceCount(),
			B->IsAlight() ? TEXT("yes") : TEXT("no"), B->GetCompletion01() * 100.f));

		// Stand off far enough that the whole house is in frame, not inside its front room.
		GSRaidDebug::TeleportPlayer(World, L + FVector(1100.f, 1100.f, 0.f));
	}));

// -------------------------------------------------------------------------------- GS.Raid.BurnHere

static FAutoConsoleCommandWithWorld GSRaidBurnHereCmd(
	TEXT("GS.Raid.BurnHere"),
	TEXT("Set the nearest building alight, as though a torch had gone through its window."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* InWorld)
	{
		UWorld* World = GSRaidDebug::GameWorld(InWorld);
		if (!World)
		{
			return;
		}

		APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
		if (!Pawn)
		{
			GSRaidDebug::Log(TEXT("no player pawn"));
			return;
		}

		AGSBuildingObjective* Nearest = nullptr;
		float BestSq = TNumericLimits<float>::Max();
		for (TActorIterator<AGSBuildingObjective> It(World); It; ++It)
		{
			AGSBuildingObjective* B = *It;
			if (!B)
			{
				continue;
			}
			const float D = FVector::DistSquared(B->GetActorLocation(), Pawn->GetActorLocation());
			if (D < BestSq)
			{
				BestSq = D;
				Nearest = B;
			}
		}

		if (!Nearest)
		{
			GSRaidDebug::Log(TEXT("no building nearby"));
			return;
		}

		Nearest->IgniteInterior(EGSBuildingIgnitionSource::Window);
		GSRaidDebug::Log(FString::Printf(TEXT("lit %s (%.0f m away, %d pieces)"),
			*Nearest->GetActorNameOrLabel(), FMath::Sqrt(BestSq) / 100.f, Nearest->GetPieceCount()));
	}));

// -------------------------------------------------------------------------- GS.Raid.BuildingStatus

static FAutoConsoleCommandWithWorld GSRaidBuildingStatusCmd(
	TEXT("GS.Raid.BuildingStatus"),
	TEXT("Report the nearest building's pieces: how many are burning, how many burnt, and the actual "
		 "GS_BurnAmount on their materials - which is what decides whether char is VISIBLE."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* InWorld)
	{
		UWorld* World = GSRaidDebug::GameWorld(InWorld);
		if (!World)
		{
			return;
		}

		APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
		if (!Pawn)
		{
			// Without a pawn every distance below is 0, so "nearest" silently became "whichever the
			// actor iterator happened to return first" while the help text promised the nearest.
			// GS.Raid.BurnHere directly above bails out loudly in exactly this situation.
			GSRaidDebug::Log(TEXT("no player pawn - cannot tell which building is nearest. Use GS.Raid.GotoBuilding <n> first."));
			return;
		}

		AGSBuildingObjective* Nearest = nullptr;
		float BestSq = TNumericLimits<float>::Max();
		for (TActorIterator<AGSBuildingObjective> It(World); It; ++It)
		{
			if (AGSBuildingObjective* B = *It)
			{
				const float D = FVector::DistSquared(B->GetActorLocation(), Pawn->GetActorLocation());
				if (D < BestSq) { BestSq = D; Nearest = B; }
			}
		}
		if (!Nearest)
		{
			GSRaidDebug::Log(TEXT("no building found"));
			return;
		}

		// Walk the pieces around it and report the three things that can independently be wrong:
		// no flammable (cannot burn), no FX component (burns invisibly), or a material with no
		// GS_BurnAmount (FX runs and paints nothing).
		int32 Pieces = 0, WithFlam = 0, WithFX = 0, Burning = 0, Burnt = 0, WithParam = 0;
		float MaxBurn = 0.f;

		// Ask the building which pieces are ITS pieces, rather than re-deriving them here. The old
		// sweep took "any actor with a flammable within 2500 uu of GetActorLocation()", which is
		// wrong three ways at once: it measures pivot-to-pivot (the bug 270d107 fixed in AdoptPieces,
		// where this kit offsets meshes up to 671 uu from their pivot), 2500 is unrelated to this
		// building's AdoptRadius, and it counts the neighbours. On a tavern spanning more than
		// 2500 uu it under-counted; between two close houses it reported the wrong house's pieces.
		// A diagnostic whose whole job is "prove char is being applied to THIS building" must not
		// describe a different set of actors from the one the building scores.
		for (const TWeakObjectPtr<AActor>& Weak : Nearest->GetPieces())
		{
			AActor* A = Weak.Get();
			if (!A)
			{
				continue;
			}
			UGSFlammableComponent* F = A->FindComponentByClass<UGSFlammableComponent>();
			++Pieces;
			if (!F)
			{
				// Counted, not skipped: an adopted piece with no flammable is one of the three
				// failures this command exists to surface, and the old radius sweep could not see it
				// at all because a missing flammable was its filter for "not a piece".
				continue;
			}
			++WithFlam;
			if (A->FindComponentByClass<UGSBurnFXComponent>()) { ++WithFX; }
			if (F->IsBurning()) { ++Burning; }
			if (F->HasBurnedDown()) { ++Burnt; }

			if (UStaticMeshComponent* M = A->FindComponentByClass<UStaticMeshComponent>())
			{
				for (int32 i = 0; i < M->GetNumMaterials(); ++i)
				{
					if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(M->GetMaterial(i)))
					{
						float V = 0.f;
						if (MID->GetScalarParameterValue(FName("GS_BurnAmount"), V))
						{
							++WithParam;
							MaxBurn = FMath::Max(MaxBurn, V);
						}
					}
				}
			}
		}

		GSRaidDebug::Log(FString::Printf(TEXT("%s alight=%s %.0f%% complete"),
			*Nearest->GetActorNameOrLabel(), Nearest->IsAlight() ? TEXT("yes") : TEXT("no"),
			Nearest->GetCompletion01() * 100.f));
		GSRaidDebug::Log(FString::Printf(
			TEXT("pieces=%d  flammable=%d  burnFX=%d  burning=%d  burnt=%d"),
			Pieces, WithFlam, WithFX, Burning, Burnt));
		GSRaidDebug::Log(FString::Printf(
			TEXT("MIDs exposing GS_BurnAmount=%d   highest value seen=%.2f  %s"),
			WithParam, MaxBurn,
			WithParam == 0 ? TEXT("<- no MIDs: char CANNOT show")
				: (MaxBurn <= 0.01f ? TEXT("<- param exists but is still 0: FX not driving it")
					: TEXT("<- char is being applied"))));
	}));

// -------------------------------------------------------------------- GS.Raid.CompleteAllObjectives
//
// The complement to GS.Burn.IgniteAll (Destruction/GSBurnDebugCommands.cpp): that one starts every
// objective burning but does not guarantee any of them REACHES its completion threshold (a field
// spreads over time, a building burns its shell down, the mill runs a fuse) - so there was still no
// way to drive the raid to "won" on demand. This jumps every burn objective straight to 1.0 via the
// new AGSBurnObjectiveBase::DebugForceComplete() and topples every monument, then lets the EXISTING
// UGSRaidDirector/AGSRunicSite cascade (HandleCarrierCompleted/HandleMonumentToppled ->
// EvaluateWinCondition -> OnRaidObjectivesComplete -> SetPortalOpen) do the rest - no changes to
// either of those classes were needed.
static FAutoConsoleCommandWithWorld GSRaidCompleteAllObjectivesCmd(
	TEXT("GS.Raid.CompleteAllObjectives"),
	TEXT("Force every burn objective and every monument to completion, so the win-condition cascade ")
		 TEXT("(director -> runic site portal) can be exercised without playing through the raid."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* InWorld)
	{
		UWorld* World = GSRaidDebug::GameWorld(InWorld);
		if (!World)
		{
			return;
		}

		int32 BurnCount = 0;
		for (TActorIterator<AGSBurnObjectiveBase> It(World); It; ++It)
		{
			if (AGSBurnObjectiveBase* Objective = *It)
			{
				if (!Objective->IsComplete())
				{
					Objective->DebugForceComplete();
					++BurnCount;
				}
			}
		}

		int32 ToppleCount = 0;
		for (TObjectIterator<UGSTopplableComponent> It; It; ++It)
		{
			UGSTopplableComponent* Topplable = *It;
			if (!IsValid(Topplable) || Topplable->GetWorld() != World)
			{
				continue;
			}
			if (!Topplable->IsToppled())
			{
				AActor* Owner = Topplable->GetOwner();
				const FVector PullDirection = Owner ? Owner->GetActorForwardVector() : FVector::ForwardVector;
				const FVector AnchorPoint = Owner ? Owner->GetActorLocation() + FVector(0.f, 0.f, 200.f) : FVector::ZeroVector;
				if (Topplable->Topple(nullptr, PullDirection, AnchorPoint))
				{
					++ToppleCount;
				}
			}
		}

		GSRaidDebug::Log(FString::Printf(
			TEXT("CompleteAllObjectives: forced %d burn objective(s), toppled %d monument(s)."),
			BurnCount, ToppleCount));
	}));
