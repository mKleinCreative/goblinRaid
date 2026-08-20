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

	UFUNCTION()
	void OnDodgeFinished();

	/** Roll speed in cm/s, applied as a single launch impulse toward the dodge direction (design
	 *  doc: "committed recovery" - no steering once you commit). A root-motion dodge montage can
	 *  replace this launch once the Scout's animation set exists (character-design-log open item);
	 *  this is a functional placeholder, not a final-feel implementation. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge")
	float DodgeSpeed = 900.f;

	/** i-frame + roll-motion window (design doc: 0.22s i-frames). ActivationOwnedTags (constructor)
	 *  keeps State.Dodging active for exactly this long, which is what blocks movement input
	 *  (AGSPlayerCharacter::Input_Move) and, once other abilities declare ActivationBlockedTags
	 *  against it, blocks attack cancels during the roll. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge")
	float DodgeDurationSeconds = 0.22f;

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
};
