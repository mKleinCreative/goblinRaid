#include "Weapons/Abilities/GSGA_Block.h"
#include "Combat/GSGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"

UGSGA_Block::UGSGA_Block()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// ActivationOwnedTags is what makes this work: GAS adds State.Blocking on activate and removes
	// it on end, automatically, including on cancellation and on death. Managing the tag by hand in
	// Activate/End would leave a permanently-blocking corpse the first time something cancelled the
	// ability down a path nobody thought about.
	ActivationOwnedTags.AddTag(GSTags::State_Blocking);

	// Also an ability tag, which is what UAbilitySystemComponent::CancelAbilities filters on.
	// Without this the only way to drop the guard on button release is CancelAbilities(nullptr),
	// which cancels EVERY ability - so letting go of block would also cancel a swing in flight.
	AbilityTags.AddTag(GSTags::State_Blocking);

	ActivationBlockedTags.AddTag(GSTags::State_Dead);
	ActivationBlockedTags.AddTag(GSTags::State_Dodging);
}

void UGSGA_Block::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Char)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (BlockIdleMontage)
	{
		Char->PlayAnimMontage(BlockIdleMontage, MontagePlayRate);
	}

	if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
	{
		CachedMaxWalkSpeed = Move->MaxWalkSpeed;
		Move->MaxWalkSpeed = CachedMaxWalkSpeed * BlockMoveSpeedScale;
	}

	// No timer and no end condition here on purpose: the ability stays active until the input is
	// released, which is the character's job to tell us about. A duration would mean the guard
	// dropping on its own mid-fight, which is a stamina system wearing a trenchcoat.
}

void UGSGA_Block::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (BlockIdleMontage)
		{
			Char->StopAnimMontage(BlockIdleMontage);
		}

		// Restore rather than recompute: whatever the speed was when the guard went up is what it
		// should be when the guard comes down, including any buff or slow that was already applied.
		if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
		{
			if (CachedMaxWalkSpeed > 0.f)
			{
				Move->MaxWalkSpeed = CachedMaxWalkSpeed;
			}
		}
	}
	CachedMaxWalkSpeed = 0.f;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
