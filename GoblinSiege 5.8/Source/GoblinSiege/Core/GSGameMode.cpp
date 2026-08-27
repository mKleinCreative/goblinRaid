#include "Core/GSGameMode.h"
#include "ACMCollisionsMasterComponent.h"
#include "Core/GSGameState.h"
#include "Core/GSPlayerState.h"
#include "Characters/GSCharacterBase.h"
#include "Raid/GSRaidDirector.h"
#include "Raid/GSRunicSite.h"
#include "Raid/GSWarren.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

namespace
{
	/** The map's runic site, or null. A level has one; iteration is fine for a once-per-death
	 *  lookup and avoids a cached pointer that has to survive PIE restarts. */
	AGSRunicSite* FindRunicSite(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		for (TActorIterator<AGSRunicSite> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}
}

AGSGameMode::AGSGameMode()
{
	CollisionsMasterComponent = CreateDefaultSubobject<UACMCollisionsMasterComponent>(TEXT("ACF Collisions Master"));

	GameStateClass = AGSGameState::StaticClass();
	PlayerStateClass = AGSPlayerState::StaticClass();
	// DefaultPawnClass and HUDClass are assigned in the BP_GSGameMode subclass so designers can
	// swap them without a recompile (project rule: tuning lives in data/Blueprint).
}

void AGSGameMode::HandleGoblinDeath(AGSCharacterBase* DeadCharacter, AController* Controller)
{
	// Corpse lifetime is NOT decided here. AGSCharacterBase::HandleDeath already applies its own
	// CorpseLifespan, which defaults to 0 meaning "never" - "correct for a playtest, you want to see
	// what you killed". This function used to hard-code SetLifeSpan(5.f) on both paths below, which
	// silently overrode that and swept every body off the map five seconds after it fell (2026-08-05:
	// Michael wants the bodies to stay). Change CorpseLifespan on the character to reinstate a limit.

	// Broadcast FIRST, above both early returns below - and the order is the whole point.
	//
	//   * A defender killed after its controller has already been destroyed arrives here with
	//     Controller == nullptr and returns on the next line.
	//   * Every AI defender has no AGSPlayerState and returns a few lines after that.
	//
	// So a broadcast placed after either guard reports PLAYER deaths only, which is the exact
	// opposite of what a "how much of the garrison is dead" signal needs. It has taken until now for
	// anything to want this, which is why the reporting gap survived so long.
	//
	// Single-fire is inherited, not re-implemented: AGSCharacterBase::HandleDeath guards on bIsDead
	// before it ever calls in here, so this cannot double-count a corpse.
	if (DeadCharacter)
	{
		OnCharacterKilled.Broadcast(
			DeadCharacter, DeadCharacter->GetRaceTag(), DeadCharacter->GetActorLocation());
	}

	if (!Controller)
	{
		return;
	}

	AGSPlayerState* PS = Controller->GetPlayerState<AGSPlayerState>();
	if (!PS)
	{
		// AI defender: no lives concept, and nothing further to do.
		return;
	}

	const int32 LivesLeft = PS->LoseLife();
	if (LivesLeft > 0)
	{
		FTimerHandle UnusedHandle;
		GetWorldTimerManager().SetTimer(UnusedHandle,
			FTimerDelegate::CreateUObject(this, &AGSGameMode::RespawnPlayer, Controller),
			RespawnDelaySeconds, false);
		return;
	}

	// Out of lives (2026-08-05). This was previously a comment saying the flow "is driven by the
	// HUD listening to OnLivesChanged hitting 0" - but nothing listened, so spending the fifth life
	// left the player a corpse on the ground with the raid still running and no way to act. Ending
	// the raid is a rules decision, so it belongs on the server here rather than in a widget: a HUD
	// that owns a lose condition cannot lose the raid on a dedicated server, where it does not run.
	//
	// The final corpse is deliberately left standing - see RespawnPlayer, which already refuses to
	// destroy a dead pawn. Timing it out here would delete the player's body out from under the
	// lose screen.
	if (UGSRaidDirector* Director = UGSRaidDirector::Get(this))
	{
		// EndRaid is first-call-wins, so a goblin who spends their last life while already
		// standing in an open portal still reads as Extracted.
		Director->EndRaid(EGSRaidResult::OutOfLives);
	}
}

AActor* AGSGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// A RESPAWN comes back through the Warren (GDD 9); the FIRST spawn of the raid never does.
	// GDD 2 opens on materializing at the runic site, and arriving by portal but coming back up
	// through the hole is the whole picture. See bRespawning for why this is a flag.
	if (bRespawning)
	{
		const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
		const FVector From = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
		if (AGSWarren* Warren = AGSWarren::FindNearestRespawnPoint(this, From))
		{
			return Warren;
		}
	}

