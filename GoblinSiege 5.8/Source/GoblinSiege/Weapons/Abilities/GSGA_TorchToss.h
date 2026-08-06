// The universal racial verb: every goblin can throw torches regardless of kit (design doc §5,
// race-design-goblins.md "fire is cheap, plentiful, and the great equalizer"). Granted at the
// character level, not the weapon level. Reconstructed 2026-07-19.
//
// 2026-08-01 - THE THROW NOW HAS A TORCH IN IT, in both senses. TorchProjectileClass was null, so
// this ability committed its cost, computed a spawn transform, and spawned nothing at all; it now
// C++-defaults to AGSTorchProjectile (which already ignites flammables and burn objectives
// correctly - no new projectile class was invented for this). And the goblin now visibly holds a
// torch for the wind-up before it leaves his hand: the held prop is owned by UGSWeaponComponent
// (see UGSWeaponDataAsset::HeldTorchMesh for why it lives there and not here), and this ability
// only tells it when to appear and when to vanish.
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

	/** What this ability will actually spawn. Exists so the aim arc can predict the SAME class the
	 *  throw spawns instead of the character keeping a second copy of the reference - one place owns
	 *  "which torch", and it is the ability that throws it. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Torch")
	TSubclassOf<AGSTorchProjectile> GetTorchProjectileClass() const { return TorchProjectileClass; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** Un-readies the held torch on EVERY exit path, not just the successful throw. An ability can
	 *  be cancelled, interrupted by a death, or blocked by a tag mid-wind-up, and a goblin left
	 *  holding a phantom torch for the rest of the raid is the failure mode this prevents. */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Spawns the projectile. Split out of ActivateAbility so the wind-up delay has something to
	 *  call - the spawn maths is unchanged from the 2026-07-19 version. */
	void ThrowTorch();

	/** Wind-up elapsed: throw, drop the held torch, end. Bound to UAbilityTask_WaitDelay's OnFinish,
	 *  which is a dynamic delegate and therefore needs the UFUNCTION. */
	UFUNCTION()
	void OnTorchWindupFinished();

	/**
	 * C++-defaulted to AGSTorchProjectile (2026-08-01) for the same reason
	 * AGSTorchProjectile::FireVolumeClass is C++-defaulted to AGSFireVolume and AGSFireVolume's
	 * FireDamageEffectClass to UGSGE_FireDamage: a C++ default cannot go missing from a content
	 * folder, and a null here made the game's most-used verb do nothing, silently.
	 *
	 * Assign a Blueprint subclass over the top when a torch needs to differ; do not clear it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Torch")
	TSubclassOf<AGSTorchProjectile> TorchProjectileClass;

	// SpawnForwardOffset was removed on 2026-08-04. The muzzle - forward offset and hand height
	// both - now lives on UGSAimComponent, which is the single thing that defines where a ranged
	// verb comes from, so the arc preview and the spawn cannot describe different points. Retune it
	// there (MuzzleForwardOffset / MuzzleHeightOffset), where the torch and the bow share it.

	/**
	 * How long the torch is visibly HELD before it leaves the hand.
	 *
	 * Without this the ability would ready and throw within one frame and the held torch would
	 * never be seen - "appears when readied, disappears when thrown" needs the two to be separate
	 * moments. 0.25s is a wind-up, not a cast time: short enough to still read as a flick, long
	 * enough that the prop registers. Set it to 0 to spawn immediately (and never show the held
	 * torch), which is the pre-2026-08-01 behaviour exactly.
	 *
	 * Implemented with UAbilityTask_WaitDelay rather than a world timer, matching UGSGA_DodgeRoll -
	 * the task is cancelled with the ability, which a raw timer would not be, and it is the hook a
	 * throw montage's AnimNotify should eventually replace.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Torch")
	float TorchWindupSeconds = 0.25f;

	/** Cooldown/cost are editor-authored GameplayEffect assets assigned on the CDO
	 *  (CooldownGameplayEffectClass / CostGameplayEffectClass) - data, not code. */
};
