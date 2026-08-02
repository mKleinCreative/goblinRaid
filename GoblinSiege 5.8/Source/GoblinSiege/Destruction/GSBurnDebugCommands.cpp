// Console commands for playtesting the three burn types without depending on the torch verb, the
// bucket-brigade AI, or any FX existing yet. Written 2026-07-28.
//
// Revised 2026-07-30 after a playtest where the harness LIED twice:
//   * "Ignited <field>" was printed even when the target cell was ash and nothing happened, because
//     ForceIgnite returned true unconditionally. Six ignite attempts in a row were all no-ops.
//   * "nothing is burning" was printed while a market stall was alight, because Douse only ever
//     iterated field objectives and never looked at markets.
// Every command below now reports what actually changed, not what was attempted. A debug tool that
// reports success it did not achieve is worse than no tool.
//
//   GS.Burn.Debug 1        - the overlay. Green unburnt, orange burning, dark ash, BLUE doused.
//   GS.Burn.Status         - per-objective completion and cell/stall breakdown
//   GS.Burn.Ignite         - light the objective you're standing in (else the nearest)
//   GS.Burn.IgniteAll      - light everything at once
//   GS.Burn.Douse [radius] - douse near the player: field cells AND market stalls
//   GS.Burn.Firebreak      - douse a LINE across the field ahead of the front
//   GS.Burn.Dry            - instantly dry all doused cells (skip the dry-out wait)

#include "Destruction/GSBurnObjectiveBase.h"
#include "Destruction/GSFieldFireObjective.h"
#include "Destruction/GSMarketObjective.h"
#include "Destruction/GSMillObjective.h"
#include "Characters/GSCharacterBase.h"
#include "Characters/GSEnemyCharacter.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

namespace GSBurnDebug
{
	static bool GetPlayerLocation(UWorld* World, FVector& OutLocation)
	{
		if (!World)
		{
			return false;
		}

		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				OutLocation = Pawn->GetActorLocation();
				return true;
			}
			if (AActor* ViewTarget = PC->GetViewTarget())
			{
				OutLocation = ViewTarget->GetActorLocation();
				return true;
			}
		}

		return false;
	}

	static void Log(const FString& Message)
	{
		UE_LOG(LogTemp, Display, TEXT("[GS.Burn] %s"), *Message);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 7.f, FColor::Yellow, Message);
		}
	}

	static const TCHAR* CellStateName(EGSFieldCellState State)
	{
		switch (State)
		{
		case EGSFieldCellState::Burning: return TEXT("burning");
		case EGSFieldCellState::Burnt:   return TEXT("ash");
		case EGSFieldCellState::Doused:  return TEXT("doused");
		default:                         return TEXT("dry crop");
		}
	}

	/**
	 * Prefer an objective the player is actually standing inside, then fall back to nearest.
	 *
	 * Without the containment preference, walking between objectives makes GS.Burn.Ignite silently
	 * retarget - the playtest log shows it hitting the market and then the mill when the field was
	 * what was wanted.
	 */
	static AGSBurnObjectiveBase* FindTarget(UWorld* World, const FVector& Location)
	{
		if (!World)
		{
			return nullptr;
		}

		AGSBurnObjectiveBase* Nearest = nullptr;
		float BestDistSq = TNumericLimits<float>::Max();

		for (TActorIterator<AGSBurnObjectiveBase> It(World); It; ++It)
		{
			AGSBurnObjectiveBase* Objective = *It;
			if (!Objective)
			{
				continue;
			}

			if (Objective->ContainsWorldLocation(Location))
			{
				return Objective; // standing in it - unambiguous
			}

			const float DistSq = FVector::DistSquared(Objective->GetActorLocation(), Location);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Nearest = Objective;
			}
		}

		return Nearest;
	}

	/**
	 * Ignites an objective through whichever door it accepts, and returns a description of what
	 * ACTUALLY changed. Never claims success it didn't achieve.
	 */
	static FString ForceIgniteVerbose(AGSBurnObjectiveBase* Objective, const FVector& PreferredLocation)
	{
		if (!Objective)
		{
			return TEXT("no objective");
		}

		const FString Name = Objective->GetName();

		// ---------------------------------------------------------------- mill
		if (AGSMillObjective* Mill = Cast<AGSMillObjective>(Objective))
		{
			const EGSMillStage Before = Mill->GetStage();
			Mill->IgniteInterior();
			const EGSMillStage After = Mill->GetStage();

			if (Before == After)
			{
				return FString::Printf(
					TEXT("%s UNCHANGED - already past Intact (interior ignition is idempotent)."), *Name);
			}
			return FString::Printf(TEXT("%s interior lit - fuse running (%.1fs to detonation)."),
				*Name, Mill->GetSecondsToDetonation());
		}

		const bool bInside = Objective->ContainsWorldLocation(PreferredLocation);
		const FVector At = bInside ? PreferredLocation : Objective->GetActorLocation();
		const TCHAR* Where = bInside ? TEXT("your position") : TEXT("its centre (you're outside it)");

		// ---------------------------------------------------------------- field
		if (AGSFieldFireObjective* Field = Cast<AGSFieldFireObjective>(Objective))
		{
			if (Field->IsComplete())
			{
				return FString::Printf(
					TEXT("%s REFUSED - objective already complete, it will not relight."), *Name);
			}

			const int32 Idx = Field->GetNearestCellIndex(At);
			const EGSFieldCellState Before = Field->GetCellState(Idx);

			Field->IgniteAtLocation(At);

			const EGSFieldCellState After = Field->GetCellState(Idx);

			if (After == EGSFieldCellState::Burning && Before != EGSFieldCellState::Burning)
			{
				return FString::Printf(TEXT("%s cell %d lit at %s (was %s). burning:%d doused:%d"),
					*Name, Idx, Where, CellStateName(Before),
					Field->GetBurningCellCount(), Field->GetDousedCellCount());
			}

			// Nothing changed - say exactly why, and what to do instead.
			if (Before == EGSFieldCellState::Burnt)
			{
				return FString::Printf(
					TEXT("%s NO-OP - cell %d at %s is ash, and ash never relights. ")
					TEXT("Stand on green or blue crop, or run GS.Burn.Dry."), *Name, Idx, Where);
			}
			if (Before == EGSFieldCellState::Burning)
			{
				return FString::Printf(TEXT("%s NO-OP - cell %d is already burning."), *Name, Idx);
			}
			return FString::Printf(TEXT("%s NO-OP - cell %d state %s, ignition refused."),
				*Name, Idx, CellStateName(Before));
		}

		// ---------------------------------------------------------------- market
		if (AGSMarketObjective* Market = Cast<AGSMarketObjective>(Objective))
		{
			if (Market->GetStallCount() == 0)
			{
				return FString::Printf(
					TEXT("%s REFUSED - zero stalls adopted, this objective cannot be completed."), *Name);
			}

			const int32 Before = Market->GetBurningStallCount();
			Market->IgniteAtLocation(At);
			const int32 After = Market->GetBurningStallCount();

			if (After > Before)
			{
				return FString::Printf(TEXT("%s lit a stall at %s (%d/%d now burning, %d burnt)."),
					*Name, Where, After, Market->GetStallCount(), Market->GetBurntStallCount());
			}
			return FString::Printf(
				TEXT("%s NO-OP - nearest stall already burning or burnt (%d/%d burning, %d burnt)."),
				*Name, After, Market->GetStallCount(), Market->GetBurntStallCount());
		}

		Objective->IgniteAtLocation(At);
		return FString::Printf(TEXT("%s ignited at %s."), *Name, Where);
	}
}