	if (AGSRunicSite* Site = FindRunicSite(GetWorld()))
	{
		return Site;
	}

	// No site: stock selection. A test map or a combat arena is allowed not to have one.
	return Super::ChoosePlayerStart_Implementation(Player);
}

TOptional<FVector> AGSGameMode::DebugSpawnOverride;

void AGSGameMode::RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot)
{
	// Debug override wins over everything, including the runic site - that is the point of it.
	if (DebugSpawnOverride.IsSet())
	{
		const FVector Where = DebugSpawnOverride.GetValue();
		UE_LOG(LogTemp, Warning, TEXT("[GoblinSiege] Spawn overridden by GS.Raid.SpawnAt -> (%.0f, %.0f, %.0f)"),
			Where.X, Where.Y, Where.Z);
		RestartPlayerAtTransform(NewPlayer, FTransform(FRotator::ZeroRotator, Where));
		return;
	}

	// See the header: the site's transform is the portal, so standing on it is standing in the
	// extraction circle. GetSpawnTransform() is the offset one, clamped outside the sphere.
	// The Warren has no extraction sphere to stand clear of - banking consumes cargo rather than
	// ending the raid - so its spawn transform is the mouth itself, ground-traced.
	if (const AGSWarren* Warren = Cast<AGSWarren>(StartSpot))
	{
		RestartPlayerAtTransform(NewPlayer, Warren->GetSpawnTransform());
		return;
	}

	if (const AGSRunicSite* Site = Cast<AGSRunicSite>(StartSpot))
	{
		RestartPlayerAtTransform(NewPlayer, Site->GetSpawnTransform());
		return;
	}

	Super::RestartPlayerAtPlayerStart(NewPlayer, StartSpot);
}

void AGSGameMode::RespawnPlayer(AController* Controller)
{
	if (!Controller)
	{
		return;
	}

	if (APawn* OldPawn = Controller->GetPawn())
	{
		Controller->UnPossess();

		// A dead pawn is left standing (lying, rather) as a corpse - destroying it here is what made
		// the player's own body vanish the instant they respawned. Anything NOT dead is still torn
		// down, because that is a forced respawn and leaving a live duplicate would be worse.
		const AGSCharacterBase* AsCharacter = Cast<AGSCharacterBase>(OldPawn);
		if (!AsCharacter || AsCharacter->IsAlive())
		{
			OldPawn->Destroy();
		}
	}

	// Respawn at the runic site (design doc §1). Plain RestartPlayer: it routes through
	// ChoosePlayerStart -> RestartPlayerAtPlayerStart, and THAT is where the site's spawn offset is
	// applied, for every spawn path at once. Duplicating the offset here as well was how the
	// initial spawn ended up missing it.
	// Scoped so an exception or an early return inside RestartPlayer cannot leave the flag raised -
	// a stuck bRespawning would send the NEXT raid's opening spawn to the Warren.
	{
		TGuardValue<bool> RespawnScope(bRespawning, true);
		RestartPlayer(Controller);
	}

	if (AGSCharacterBase* NewCharacter = Controller->GetPawn<AGSCharacterBase>())
	{
		NewCharacter->ApplyRespawnState(RespawnHealthFraction, RespawnInvulnerabilitySeconds);
	}
}
