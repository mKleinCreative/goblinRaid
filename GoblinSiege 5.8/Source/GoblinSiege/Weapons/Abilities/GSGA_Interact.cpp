#include "Weapons/Abilities/GSGA_Interact.h"

#include "Combat/GSGameplayTags.h"
#include "Interaction/GSInteractionComponent.h"

UGSGA_Interact::UGSGA_Interact()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Unpredicted on purpose (multiplayer posture): a channel is a second long and its payout is
	// authoritative, so there is nothing here worth mispredicting.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	bRetriggerInstancedAbility = false;

	ActivationOwnedTags.AddTag(GSTags::State_Interacting);

	// Also an ability tag, the same way UGSGA_Block carries State.Blocking: it is what
	// UAbilitySystemComponent::CancelAbilities filters on, so a guard break can rip a channel open
	// without CancelAbilities(nullptr) taking the victim's swing and dodge with it.
	AbilityTags.AddTag(GSTags::State_Interacting);

	// Blocking on our own owned tag is what stops a second E from restarting a channel mid-loot.
	ActivationBlockedTags.AddTag(GSTags::State_Interacting);
	ActivationBlockedTags.AddTag(GSTags::State_Dead);
	ActivationBlockedTags.AddTag(GSTags::State_Dodging);
	ActivationBlockedTags.AddTag(GSTags::State_HitReact);

	// Staggered hands cannot loot (ruling 2026-08-04). This is the activation half; the in-flight
	// half is the guard break cancelling by AbilityTags above, since ActivationBlockedTags only ever
	// refuses a start and a channel begun before the kick would otherwise run to completion.
	ActivationBlockedTags.AddTag(GSTags::State_GuardBroken);
}

void UGSGA_Interact::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* /*TriggerEventData*/)
{
	bChannelEnded = false;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UGSInteractionComponent* Component = IsValid(Avatar) ? Avatar->FindComponentByClass<UGSInteractionComponent>() : nullptr;
	if (!Component)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	InteractionComponent = Component;

	// Bind BEFORE starting: a zero-second verb completes inside BeginChannel, and an ability that
	// bound afterwards would never hear its own channel end.
	Component->OnChannelEnded.AddDynamic(this, &UGSGA_Interact::HandleChannelEnded);

	if (!Component->BeginChannel() && !bChannelEnded)
	{
		// Nothing in range, nothing in the cone, nothing in hand.
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UGSGA_Interact::InputReleased(const FGameplayAbilitySpecHandle /*Handle*/,
	const FGameplayAbilityActorInfo* /*ActorInfo*/,
	const FGameplayAbilityActivationInfo /*ActivationInfo*/)
{
	if (UGSInteractionComponent* Component = InteractionComponent.Get())
	{
		Component->ReleaseInteractInput();
	}
}

void UGSGA_Interact::HandleChannelEnded(bool bCompleted, EGSInteractEndReason /*Reason*/)
{
	bChannelEnded = true;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bCompleted);
}

void UGSGA_Interact::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UGSInteractionComponent* Component = InteractionComponent.Get())
	{
		Component->OnChannelEnded.RemoveDynamic(this, &UGSGA_Interact::HandleChannelEnded);

		// Cancelled from outside - death, a stun, a blocking tag. The channel does not get to outlive
		// the ability that started it.
		if (Component->IsChannelling())
		{
			Component->AbortChannel(EGSInteractEndReason::Cancelled);
		}
	}

	InteractionComponent = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
