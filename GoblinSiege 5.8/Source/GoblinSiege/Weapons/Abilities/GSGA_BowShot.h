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

	/** Adds the rate-of-fire gate on top of the usual tag and cost checks. */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** Seconds that must elapse between shots: the equipped weapon's RangedAttackCooldownSeconds,
	 *  falling back to FallbackFireIntervalSeconds when there is no weapon to ask. */
	float GetFireIntervalSeconds(const FGameplayAbilityActorInfo* ActorInfo) const;

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

	/**
	 * Minimum seconds between shots when the avatar has no equipped weapon, or that weapon's
	 * RangedAttackCooldownSeconds is <= 0.
	 *
	 * The number that normally applies is UGSWeaponDataAsset::RangedAttackCooldownSeconds, so the
	 * bow's rate of fire is retuned in DA_Weapon_Scout without a rebuild. That field has carried a
	 * 1.5s default and the comment "ranged trades damage-per-second for range/safety" since the data
	 * asset was written, and had NO reader anywhere in the project - which is exactly why the bow
	 * fired as fast as the attack button could be clicked. This is the reader.
	 *
	 * Deliberately NOT a cooldown GameplayEffect. A GE would be the GAS-idiomatic answer and is the
	 * right move the day the HUD needs to draw a cooldown sweep, because a tag on the ASC is the only
	 * form UI can observe. It would also cost a new UGameplayEffect class and a new Cooldown.* tag in
	 * the shared GSGameplayTags, for a rule nothing currently watches. A timestamp is the pattern
	 * already used by AGSCharacterBase::HitReactCooldownSeconds and UBTTask_MeleeAttack, so this is
	 * consistent rather than novel. Revisit when something needs to READ the remaining time.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Bow", meta = (ClampMin = "0.0"))
	float FallbackFireIntervalSeconds = 1.5f;

	/**
	 * World time of the last activation allowed through, or negative if this avatar has not shot yet.
	 *
	 * A plain member works because the ability is InstancedPerActor - one instance per archer, living
	 * across activations. It would silently do nothing under InstancedPerExecution, so that policy and
	 * this gate cannot both be true; the constructor's choice is load-bearing here.
	 *
	 * The "has not fired" case is tested explicitly rather than by seeding a large negative value,
	 * because world time starts at 0 and any sentinel close enough to be readable would swallow the
	 * first shot of the raid.
	 */
	float LastFireTimeSeconds = -1.f;

	/** Cost is still an editor-authored GameplayEffect assigned on the CDO
	 *  (CostGameplayEffectClass) - data, not code. Arrow supply, when it exists, belongs there. */
};
