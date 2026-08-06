// The sword combo. One ability, N stages - not one ability per swing.
//
// Why one ability: a chain of separate abilities means N grants, N input bindings, N places for
// the "which swing am I on" state to disagree, and a re-activation race every time the chain
// advances. A single InstancedPerActor ability owns the stage index for as long as the chain
// lives, and GAS's own refusal to re-activate an active ability is what turns a second button
// press into a BUFFERED input rather than a competing swing.
//
// Why timers rather than anim notify states: notify timings live inside a montage asset, and
// structural montage edits hard-crash the editor while its editor window is open. These numbers
// get tuned twenty times an hour. They belong on a Blueprint CDO you can drag during PIE.
// Moving the window onto notifies later is mechanical, and should happen once the feel is locked.
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GSGA_SwordLight.generated.h"

class UAnimMontage;
class UGameplayEffect;

/** One swing in the chain. Every number that decides how a hit feels lives here. */
USTRUCT(BlueprintType)
struct FGSSwingStage
{
	GENERATED_BODY()

	/** Optional - a stage with no montage still swings and still hits, which is the right
	 *  fallback for testing timing before the animation exists. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0.1"))
	float MontagePlayRate = 1.5f;

	/** Press-to-contact. Set it so the damage window straddles the montage's swing apex - the
	 *  apex can be measured rather than guessed by sampling the sword arm's angular speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float WindupSeconds = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.02"))
	float DamageWindowSeconds = 0.16f;

	/** Committed tail. This is also the back half of the combo input buffer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float RecoverySeconds = 0.30f;

	/**
	 * MaxWalkSpeed multiplier held from the swing starting until the damage window closes.
	 *
	 * Attacking never stops the character - being rooted mid-swing is what makes melee feel like
	 * a turn-based exchange rather than a fight - but committing to a swing should cost mobility
	 * in proportion to what the swing is worth. A light attack stays mobile enough to chase; a
	 * heavy should feel like it plants you. 1.0 disables the slow entirely.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MoveSpeedScale = 0.55f;

	/**
	 * MaxWalkSpeed multiplier for the recovery tail, applied when the damage window closes.
	 *
	 * Separate from the swing scale because the two do different jobs: the swing slow is the
	 * commitment, the recovery slow is how long that commitment keeps costing you. Setting this
	 * above MoveSpeedScale lets a character start peeling away as the blade comes back.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RecoveryMoveSpeedScale = 0.75f;

	/**
	 * Forward launch speed in uu/s, fired the instant the damage window opens. 0 = no lunge.
	 *
	 * Deliberately tied to the damage window rather than the windup, so the step and the strike
	 * land together and it reads as one committed motion instead of a hop followed by a swing.
	 * The character's braking bleeds it off, so this is a shove, not a dash - a value near the
	 * character's walk speed moves them roughly a body's width.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float LungeSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (ClampMin = "0.0"))
	float Damage = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (ClampMin = "1.0"))
	float SweepRadius = 110.f;

	/** Distance in front of the character the sphere is centred. Reach = this + SweepRadius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	float SweepForwardOffset = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	float SweepHeightOffset = 120.f;

	/** Total arc in front that counts as hittable. 360 for a spin finisher. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (ClampMin = "10.0", ClampMax = "360.0"))
	float SweepArcDegrees = 160.f;

	/** Whether a hit rips the target's guard open. A guard break is not a damage stat - a kick
	 *  that merely hurts a bit is a worse light attack, not a new verb - so damage stays low and
	 *  the value is entirely in the opening it creates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bBreaksGuard = false;

	/** How long State.GuardBroken is held on the target. This is the whole mechanic: it is the
	 *  window in which they cannot re-raise the guard, so it should be long enough to land a real
	 *  punish and short enough that a whiffed guard break is not a free combo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat",
		meta = (ClampMin = "0.0", EditCondition = "bBreaksGuard"))
	float GuardBreakStaggerSeconds = 1.2f;

	/** Damage multiplier applied to a target whose guard this hit actually broke. 1.0 keeps the
	 *  break purely tactical; above 1 makes turtling into a kick genuinely costly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat",
		meta = (ClampMin = "1.0", EditCondition = "bBreaksGuard"))
	float GuardBreakDamageScale = 1.f;
};

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

	/** Called by the character when the attack button is pressed while this ability is already
	 *  running. Returns true if the press was accepted into the buffer. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Swing")
	bool BufferComboInput();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Swing")
	int32 GetCurrentStageIndex() const { return CurrentStage; }

protected:
	/** The chain, in order. One entry = a single non-combo attack, which is a valid setup. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Swing")
	TArray<FGSSwingStage> Stages;

	/** Carries UGSDamageExecCalculation. Defaults to UGSGE_WeaponDamage in the constructor. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Swing")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** How often the damage window re-sweeps. 60Hz keeps fast swings from stepping over a target. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Swing", meta = (ClampMin = "0.008"))
	float SweepIntervalSeconds = 0.0167f;

	/** Open the combo buffer as soon as the windup ends, rather than only during recovery. Off
	 *  feels stricter and reads as "time your follow-up"; on feels forgiving. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Swing")
	bool bBufferOpensAtDamageWindow = true;

	/** On by default. A melee trace you cannot see is a melee trace you cannot debug. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Swing|Debug")
	bool bDrawDebugSweep = true;

private:
	void RunStage();
	void OpenDamageWindow();
	void DoSweep();
	/** Rips a blocking target's guard open: cancels the block by tag and holds State.GuardBroken
	 *  for the stagger window. Returns true only if there was actually a guard to break, which is
	 *  what lets the caller decide whether the damage scale applies. */
	bool BreakGuard(AActor* Target, UAbilitySystemComponent* TargetASC, const FGSSwingStage& S);

	void CloseDamageWindow();
	void FinishRecovery();
	void ClearAllTimers();
	const FGSSwingStage& GetStage() const;

	/** Scales MaxWalkSpeed off the speed the character had when the ability STARTED, caching it
	 *  once on first use. Caching per stage would compound: stage 2 would scale an already-scaled
	 *  value and a three-hit combo would grind to a halt by the end of it. */
	void ApplyMoveSpeedScale(float Scale);

	/** Restores rather than recomputes, so any buff or slow applied mid-swing survives the swing.
	 *  Same reasoning as UGSGA_Block::EndAbility. */
	void RestoreMoveSpeed();

	/** Forward shove at the strike. No-op when the stage's LungeSpeed is 0. */
	void ApplyLunge(const FGSSwingStage& S);

	/** 0 means "nothing cached yet", which is also the reset value - so a restore that runs
	 *  without a matching apply cannot zero the character's walk speed. */
	float CachedMaxWalkSpeed = 0.f;

	/** Cleared per STAGE, not per ability: each swing in a chain gets its own fresh hit set, so a
	 *  three-hit combo lands three times on one target - but one swing never double-hits. */
	UPROPERTY()
	TSet<TObjectPtr<AActor>> HitActorsThisSwing;

	int32 CurrentStage = 0;
	bool bComboQueued = false;
	bool bBufferOpen = false;

	FTimerHandle WindupTimer;
	FTimerHandle SweepTimer;
	FTimerHandle WindowTimer;
	FTimerHandle RecoveryTimer;
};
