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

/**
 * Master switch for combat debug drawing - the `GS.Combat.Debug` cvar. Defined in
 * GSGA_SwordLight.cpp, which is where the cvar itself lives.
 *
 * DECLARED HERE because it was declared NOWHERE (#167). GSArrowProjectile.cpp called it and compiled
 * only because the unity build happened to put both files in one translation unit; adding an
 * unrelated include to this file re-split the blob and the call became "identifier not found". A
 * free function used across files needs a declaration, not luck about how the build groups them.
 */
GOBLINSIEGE_API bool GSCombatDebugEnabled();

/** One swing in the chain. Every number that decides how a hit feels lives here. */
USTRUCT(BlueprintType)
struct FGSSwingStage
{
	GENERATED_BODY()

	/** Optional - a stage with no montage still swings and still hits, which is the right
	 *  fallback for testing timing before the animation exists. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	/**
	 * Alternative animations for this same swing, picked at random alongside Montage.
	 *
	 * THIS IS VARIETY, NOT A COMBO. A combo is a chain of DIFFERENT stages with different damage and
	 * different timing, advanced by input; this is one attack that does not look identical every
	 * time. Michael asked for exactly that distinction - "I don't want the full combo, I want this
	 * attack" and then "varying it up to use this attack sometimes".
	 *
	 * Variants may be different lengths. The stage's own timings stay authoritative and the play
	 * rate is derived per swing so whichever animation is chosen finishes exactly as the stage does
	 * - see RunStage. That means a longer variant plays slightly faster rather than being cut off,
	 * which is the failure this whole pass was fixing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TArray<TObjectPtr<UAnimMontage>> MontageVariants;

	/**
	 * Play rate for Montage, used only when bFitMontageToStage is false.
	 *
	 * When fitting is on this is ignored: the rate is computed from the chosen animation's length so
	 * it lands on the stage's own duration.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0.1"))
	float MontagePlayRate = 1.5f;

	/**
	 * Scale the chosen animation so it fills exactly windup + damage window + recovery.
	 *
	 * OFF by default, and that default is deliberate. Fitting is REQUIRED wherever MontageVariants
	 * are used - A1 is 1.32s and C1 is 1.50s, so no single fixed rate can fit both and one of them
	 * would always be wrong - but turning it on globally silently retimes every other attack in the
	 * project against stage numbers that were tuned for a different montage entirely.
	 *
	 * That is not hypothetical: with this defaulted ON, the Knight was measured playing
	 * AM_KN_Atk_Stab (1.50s) at rate 1.67 to squeeze into its untouched 0.90s stage - a swing sped
	 * up by two thirds. The player's axe chain would have been retimed the same way, and Michael had
	 * just said the player's attack was fine. An opt-in flag changes only the abilities that ask for
	 * it; an opt-out flag changes everything and waits to be noticed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	bool bFitMontageToStage = false;

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

	/**
	 * Damage dealt to breakable PROPS by this swing, in hit points rather than health.
	 *
	 * Separate from Damage on purpose: props are not combatants and have no health attribute, no
	 * armour and no race. A crate takes 1 hit, a statue 4, and that scale has nothing to do with the
	 * 25 health a militiaman loses. 0 disables prop smashing for this stage entirely.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (ClampMin = "0"))
	int32 SmashDamage = 1;

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

	// ---- Hitstop (#353) ----------------------------------------------------------------------
	// Both parties freeze for a few frames on contact. Per-stage so a light, a heavy and a
	// guard-break each land with their own weight; 0 disables it for the stage.

	/** Wall-clock seconds both attacker and victim are held on contact. 0.06 is a light tap; a
	 *  heavy wants roughly double. Above ~0.15 it starts to read as lag rather than impact. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float HitstopSeconds = 0.06f;

	/** Time dilation during the hold. 0 is a dead freeze; 0.05 keeps a whisper of motion so the
	 *  pose does not read as a dropped frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HitstopScale = 0.05f;

	/** Multiplier on HitstopSeconds when this hit actually broke a guard. A kick that rips a shield
	 *  open should land harder than the same kick into empty air. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop", meta = (ClampMin = "1.0", EditCondition = "bBreaksGuard"))
	float GuardBreakHitstopScale = 2.f;

	// ---- Camera shake (#355) -------------------------------------------------------------------
	// The PLAYER's camera only - a shake on an AI's camera is a shake on nothing. Which class plays
	// is chosen by weight: bHeavyShake picks the larger, longer, rolling shake.

	/** Play a camera shake on the attacker's own camera when this stage connects. Off for stages
	 *  that should feel like a tap rather than a blow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop")
	bool bCameraShakeOnHit = true;

	/** Use the heavy shake class instead of the light one. Set on heavy and guard-break stages. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop", meta = (EditCondition = "bCameraShakeOnHit"))
	bool bHeavyShake = false;
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

	/**
	 * Recovers a REAL impact for a target the overlap already accepted: contact point, surface
	 * normal, the bone that was struck and the physical material.
	 *
	 * WHY THIS EXISTS (#349). The damage window is an OverlapMultiByObjectType, and an overlap
	 * carries none of that - no impact point, no normal, no bone, no material. Until now the code
	 * fabricated a hit from the target's actor location and said so in its own comment. Three
	 * things were impossible as a direct result:
	 *
	 *   - impact FX could not be placed (they would spawn at the victim's feet, on no material);
	 *   - nothing could tell a head from an arm, so no hit-location feedback and no dismemberment;
	 *   - ACF's hit reactions and ragdoll impulse were fed a synthetic direction.
	 *
	 * The overlap stays as the CANDIDATE FINDER - it is cheap, and it already carries the arc, race,
	 * dead and invulnerable gating that decides whether a hit counts at all. This only upgrades the
	 * DATA for a target already accepted, so no hit can be lost by adding it.
	 *
	 * Returns false when the trace finds nothing - a capsule-only target, or a mesh the ray misses at
	 * this angle. Callers MUST fall back to the synthesised hit rather than dropping the hit: a swing
	 * that connected must never stop dealing damage because its cosmetic trace missed.
	 */
	bool ResolveImpact(AActor* Target, const FVector& SweepOrigin, FHitResult& OutHit) const;

