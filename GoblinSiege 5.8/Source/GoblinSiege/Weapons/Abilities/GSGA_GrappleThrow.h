// Throws the grappling hook. The fourth wheel slot's verb (2026-08-17).
//
// Shaped on UGSGA_TorchToss deliberately: both are "the attack key, reinterpreted by what is in
// your hand", both spawn a projectile from UGSAimComponent's muzzle after a short wind-up, and
// both must tear down on EVERY exit path rather than only the successful one. Copying that shape
// means the grapple inherits the fixes those two paths already took (#040's montage/spawn coupling,
// #048's assignment failure, the muzzle move to UGSAimComponent in #004).
//
// What it does NOT do is own the rope. AGSGrappleHookProjectile derives its own anchor and lays its
// own rope on impact - so this ability's whole job is "put a hook in the air, once".
//
// Rebased onto UACFGameplayAbility (#390, 2026-08-31), NOT plain UGameplayAbility. GS's own ASC
// (AGSCharacterBase::AbilitySystemComponent) IS ACF's UACFAbilitySystemComponent - see
// gs-abilities-outside-acf-asc - but CurrentPriority/buffering are armed only by
// UACFGameplayAbility::ActivateAbility/::EndAbility calling ACFAbilityComponent->OnAbilityStarted/
// OnAbilityEnded, and a plain UGameplayAbility never does. This ability does NOT call
// Super::ActivateAbility (that pipeline commits an ActionConfig cost against a
// UACFGASStatisticsComponent this character may not have, and would silently no-op the whole throw
// if that component is absent) - it calls OnAbilityStarted directly instead, keeping the existing
// hand-rolled windup/spawn timing. Super::EndAbility IS called (unchanged from before this rebase)
// and that alone reaches UACFGameplayAbility::EndAbility's OnAbilityEnded call, which is what
// arbitration and combo buffering actually key off. ActionConfig.bAutoStartCooldown is turned off in
// the constructor so the unused cooldown half of that same base-class pipeline stays inert too.
#pragma once

#include "CoreMinimal.h"
#include "ACFGameplayAbility.h"
#include "GSGA_GrappleThrow.generated.h"

class UAnimMontage;
class AGSGrappleHookProjectile;

UCLASS()
class GOBLINSIEGE_API UGSGA_GrappleThrow : public UACFGameplayAbility
{
	GENERATED_BODY()

public:
	UGSGA_GrappleThrow();

	/** What this ability will actually spawn, so UGSAimComponent's arc can predict the SAME class
	 *  the throw spawns. Exactly UGSGA_TorchToss::GetTorchProjectileClass's reason for existing:
	 *  one place owns "which hook", and it is the ability that throws it. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Grapple")
	TSubclassOf<AGSGrappleHookProjectile> GetHookProjectileClass() const { return HookProjectileClass; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Spawns the hook. Split out so the wind-up delay has something to call. */
	void ThrowHook();

	/** Wind-up elapsed. Bound to UAbilityTask_WaitDelay's OnFinish, which is a dynamic delegate and
	 *  therefore needs the UFUNCTION. */
	UFUNCTION()
	void OnGrappleWindupFinished();

	/**
	 * C++-defaulted to AGSGrappleHookProjectile (#390) - the BP_GrappleHook prototype this used to
	 * soft-load is retired. Same reasoning as AGSTorchProjectile::FireVolumeClass: a reference
	 * assigned only in a content folder is one bad merge or rename away from being null again, and
	 * that failure is silent - the wheel's newest slot would throw nothing and say nothing, which is
	 * #048 and #088 both. Still EditDefaultsOnly so a thin Blueprint child (for a hook mesh/rope
	 * material override) can be substituted without touching code.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Grapple")
	TSubclassOf<AGSGrappleHookProjectile> HookProjectileClass;

	/** Wind-up before the hook leaves the hand, matching the torch's 0.25s. Short enough to still
	 *  read as a flick. A world timer would not be cancelled with the ability; the task is. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Grapple")
	float GrappleWindupSeconds = 0.25f;

	/**
	 * Throw animation. Shares AM_GS_ThrowTorch until AM_GS_GrappleThrow is built from
	 * Downloads\Throw Object.fbx - so the grapple throw and the torch throw look identical for now,
	 * and the ticket says so rather than letting it read as a bug.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Grapple")
	TSoftObjectPtr<UAnimMontage> ThrowMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ResolvedThrowMontage;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Grapple", meta = (ClampMin = "0.1"))
	float ThrowMontagePlayRate = 1.5f;
};
