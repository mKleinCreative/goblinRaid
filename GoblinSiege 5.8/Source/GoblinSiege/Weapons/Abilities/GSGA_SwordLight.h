// Light melee swing. The first ability in this project that actually generates a hit.
//
// Shape: windup -> damage window -> recovery, each an EditDefaultsOnly float on the CDO so the
// whole feel can be tuned from the details panel while PIE is running. That is the entire reason
// this is three timers rather than an anim-notify-state window: notify timings live inside a
// montage asset, and structural montage edits hard-crash the editor while the montage editor is
// open. Numbers you tune twenty times an hour do not belong there yet. Once the swing FEELS right,
// moving the window onto a notify state is a mechanical change.
//
// Hit detection is a sphere sweep in front of the character, not a socket-pair weapon trace. ACF's
// ACMCollisionManagerComponent sweeps between SOCKET_start/SOCKET_end on the weapon mesh, and our
// weapon static meshes have no sockets - adding them means a Blender round trip on a source FBX
// that does not currently exist on this machine. A generous sphere answers "does the swing read
// and does the reach feel right", which is the question this week.
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GSGA_SwordLight.generated.h"

class UAnimMontage;
class UGameplayEffect;

UCLASS()
class GOBLINSIEGE_API UGSGA_SwordLight : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGSGA_SwordLight();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Optional. If unset the swing still works and still hits - it just isn't animated, which is
	 *  the correct fallback for a timing test. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Swing|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Swing|Animation", meta = (ClampMin = "0.1"))
	float MontagePlayRate = 1.6f;

	/** Delay from button press to the damage window opening. The single most important feel number. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Swing|Timing", meta = (ClampMin = "0.0"))
	float WindupSeconds = 0.18f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Swing|Timing", meta = (ClampMin = "0.02"))
	float DamageWindowSeconds = 0.16f;

	/** Locked-out tail after the window. Short reads twitchy, long reads committed. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Swing|Timing", meta = (ClampMin = "0.0"))
	float RecoverySeconds = 0.22f;

	/** How often the window re-sweeps. 60Hz is plenty and keeps fast swings from tunnelling. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Swing|Timing", meta = (ClampMin = "0.008"))
	float SweepIntervalSeconds = 0.0167f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Swing|Hit", meta = (ClampMin = "1.0"))
	float SweepRadius = 110.f;

	/** Distance in front of the character the sphere is centred. Reach = this + SweepRadius. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Swing|Hit")
	float SweepForwardOffset = 90.f;

	/** Height above the actor origin. The goblin is 240uu tall, so chest height is ~130. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Swing|Hit")
	float SweepHeightOffset = 120.f;

	/** Cone in front of the character that counts as hittable, in degrees, total. 360 = no filter. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Swing|Hit", meta = (ClampMin = "10.0", ClampMax = "360.0"))
	float SweepArcDegrees = 160.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Swing|Hit", meta = (ClampMin = "0.0"))
	float Damage = 25.f;

	/** Carries UGSDamageExecCalculation. Defaults to UGSGE_WeaponDamage in the constructor. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Swing|Hit")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** On by default, deliberately. A melee trace you cannot see is a melee trace you cannot debug,
	 *  and the first week of a combat system is entirely debugging. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Swing|Debug")
	bool bDrawDebugSweep = true;

private:
	void OpenDamageWindow();
	void DoSweep();
	void CloseDamageWindow();
	void FinishRecovery();
	void ClearAllTimers();

	/** One entry per swing, so a single swing cannot hit the same actor twice - but a second swing
	 *  can. Cleared on activate, not on end, so a cancelled ability never leaks immunity. */
	UPROPERTY()
	TSet<TObjectPtr<AActor>> HitActorsThisSwing;

	FTimerHandle WindupTimer;
	FTimerHandle SweepTimer;
	FTimerHandle WindowTimer;
	FTimerHandle RecoveryTimer;
};
