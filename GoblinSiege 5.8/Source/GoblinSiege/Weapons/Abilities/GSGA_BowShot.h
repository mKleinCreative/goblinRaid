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

public:
	/**
	 * Which item a shot from THIS avatar's bow spends, or null if its shots are free (ruling 46).
	 *
	 * STATIC AND PUBLIC because three callers need the same answer and a second copy of "which item
	 * is ammo" would drift: the ability itself (refusal + consume), AGSPlayerCharacter (whether to
	 * start the draw at all), and UGSPlayerHUDWidget (what number to show). One rule, one place.
	 *
	 * Non-null only when BOTH: the avatar has a UGSBowTimingComponent, and its equipped
	 * UGSWeaponDataAsset names an ArrowItemClass. See GetAmmoItemClass for the full reasoning on why
	 * that pair is what keeps quivers player-only and why both halves fail OPEN.
	 */
	static TSubclassOf<class UACFItem> GetAmmoItemClassFor(const AActor* Avatar);

	/**
	 * True if this avatar could loose an arrow right now as far as AMMO is concerned.
	 *
	 * A bow that spends nothing always returns true - free shots are never blocked. Says nothing
	 * about the fire interval, which stays enforced inside the ability where it always was.
	 */
	static bool HasAmmoFor(const AActor* Avatar);

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

	/**
	 * The item class this shot must SPEND, or null if this shot is free (ruling 46, #280).
	 *
	 * ONE HELPER, TWO CALL SITES - the refusal in CanActivateAbility and the consume in FireArrow -
	 * so "who pays for arrows" is answered in exactly one place and cannot drift between them.
	 *
	 * Non-null only when BOTH hold:
	 *   1. the avatar has a UGSBowTimingComponent, and
	 *   2. the equipped UGSWeaponDataAsset has a non-null ArrowItemClass.
	 *
	 * THIS IS WHAT KEEPS THE QUIVER PLAYER-ONLY. This ability is shared: BP_ErikaArcher's
	 * RangedAttackAbilityClass and the player's BowShotAbilityClass are both this class with no
	 * Blueprint child between them, so a naive count check would leave an AI archer in a permanent
	 * draw the moment she ran out. Test 1 is the discriminator this file already uses twice (see
	 * FireArrow's "THE ONE LINE" comment) - an AI archer simply has nothing to ask. It is
	 * deliberately NOT IsPlayerControlled(), which flips the first time anyone possesses an archer
	 * for a debug session and silently stops her shooting.
	 *
	 * IT FAILS OPEN. Both tests default to "free": a missing component or an unset ArrowItemClass
	 * costs infinite arrows, never a dead bow.
	 */
	TSubclassOf<class UACFItem> GetAmmoItemClass(const FGameplayAbilityActorInfo* ActorInfo) const;

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
	 * ERIKA'S RECOIL, and only Erika's.
	 *
	 * Played when the avatar has NO UGSBowTimingComponent - i.e. an AI archer. The player's bow
	 * animation is driven by that component instead, because the player's draw is indefinite and
	 * only the component knows when it starts and ends. Asking for the component's absence is the
	 * same structural gate the damage multiplier uses; there is no IsPlayerControlled() here either.
	 *
	 * The human DRAW montage is deliberately not played from here. An AI archer's draw window lives
	 * in UBTTask_RangedAttack::DrawSeconds (0.8s), which is where a draw animation has to start to
	 * line up with it - by the time this ability activates the shot is already leaving. That belongs
	 * in the BT task and is not done yet.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Bow|Animation")
	TSoftObjectPtr<UAnimMontage> AIReleaseMontage;

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
