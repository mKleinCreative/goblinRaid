#include "Core/GSGameMode.h"
#include "Core/GSGameState.h"
#include "Core/GSPlayerState.h"
#include "Characters/GSCharacterBase.h"
#include "TimerManager.h"

AGSGameMode::AGSGameMode()
{
	GameStateClass = AGSGameState::StaticClass();
	PlayerStateClass = AGSPlayerState::StaticClass();
	// DefaultPawnClass and HUDClass are assigned in the BP_GSGameMode subclass so designers can
	// swap them without a recompile (project rule: tuning lives in data/Blueprint).
}

void AGSGameMode::HandleGoblinDeath(AGSCharacterBase* DeadCharacter, AController* Controller)
{
	if (!Controller)
	{
		// Uncontrolled pawn (shouldn't happen) - just let the corpse time out.
		if (DeadCharacter)
		{
			DeadCharacter->SetLifeSpan(5.f);
		}
		return;
	}

	AGSPlayerState* PS = Controller->GetPlayerState<AGSPlayerState>();
	if (!PS)
	{
		// AI defender: no lives concept - clean up the corpse after a beat for gibs/ragdoll.
		if (DeadCharacter)
		{
			DeadCharacter->SetLifeSpan(5.f);
		}
		return;
	}

	const int32 LivesLeft = PS->LoseLife();
	if (LivesLeft > 0)
	{
		FTimerHandle UnusedHandle;
		GetWorldTimerManager().SetTimer(UnusedHandle,
			FTimerDelegate::CreateUObject(this, &AGSGameMode::RespawnPlayer, Controller),
			RespawnDelaySeconds, false);
	}
	// else: out of lives - raid over for this goblin. Game-over/score-screen flow is driven by
	// the HUD listening to the PlayerState's OnLivesChanged hitting 0 (tech doc §11).
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
		OldPawn->Destroy();
	}

	RestartPlayer(Controller);

	if (AGSCharacterBase* NewCharacter = Controller->GetPawn<AGSCharacterBase>())
	{
		NewCharacter->ApplyRespawnState(RespawnHealthFraction, RespawnInvulnerabilitySeconds);
	}
}