// ---------------------------------------------------------------------------- GS.Burn.Status

static FAutoConsoleCommandWithWorld GSBurnStatusCmd(
	TEXT("GS.Burn.Status"),
	TEXT("Log completion and state breakdown for every burn objective in the level."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		if (!World)
		{
			return;
		}

		int32 Count = 0;
		for (TActorIterator<AGSBurnObjectiveBase> It(World); It; ++It)
		{
			AGSBurnObjectiveBase* Objective = *It;
			if (!Objective)
			{
				continue;
			}

			++Count;

			FString Extra;
			if (AGSFieldFireObjective* Field = Cast<AGSFieldFireObjective>(Objective))
			{
				Extra = FString::Printf(TEXT("  [%d cells: %d burning, %d doused]"),
					Field->GetCellCount(), Field->GetBurningCellCount(), Field->GetDousedCellCount());
			}
			else if (AGSMarketObjective* Market = Cast<AGSMarketObjective>(Objective))
			{
				Extra = FString::Printf(TEXT("  [%d stalls: %d burning, %d burnt]"),
					Market->GetStallCount(), Market->GetBurningStallCount(), Market->GetBurntStallCount());
			}
			else if (AGSMillObjective* Mill = Cast<AGSMillObjective>(Objective))
			{
				Extra = FString::Printf(TEXT("  [stage %d, %.1fs to detonation]"),
					static_cast<int32>(Mill->GetStage()), Mill->GetSecondsToDetonation());
			}

			GSBurnDebug::Log(FString::Printf(TEXT("%s : %.0f%%%s%s"),
				*Objective->GetName(),
				Objective->GetCompletion01() * 100.f,
				Objective->IsComplete() ? TEXT("  [COMPLETE]") : TEXT(""),
				*Extra));
		}

		if (Count == 0)
		{
			GSBurnDebug::Log(TEXT("No burn objectives in this level."));
		}

		// Characters and their HP - the actual proof that fire damage lands.
		for (TActorIterator<AGSCharacterBase> It(World); It; ++It)
		{
			AGSCharacterBase* Character = *It;
			if (!Character)
			{
				continue;
			}

			GSBurnDebug::Log(FString::Printf(TEXT("  %s : %.1f/%.1f HP%s"),
				*Character->GetName(), Character->GetHealth(), Character->GetMaxHealth(),
				Character->IsAlive() ? TEXT("") : TEXT("  [DEAD]")));
		}
	}));

