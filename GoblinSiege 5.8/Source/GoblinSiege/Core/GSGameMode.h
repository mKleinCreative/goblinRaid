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

protected:
	/**
	 * ACF's collisions master (#229). UACMCollisionManagerComponent on each character looks for this
	 * on the GAME MODE and logs `Add Collisions Master o your Game Mode!` (ACF's typo) when it is
	 * absent - which every run did after the Phase 2a reparent gave every character a collision
	 * manager. It drives ACF's swept melee traces; ours still uses its own sweep, so this is here to
	 * satisfy the lookup and to be ready for 2b-2 rather than because anything reads it yet.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|ACF")
	TObjectPtr<class UACMCollisionsMasterComponent> CollisionsMasterComponent;


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

	/**
	 * Spawn OFFSET from a runic site rather than on it.
	 *
	 * The site's own transform is the PORTAL. Spawning on it drops the player inside the extraction
	 * sphere, and then the raid ends itself as a win the instant the portal opens - which is exactly
	 * what happened on the first full playtest: portal-open and "extracted" landed in the same
	 * millisecond, with the player never having moved.
	 *
	 * This is the single choke point for it. Every spawn path - initial spawn, respawn, a Blueprint
	 * calling RestartPlayer - ends up here, so the offset cannot be forgotten by one of them.
	 */
	virtual void RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot) override;

	/**
	 * DEBUG. When set, every spawn AND respawn lands here instead of the runic site.
	 *
	 * Set by GS.Raid.SpawnAt, cleared by GS.Raid.SpawnAt off. It exists because playtesting one
	 * corner of a 3 km hamlet otherwise starts with a two-minute walk from the portal, every single
	 * time - and a test you have to walk to is a test that gets skipped.
	 *
	 * A plain static rather than a UPROPERTY on purpose: it must survive PIE restarts (each PIE gets
	 * a fresh GameMode instance), and it must never be saved into an asset where it could follow
	 * someone into a real playthrough.
	 */
	static TOptional<FVector> DebugSpawnOverride;

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