	/** Chooses this swing's animation from Montage plus MontageVariants, all equally likely. Null
	 *  when the stage has no animation at all, which is a valid setup - the swing still hits. */
	UAnimMontage* PickStageMontage(const FGSSwingStage& S) const;

	/**
	 * Stops root motion driving the character for this swing, and remembers to put it back.
	 *
	 * WHY (#350). The attack animations carry root motion - `A_GOB_DA_Combo_C1/C2/C3_RM` all have
	 * `bEnableRootMotion` - and a root-motion montage makes the character root-motion-driven for its
	 * whole duration. On the ground that is the authored lunge and it is wanted. **In the air it
	 * overrides gravity, so the goblin stops falling and hangs there** - Michael: "when you jump, it
	 * freezes you in mid air". That is the same failure #128 recorded and fixed by stripping root
	 * motion; the CombatMasterBundle retargets reintroduced it.
	 *
	 * Suppressing it only WHILE FALLING keeps the grounded swing exactly as authored and turns the
	 * air attack into a real verb rather than a freeze - which is the point: a goblin that can hit
	 * you on the way past is the mobility the design keeps promising.
	 */
	void SuppressRootMotionIfAirborne();

	/** Restores whatever root-motion mode SuppressRootMotionIfAirborne replaced. Safe to call twice
	 *  and safe when suppression never ran, so every EndAbility path can call it unconditionally. */
	void RestoreRootMotionMode();

	/** Set only while this swing suppressed root motion, so a restore cannot clobber a mode the
	 *  ability never touched. */
	bool bRootMotionSuppressed = false;

	/** What the anim instance was using before we changed it. */
	TEnumAsByte<ERootMotionMode::Type> CachedRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;

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
