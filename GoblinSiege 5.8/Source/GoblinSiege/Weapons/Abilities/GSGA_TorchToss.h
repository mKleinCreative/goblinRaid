// The universal racial verb: every goblin can throw torches regardless of kit (design doc §5,
// race-design-goblins.md "fire is cheap, plentiful, and the great equalizer"). Granted at the
// character level, not the weapon level. Reconstructed 2026-07-19.
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GSGA_TorchToss.generated.h"

class AGSTorchProjectile;

UCLASS()
class GOBLINSIEGE_API UGSGA_TorchToss : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGSGA_TorchToss();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Torch")
	TSubclassOf<AGSTorchProjectile> TorchProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Torch")
	float SpawnForwardOffset = 80.f;

	/** Cooldown/cost are editor-authored GameplayEffect assets assigned on the CDO
	 *  (CooldownGameplayEffectClass / CostGameplayEffectClass) - data, not code. */
};
