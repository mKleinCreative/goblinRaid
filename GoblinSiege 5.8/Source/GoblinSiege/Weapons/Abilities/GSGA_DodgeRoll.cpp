#include "Weapons/Abilities/GSGA_DodgeRoll.h"
#include "Characters/GSPlayerCharacter.h"
#include "Combat/GSGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/Character.h"

UGSGA_DodgeRoll::UGSGA_DodgeRoll()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// Active exactly while the roll is in flight - GAS auto-manages add/remove for the ability's
	// lifetime, so nothing else needs to touch this tag manually.
	ActivationOwnedTags.AddTag(GSTags::State_Dodging);
}

void UGSGA_DodgeRoll::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Avatar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (Avatar && Avatar->HasAuthority())
	{
		const AGSPlayerCharacter* PlayerAvatar = Cast<AGSPlayerCharacter>(Avatar);
		const FVector DodgeDir = PlayerAvatar ? PlayerAvatar->GetDodgeDirection() : Avatar->GetActorForwardVector();

		Avatar->LaunchCharacter(DodgeDir * DodgeSpeed, true, false);

		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			if (IFrameEffectClass)
			{
				FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
				Context.AddSourceObject(this);
				const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(IFrameEffectClass, 1.f, Context);
				if (SpecHandle.IsValid())
				{
					ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
				}
			}
		}
	}

	if (UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, DodgeDurationSeconds))
	{
		WaitTask->OnFinish.AddDynamic(this, &UGSGA_DodgeRoll::OnDodgeFinished);
		WaitTask->ReadyForActivation();
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UGSGA_DodgeRoll::OnDodgeFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
