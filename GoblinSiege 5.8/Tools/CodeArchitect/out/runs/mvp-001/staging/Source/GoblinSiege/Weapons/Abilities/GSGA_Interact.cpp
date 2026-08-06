#include "Weapons/Abilities/GSGA_Interact.h"

#include "Combat/GSGameplayTags.h"
#include "Interaction/GSInteractableComponent.h"
#include "Interaction/GSInteractionComponent.h"

UGSGA_Interact::UGSGA_Interact()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// State.Interacting is applied for the ability's lifetime — the tag other systems query
	// (noise scaling, the HUD, future takedown eligibility) without reaching into this class.
	ActivationOwnedTags.AddTag(GSTags::State_Interacting);

	// Same composition rule as dodge/block: committed states exclude each other.
	ActivationBlockedTags.AddTag(GSTags::State_Dead);
	ActivationBlockedTags.AddTag(GSTags::State_Dodging);
	ActivationBlockedTags.AddTag(GSTags::State_HitReact);
}

UGSInteractionComponent* UGSGA_Interact::GetInteractionComponent(const FGameplayAbilityActorInfo* ActorInfo) const
{
	return ActorInfo && ActorInfo->AvatarActor.IsValid()
		? ActorInfo->AvatarActor->FindComponentByClass<UGSInteractionComponent>()
		: nullptr;
}

bool UGSGA_Interact::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const UGSInteractionComponent* Interaction = GetInteractionComponent(ActorInfo);
	return Interaction && Interaction->GetFocusedInteractable() != nullptr;
}

void UGSGA_Interact::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	UGSInteractionComponent* Interaction = GetInteractionComponent(ActorInfo);
	if (!Interaction || !Interaction->BeginInteract())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!bDelegatesBound)
	{
		Interaction->OnChannelCompleted.AddDynamic(this, &UGSGA_Interact::HandleChannelCompleted);
		Interaction->OnChannelAborted.AddDynamic(this, &UGSGA_Interact::HandleChannelAborted);
		bDelegatesBound = true; // InstancedPerActor: bind once, not once per activation
	}
}

void UGSGA_Interact::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true); // release-abort
	}
}

void UGSGA_Interact::HandleChannelCompleted(UGSInteractableComponent* /*Interactable*/)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UGSGA_Interact::HandleChannelAborted(UGSInteractableComponent* /*Interactable*/)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UGSGA_Interact::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bWasCancelled)
	{
		if (UGSInteractionComponent* Interaction = GetInteractionComponent(ActorInfo))
		{
			Interaction->EndInteract(); // cancellation from any path aborts the channel exactly once
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
