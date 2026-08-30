// Universal kit: every goblin can dodge-roll regardless of weapon kit (design doc §4/§7 - 0.22s
// i-frames, committed recovery). Granted at the character level alongside torch toss, not the
// weapon level - dodge is a racial verb, not a class one (mirrors GSGA_TorchToss's pattern).
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GSGA_DodgeRoll.generated.h"

class UAnimMontage;

UCLASS()
class GOBLINSIEGE_API UGSGA_DodgeRoll : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGSGA_DodgeRoll();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** Restores the movement values bSuppressFrictionDuringRoll zeroed. Overridden rather than
	 *  cleaning up in OnDodgeFinished so a cancelled roll - death, interrupt - restores them too. */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	UFUNCTION()
	void OnDodgeFinished();

	/** Roll speed in cm/s, applied as a single launch impulse toward the dodge direction (design
	 *  doc: "committed recovery" - no steering once you commit). A root-motion dodge montage can
	 *  replace this launch once the Scout's animation set exists (character-design-log open item);
	 *  this is a functional placeholder, not a final-feel implementation.
	 *
	 *  Raised 900 -> 1500 in #345. Michael: "dodge doesn't seem to cover enough distance." Speed was
	 *  only half the cause - see bSuppressFrictionDuringRoll, which is what actually lets the
	 *  impulse carry. Tune the two together; ACF's own UACFDirectionalDodgeAction uses a
	 *  DodgeLength of 600uu as a reference for how far a roll should go. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge")
	float DodgeSpeed = 1500.f;

	/**
	 * Hold ground friction and braking at zero for the roll, restoring them when the ability ends.
	 *
	 * THIS, NOT DodgeSpeed, IS WHY THE ROLL WAS SHORT. LaunchCharacter sets a velocity; the
	 * movement component then bleeds it off against GroundFriction (8 by default) and
	 * BrakingDecelerationWalking (2048), so almost all of a 900uu/s impulse was gone within a
	 * couple of frames and the roll read as a hop. "DodgeSpeed x DodgeDurationSeconds" was never
	 * the distance travelled - it was an upper bound the character never reached.
	 *
	 * Restored in EndAbility rather than in OnDodgeFinished, because a roll cancelled by death or
	 * an interrupt must put friction back too - a character left at zero friction slides for the
	 * rest of the raid.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge")
	bool bSuppressFrictionDuringRoll = true;

	/**
	 * MoveSpeedMultiplier applied for the roll, so the launch is not re-clamped to walking speed.
	 *
	 * MEASURED, NOT GUESSED (#345). The roll was short because CharacterMovementComponent re-clamps
	 * velocity toward MaxWalkSpeed every frame in walking mode. The player's MaxWalkSpeed is
	 * **470**, so a 1500, 2400 or 4800 DodgeSpeed all produced the same ~470uu/s roll - raising
	 * DodgeSpeed did essentially nothing, which is exactly what Michael reported twice.
	 *
	 * 6.0 lifts the ceiling to ~2820uu/s for the roll's duration, which is what finally makes
	 * DodgeSpeed the real dial. Tune DISTANCE with DodgeSpeed; this only has to be high enough not
	 * to be the limiting factor.
	 *
	 * Applied as a UGSGE_MoveSpeedScalar rather than a write to MaxWalkSpeed, per the standing rule:
	 * a cache-and-restore of MaxWalkSpeed cannot stack, and AGSPlayerCharacter::ApplyMoveSpeed is
	 * the single place speed is derived from the attribute. Removed by handle in EndAbility, beside
	 * the friction restore, so a cancelled roll cannot leave the character permanently fast.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge", meta = (ClampMin = "1.0"))
	float RollSpeedCapMultiplier = 6.f;

	/** i-frame + roll-motion window (design doc: 0.22s i-frames). ActivationOwnedTags (constructor)
	 *  keeps State.Dodging active for exactly this long, which is what blocks movement input
	 *  (AGSPlayerCharacter::Input_Move) and, once other abilities declare ActivationBlockedTags
	 *  against it, blocks attack cancels during the roll. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge")
	float DodgeDurationSeconds = 0.22f;

	/**
	 * Stamina taken by one dodge, spent through UGSStaminaComponent::TryConsume.
	 *
	 * TryConsume is ALL-OR-NOTHING by design - its own comment says a vault that takes your last 3
	 * stamina and then fails to clear the wall is worse than a vault that refuses - so a dodge that
	 * cannot be paid for simply does not happen. The ability ends before the launch, before the
	 * montage, and before State.Dodging can lock movement.
	 *
	 * SIZING THIS AGAINST REGEN MATTERS MORE THAN THE NUMBER LOOKS. The pool is 100 and regenerates
	 * 25/second with no delay, while a dodge commits for the montage's length (0.833s forward and
	 * back, 1.000s left and right). So roughly 21-25 stamina comes BACK during the roll itself, and
	 * any cost at or under that is free in practice - the spam this exists to stop would still work.
	 * 30 nets about -8 per dodge cycle, which drains a full bar in about a dozen consecutive rolls
	 * while never interfering with using one or two in a fight.
	 *
	 * If that is still too permissive the second lever is UGSStaminaComponent's RegenDelaySeconds,
	 * currently 0 - a short delay after any spend bites much harder than a bigger number here, but
	 * it also affects sprint and climb, so it is a balance decision rather than a dodge one.
	 *
	 * For scale, the Blueprint's traversal moves cost 8 (vault) and 18 (mantle).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge", meta = (ClampMin = "0.0"))
	float DodgeStaminaCost = 30.f;

	/** A short duration GameplayEffect granting State.Invulnerable for DodgeDurationSeconds - author
	 *  as a data asset (same pattern as GSCharacterBase::ApplyRespawnState's invulnerability note).
	 *  Left null-safe: the roll still moves and locks input without it, it just won't grant i-frames
	 *  until Michael authors the effect in-editor. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge")
	TSubclassOf<UGameplayEffect> IFrameEffectClass;

	// ---- Directional roll animation -------------------------------------------------------
	// Eight rolls were authored and none were ever played: this ability was a bare LaunchCharacter.
	// The four in-place variants are used here and the launch is KEPT, so travel distance is
	// unchanged (DodgeSpeed x time, exactly as before). The _RM variants would replace the launch
	// with authored displacement - that is a separate change because it moves the distance, and the
	// class comment above already flags it as the eventual destination.
	//
	// The pick is ACTOR-RELATIVE, which is correct in both of this pawn's rotation modes:
	//   free movement (bOrientRotationToMovement) - the body already faces the roll, so this
	//     resolves to Forward, which is the only one that would look right anyway;
	//   aim / face-lock (bUseControllerRotationYaw, GSPlayerCharacter.cpp:1199-1202) - facing is
	//     decoupled from input, so Back/Left/Right become reachable and correct.

	/** Roll played when dodging roughly toward the character's own facing. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge|Animation")
	TObjectPtr<UAnimMontage> DodgeMontageForward;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge|Animation")
	TObjectPtr<UAnimMontage> DodgeMontageBackward;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge|Animation")
	TObjectPtr<UAnimMontage> DodgeMontageLeft;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge|Animation")
	TObjectPtr<UAnimMontage> DodgeMontageRight;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge|Animation")
	float DodgeMontagePlayRate = 1.f;

	/** Whether the ability's commit window stretches to cover the roll animation.
	 *
	 *  FALSE keeps today's timing exactly: the ability ends after DodgeDurationSeconds (0.22s) and
	 *  input unlocks there, while the ~1.0s roll keeps playing full-body over restored movement -
	 *  i.e. the feet slide for the remainder. Nothing about balance changes.
	 *
	 *  TRUE holds State.Dodging for the montage's real length instead, so the roll finishes before
	 *  control returns. That is the standard action-game shape (i-frames are a short window INSIDE a
	 *  longer commit) and it is what makes the animation read - but it makes the dodge materially
	 *  more committal, which is a FEEL decision, so it is one checkbox to reverse.
	 *
	 *  Either way i-frames stay governed by IFrameEffectClass's own duration, not by this. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge|Animation")
	bool bCommitForFullMontage = true;

private:
	/** Chooses among the four montages by projecting the world-space dodge direction into actor
	 *  space. Returns null when nothing is assigned, which leaves the old launch-only behaviour. */
	UAnimMontage* PickDirectionalMontage(const class ACharacter* Avatar, const FVector& WorldDodgeDir) const;

	/** Zeroes ground friction and braking, remembering what they were. No-op unless
	 *  bSuppressFrictionDuringRoll. */
	void SuppressFriction(class ACharacter* Avatar);

	/** Puts back exactly what SuppressFriction cached, then disarms itself. Safe to call twice and
	 *  safe to call when suppression never ran - the same reasoning as
	 *  UGSGA_SwordLight::RestoreMoveSpeed, which restores rather than recomputes so a buff applied
	 *  mid-roll survives. */
	void RestoreFriction();

	/** -1 means "nothing cached", which is also the reset value, so a restore without a matching
	 *  suppress cannot zero the character's friction. */
	float CachedGroundFriction = -1.f;
	float CachedBrakingDeceleration = -1.f;

	/** The speed-cap effect, removed by handle in EndAbility. Same shape as UGSGA_Block's
	 *  BlockSlowHandle. */
	FActiveGameplayEffectHandle RollSpeedHandle;

	/** The avatar whose movement was altered. Held weakly: the roll can outlive its character. */
	TWeakObjectPtr<class ACharacter> FrictionAvatar;
};
