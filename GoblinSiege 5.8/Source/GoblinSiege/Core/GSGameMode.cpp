#include "Core/GSGameMode.h"
#include "Core/GSGameState.h"
#include "Core/GSPlayerState.h"
#include "Characters/GSCharacterBase.h"
#include "Raid/GSRaidDirector.h"
#include "Raid/GSRunicSite.h"
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
	// Initial spawn only - respawns go through RespawnPlayer's RestartPlayerAtTransform, which
	// needs the site's forward offset and so cannot express itself as "an actor to stand on".
	if (AGSRunicSite* Site = FindRunicSite(GetWorld()))
	{
		return Site;
	}

	// No site: stock selection. A test map or a combat arena is allowed not to have one.
	return Super::ChoosePlayerStart_Implementation(Player);
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

	// Respawn at the runic site (design doc §1). RestartPlayerAtTransform rather than
	// RestartPlayer + ChoosePlayerStart, because the site's spawn point is OFFSET from the site's
	// own transform - it has to land outside the extraction sphere, or respawning while the portal
	// stands open would instantly extract the player and end the raid as a win they never chose.
	// An actor return value cannot carry that offset; a transform can.
	if (const AGSRunicSite* Site = FindRunicSite(GetWorld()))
	{
		RestartPlayerAtTransform(Controller, Site->GetSpawnTransform());
	}
	else
	{
		RestartPlayer(Controller);
	}

	if (AGSCharacterBase* NewCharacter = Controller->GetPawn<AGSCharacterBase>())
	{
		NewCharacter->ApplyRespawnState(RespawnHealthFraction, RespawnInvulnerabilitySeconds);
	}
}