// ---------------------------------------------------------------------------- GS.Burn.Ignite

static FAutoConsoleCommandWithWorld GSBurnIgniteCmd(
	TEXT("GS.Burn.Ignite"),
	TEXT("Ignite the burn objective you are standing in, else the nearest one."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		FVector PlayerLocation;
		if (!GSBurnDebug::GetPlayerLocation(World, PlayerLocation))
		{
			GSBurnDebug::Log(TEXT("No player pawn - are you in PIE?"));
			return;
		}

		AGSBurnObjectiveBase* Target = GSBurnDebug::FindTarget(World, PlayerLocation);
		if (!Target)
		{
			GSBurnDebug::Log(TEXT("No burn objectives in this level."));
			return;
		}

		GSBurnDebug::Log(GSBurnDebug::ForceIgniteVerbose(Target, PlayerLocation));
	}));

// ---------------------------------------------------------------------------- GS.Burn.IgniteAll

static FAutoConsoleCommandWithWorld GSBurnIgniteAllCmd(
	TEXT("GS.Burn.IgniteAll"),
	TEXT("Ignite every burn objective in the level."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		if (!World)
		{
			return;
		}

		for (TActorIterator<AGSBurnObjectiveBase> It(World); It; ++It)
		{
			if (AGSBurnObjectiveBase* Objective = *It)
			{
				GSBurnDebug::Log(GSBurnDebug::ForceIgniteVerbose(Objective, Objective->GetActorLocation()));
			}
		}
	}));

// ---------------------------------------------------------------------------- GS.Burn.Douse

static FAutoConsoleCommandWithWorldAndArgs GSBurnDouseCmd(
	TEXT("GS.Burn.Douse"),
	TEXT("Douse near the player - field cells AND market stalls. Optional radius (default 1200)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World)
	{
		FVector PlayerLocation;
		if (!GSBurnDebug::GetPlayerLocation(World, PlayerLocation))
		{
			GSBurnDebug::Log(TEXT("No player pawn - are you in PIE?"));
			return;
		}

		const float Radius = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 1200.f;

		int32 CellsDoused = 0;
		int32 StallsDoused = 0;
		int32 BurningTotal = 0;
		float NearestBurningDist = TNumericLimits<float>::Max();

		// ---- fields ------------------------------------------------------
		for (TActorIterator<AGSFieldFireObjective> It(World); It; ++It)
		{
			AGSFieldFireObjective* Field = *It;
			if (!Field)
			{
				continue;
			}

			// Measure BEFORE dousing, so a zero result can explain itself.
			for (int32 i = 0; i < Field->GetCellCount(); ++i)
			{
				if (Field->GetCellState(i) == EGSFieldCellState::Burning)
				{
					++BurningTotal;
					NearestBurningDist = FMath::Min(NearestBurningDist,
						FVector::Dist(Field->GetCellWorldLocation(i), PlayerLocation));
				}
			}

			CellsDoused += Field->DouseAtLocation(PlayerLocation, Radius);
		}

		// ---- markets (the case the previous version was blind to) --------
		for (TActorIterator<AGSMarketObjective> It(World); It; ++It)
		{
			AGSMarketObjective* Market = *It;
			if (!Market)
			{
				continue;
			}

			if (Market->GetBurningStallCount() > 0)
			{
				BurningTotal += Market->GetBurningStallCount();
				NearestBurningDist = FMath::Min(NearestBurningDist,
					FVector::Dist(Market->GetActorLocation(), PlayerLocation));
			}

			StallsDoused += Market->DouseAtLocation(PlayerLocation, Radius);
		}

		if (CellsDoused > 0 || StallsDoused > 0)
		{
			FString Msg;
			if (CellsDoused > 0)
			{
				// Firebreaks are temporary by design - see DousedDryOutSeconds. Permanent ones can
				// make the field mathematically impossible to complete.
				Msg += FString::Printf(
					TEXT("%d field cell(s) doused - now firebreaks (blue). Spread can't cross them; ")
					TEXT("a torch can relight them; they dry back to crop on their own. "), CellsDoused);
			}
			if (StallsDoused > 0)
			{
				Msg += FString::Printf(TEXT("%d market stall(s) extinguished."), StallsDoused);
			}
			GSBurnDebug::Log(Msg);
		}
		else if (BurningTotal == 0)
		{
			GSBurnDebug::Log(TEXT("Doused 0 - nothing is burning anywhere (checked fields AND markets). "
								  "Run GS.Burn.Ignite first."));
		}
		else
		{
			GSBurnDebug::Log(FString::Printf(
				TEXT("Doused 0 - %d thing(s) burning but the nearest is %.0fuu away (radius %.0f). ")
				TEXT("Walk closer, or: GS.Burn.Douse %.0f"),
				BurningTotal, NearestBurningDist, Radius, NearestBurningDist + 500.f));
		}
	}));

