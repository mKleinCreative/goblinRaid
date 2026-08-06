// The bow shot. Added 2026-08-04 alongside UGSAimComponent, and deliberately built to the same
// shape as UGSGA_TorchToss - hold to aim, brief wind-up, spawn along the aim, end - because the two
// verbs ARE the same verb with different numbers, and the aim framework exists to make that
// literally true rather than merely true in spirit.
//
// The spawn transform comes from UGSAimComponent::GetMuzzleTransform(), which is also what drew the
// arc the player just aimed with. That shared call is the entire point of the framework: the arrow
// leaves from the place the preview said it would, in the direction the preview said it would, and
// there is no second copy of those constants to drift.
//
// Not granted by the character's kit unconditionally the way the torch is - the torch is a racial
// verb every goblin has, the bow is the Scout's ranged half, and the character only routes the
// attack button here while UGSWeaponComponent::IsInRangedMode() is true.
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GSGA_BowShot.generated.h"

class AGSArrowProjectile;

UCLASS()
class GOBLINSIEGE_API UGSGA_BowShot : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGSGA_BowShot();

	/** What this ability will actually spawn, so the aim arc predicts the same class the shot fires.
	 *  Same reasoning as UGSGA_TorchToss::GetTorchProjectileClass. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Bow")
	TSubclassOf<AGSArrowProjectile> GetArrowProjectileClass() const { return ArrowProjectileClass; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** Spawns the arrow at the aim component's muzzle. Split out of ActivateAbility so the release
	 *  delay has something to call, exactly as UGSGA_TorchToss::ThrowTorch is. */
	void FireArrow();

	/** Release delay elapsed: fire and end. Bound to UAbilityTask_WaitDelay's OnFinish, which is a
	 *  dynamic delegate and therefore needs the UFUNCTION. */
	UFUNCTION()
	void OnReleaseFinished();

	/**
	 * C++-defaulted to AGSArrowProjectile for the reason argued at length in UGSGA_TorchToss.h: a
	 * C++ default cannot go missing from a content folder, and a null projectile class is the exact
	 * failure that once made the torch toss commit its cost and spawn nothing at all.
	 *
	 * Assign a Blueprint subclass over the top when an arrow needs to differ; do not clear it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Bow")
	TSubclassOf<AGSArrowProjectile> ArrowProjectileClass;

	/**
	 * Seconds between the release and the arrow leaving the string.
	 *
	 * Shorter than the torch's 0.25s wind-up because the two moments are not the same thing: the
	 * torch's delay exists so the held prop can be SEEN before it goes, whereas the bow was already
	 * drawn for the whole time the player held aim. This is release recoil, not a cast time. Set it
	 * to 0 for an instant loose, and let a release montage's AnimNotify replace it when one exists.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Bow", meta = (ClampMin = "0.0"))
	float ReleaseDelaySeconds = 0.08f;

	/** Cooldown/cost are editor-authored GameplayEffect assets assigned on the CDO
	 *  (CooldownGameplayEffectClass / CostGameplayEffectClass) - data, not code. Arrow supply, when
	 *  it exists, belongs there and not in this class. */
};
