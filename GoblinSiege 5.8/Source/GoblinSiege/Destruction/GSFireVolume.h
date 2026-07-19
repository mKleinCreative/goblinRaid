// Ground fire spawned by torch impacts: radial DoT with uniform friendly fire (design doc §4 -
// "fire doesn't check factions") plus delayed spread to nearby flammables. Reconstructed
// 2026-07-19 to match the surviving GSFireVolume.cpp exactly.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSFireVolume.generated.h"

class USphereComponent;
class UNiagaraComponent;

UCLASS()
class GOBLINSIEGE_API AGSFireVolume : public AActor
{
	GENERATED_BODY()

public:
	AGSFireVolume();

protected:
	virtual void BeginPlay() override;

	void DamageTick();
	void SpreadTick();

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Fire")
	TObjectPtr<USphereComponent> DamageSphere;

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Fire")
	TObjectPtr<UNiagaraComponent> FireFX;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Tuning")
	float DamageRadius = 140.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Tuning")
	float DamagePerTick = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Tuning")
	float DamageTickInterval = 0.5f;

	/** Only matters when the burn victim is another goblin/ally; enemies always take full damage
	 *  (design doc §4 friendly-fire knob). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Tuning")
	float FriendlyFireScalar = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Tuning")
	float SpreadDelaySeconds = 2.5f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Tuning")
	float SpreadRadius = 220.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Tuning")
	float LifetimeSeconds = 10.f;

	FTimerHandle DamageTickHandle;
	FTimerHandle SpreadTimerHandle;
};
