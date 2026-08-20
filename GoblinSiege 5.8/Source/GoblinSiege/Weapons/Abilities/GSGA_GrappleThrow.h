// Throws the grappling hook. The fourth wheel slot's verb (2026-08-17).
//
// Shaped on UGSGA_TorchToss deliberately: both are "the attack key, reinterpreted by what is in
// your hand", both spawn a projectile from UGSAimComponent's muzzle after a short wind-up, and
// both must tear down on EVERY exit path rather than only the successful one. Copying that shape
// means the grapple inherits the fixes those two paths already took (#040's montage/spawn coupling,
// #048's assignment failure, the muzzle move to UGSAimComponent in #004).
//
// What it does NOT do is own the rope. The hook actor derives its own anchor and lays its own rope
// on impact - see BP_GrappleHook - so this ability's whole job is "put a hook in the air, once".
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GSGA_GrappleThrow.generated.h"

class UAnimMontage;

UCLASS()
class GOBLINSIEGE_API UGSGA_GrappleThrow : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGSGA_GrappleThrow();

	/** What this ability will actually spawn, so UGSAimComponent's arc can predict the SAME class
	 *  the throw spawns. Exactly UGSGA_TorchToss::GetTorchProjectileClass's reason for existing:
	 *  one place owns "which hook", and it is the ability that throws it. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Grapple")
	TSubclassOf<AActor> GetHookProjectileClass() const { return HookProjectileClass; }

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
	 * Soft-defaulted to /Game/Blueprints/Grapple/BP_GrappleHook.
	 *
	 * A TSubclassOf<AActor> rather than a concrete C++ projectile type because the prototype hook
	 * IS a Blueprint - there is no AGSGrappleHookProjectile yet. When that class lands this should
	 * be re-typed to it; until then a null here would make the wheel's newest slot do nothing at
	 * all, silently, which is #048 and #088 both.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Grapple")
	TSoftClassPtr<AActor> HookProjectileClassPath;

	/** Resolved HookProjectileClassPath, cached for the life of the instance. Transient: rebuilt on
	 *  demand, never saved. */
	UPROPERTY(Transient)
	TSubclassOf<AActor> HookProjectileClass;

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
