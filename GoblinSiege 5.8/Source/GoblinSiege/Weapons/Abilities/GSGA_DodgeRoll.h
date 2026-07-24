// Universal kit: every goblin can dodge-roll regardless of weapon kit (design doc §4/§7 - 0.22s
// i-frames, committed recovery). Granted at the character level alongside torch toss, not the
// weapon level - dodge is a racial verb, not a class one (mirrors GSGA_TorchToss's pattern).
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GSGA_DodgeRoll.generated.h"

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
};
