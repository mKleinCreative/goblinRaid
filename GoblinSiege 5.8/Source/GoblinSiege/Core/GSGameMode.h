// Server-only raid orchestration: the death → lives → respawn flow (design doc §3 - "lives, not
// permadeath"; respawn frictionless with partial HP + brief i-frames). Reconstructed 2026-07-19
// to match AGSCharacterBase::HandleDeath's call signature.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GSGameMode.generated.h"

class AGSCharacterBase;

UCLASS()
class GOBLINSIEGE_API AGSGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGSGameMode();

	/** Called by AGSCharacterBase::HandleDeath (server). AI defenders just clean up; goblins with
	 *  a PlayerState spend a life and respawn on a short timer. */
	void HandleGoblinDeath(AGSCharacterBase* DeadCharacter, AController* Controller);

	/**
	 * Respawn at the runic site (design doc §1 - "respawn is at the runic site"), falling back to
	 * stock PlayerStart selection when a map has no site.
	 *
	 * This override is the whole implementation of that rule: RespawnPlayer already called
	 * RestartPlayer, which already routed through here - it just had nothing to say, so every
	 * respawn landed on whichever PlayerStart the engine happened to pick.
	 */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

protected:
	void RespawnPlayer(AController* Controller);

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Respawn|Tuning")
	float RespawnDelaySeconds = 4.f;

	/** "partial HP" on respawn (race-design-goblins.md keeps downtime under ~15% of a raid). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Respawn|Tuning")
	float RespawnHealthFraction = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Respawn|Tuning")
	float RespawnInvulnerabilitySeconds = 2.f;
};