// ---------------------------------------------------------------------------- GS.Burn.Firebreak

static FAutoConsoleCommandWithWorld GSBurnFirebreakCmd(
	TEXT("GS.Burn.Firebreak"),
	TEXT("Douse a band across the field just ahead of the burn front, to see if firebreaks hold."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		if (!World)
		{
			return;
		}

		int32 Total = 0;
		for (TActorIterator<AGSFieldFireObjective> It(World); It; ++It)
		{
			AGSFieldFireObjective* Field = *It;
			if (!Field || Field->GetBurningCellCount() == 0)
			{
				continue;
			}

			// A BAND around the front, not the whole field. The previous version used radius 99999,
			// which doused every burning cell at once - that converts the entire fire into firebreak
			// and makes re-ignition look broken, which is exactly what it did in testing.
			Total += Field->DouseAtLocation(Field->GetBurningCentroid(), 700.f);
		}

		GSBurnDebug::Log(Total > 0
			? FString::Printf(TEXT("Firebreak: doused %d cell(s) around the front. Watch whether the "
								   "fire routes around the blue band or stalls against it."), Total)
			: FString(TEXT("Firebreak: nothing burning. Run GS.Burn.Ignite first.")));
	}));

// ---------------------------------------------------------------------------- GS.Burn.SpawnDummy

static FAutoConsoleCommandWithWorldAndArgs GSBurnSpawnDummyCmd(
	TEXT("GS.Burn.SpawnDummy"),
	TEXT("Spawn defender dummies standing IN the burn front, to verify fire damage. Optional count."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		const int32 Count = Args.Num() > 0 ? FMath::Clamp(FCString::Atoi(*Args[0]), 1, 10) : 1;

		// Prefer the burn front - a dummy standing outside the fire proves nothing.
		FVector SpawnAt;
		bool bInFire = false;
		for (TActorIterator<AGSFieldFireObjective> It(World); It; ++It)
		{
			AGSFieldFireObjective* Field = *It;
			if (Field && Field->GetBurningCellCount() > 0)
			{
				SpawnAt = Field->GetBurningCentroid();
				bInFire = true;
				break;
			}
		}

		if (!bInFire && !GSBurnDebug::GetPlayerLocation(World, SpawnAt))
		{
			GSBurnDebug::Log(TEXT("Nothing burning and no player pawn - ignite something first."));
			return;
		}

		int32 Spawned = 0;
		for (int32 i = 0; i < Count; ++i)
		{
			// Fan them out slightly so capsules don't fight each other.
			const float Angle = (2.f * PI * i) / FMath::Max(1, Count);
			const FVector Offset(FMath::Cos(Angle) * 90.f * (i > 0 ? 1.f : 0.f),
								 FMath::Sin(Angle) * 90.f * (i > 0 ? 1.f : 0.f), 100.f);

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			if (AGSEnemyCharacter* Dummy = World->SpawnActor<AGSEnemyCharacter>(
					AGSEnemyCharacter::StaticClass(), SpawnAt + Offset, FRotator::ZeroRotator, Params))
			{
#if WITH_EDITOR
				Dummy->SetActorLabel(FString::Printf(TEXT("FireDummy_%d"), i));
#endif
				++Spawned;
			}
		}

		GSBurnDebug::Log(FString::Printf(
			TEXT("Spawned %d dummy defender(s) %s (100 HP, 0 armor). Watch the white HP text over their "
				 "heads with GS.Burn.Debug 1, or run GS.Burn.Status."),
			Spawned, bInFire ? TEXT("IN the burn front") : TEXT("at your position (nothing is burning!)")));
	}));

// ---------------------------------------------------------------------------- GS.Burn.Dry

static FAutoConsoleCommandWithWorld GSBurnDryCmd(
	TEXT("GS.Burn.Dry"),
	TEXT("Instantly dry every doused cell back to crop, without waiting DousedDryOutSeconds."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		if (!World)
		{
			return;
		}

		int32 Total = 0;
		for (TActorIterator<AGSFieldFireObjective> It(World); It; ++It)
		{
			if (AGSFieldFireObjective* Field = *It)
			{
				Total += Field->DryAllDousedCells();
			}
		}

		GSBurnDebug::Log(FString::Printf(
			TEXT("Dried %d doused cell(s) back to crop - they will catch from spread again now."), Total));
	}));
