// The GAS face of the hold-E framework (GDD §8). Universal kit like torch toss and dodge roll, so
// it lives beside them and is granted at the character level, not the weapon level. The ability
// owns activation gating and the State.Interacting tag; the seconds, the progress, and the abort
// rules live on UGSInteractionComponent - a HUD bar that only works while an ability instance
// happens to exist would be the wrong split.
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Interaction/GSInteractionComponent.h"
#include "GSGA_Interact.generated.h"

UCLASS()
class GOBLINSIEGE_API UGSGA_Interact : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGSGA_Interact();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** Only fires when the ability was activated through an ASC input ID. The character's release
	 *  handler calls the component directly for the TryActivateAbilityByClass route; both paths land
	 *  on the same abort. */
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	UFUNCTION()
	void HandleChannelEnded(bool bCompleted, EGSInteractEndReason Reason);

private:
	TWeakObjectPtr<UGSInteractionComponent> InteractionComponent;

	/** Set when the channel ended inside ActivateAbility - an instant verb finishes before the
	 *  activation call returns, and the ability must not then end itself twice. */
	bool bChannelEnded = false;
};
