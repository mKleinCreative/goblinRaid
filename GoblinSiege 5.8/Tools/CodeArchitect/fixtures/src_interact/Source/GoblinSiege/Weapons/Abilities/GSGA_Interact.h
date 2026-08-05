// GAS entry gate for the interact channel. Deliberately thin: UGSInteractionComponent owns the
// channel (progress, abort rules); this ability owns WHO MAY START ONE and the State.Interacting
// tag other systems key off. Blocked while dodging, dead, or hit-reacting via ActivationBlockedTags
// — the same pattern UGSGA_DodgeRoll and UGSGA_Block established, so kit abilities compose without
// bespoke checks. Input flow: press activates, release cancels; both routed from the character's
// Interact input action (see NOTES patch instructions).
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GSGA_Interact.generated.h"

class UGSInteractionComponent;

UCLASS()
class GOBLINSIEGE_API UGSGA_Interact : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGSGA_Interact();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** Release of the Interact input — routed here by the character. Ends the ability, which
	 *  aborts the channel (release-abort rule). */
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UGSInteractionComponent* GetInteractionComponent(const FGameplayAbilityActorInfo* ActorInfo) const;

	UFUNCTION()
	void HandleChannelCompleted(class UGSInteractableComponent* Interactable);

	UFUNCTION()
	void HandleChannelAborted(class UGSInteractableComponent* Interactable);

	bool bDelegatesBound = false;
};
