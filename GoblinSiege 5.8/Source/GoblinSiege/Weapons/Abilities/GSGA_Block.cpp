#include "Weapons/Abilities/GSGA_Block.h"
#include "Combat/GSGameplayTags.h"
#include "Combat/GSGE_MoveSpeedScalar.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

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

	// This line is the guard break. Cancelling the block on kick impact only interrupts it - the
	// victim would jam the button and be guarding again the same frame, and the kick would have
	// bought the attacker nothing. Refusing to re-activate for the stagger window is what turns
	// the break into an actual opening.
	ActivationBlockedTags.AddTag(GSTags::State_GuardBroken);

	// Hands full, or hands busy: you cannot raise a guard around a sack, and a channel you started
	// is not interrupted by reaching for the block key (GDD §8, ruling 2026-08-04).
	ActivationBlockedTags.AddTag(GSTags::State_Carrying);
	ActivationBlockedTags.AddTag(GSTags::State_Interacting);
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

		// The guard pose is a finite clip (1.83s) but the guard lasts as long as the button is
		// held, so without this the goblin drops its arms mid-block and stands there mitigating
		// damage with an idle pose. Point the montage's first section at itself and it loops.
		// Done at runtime rather than in the asset because montage sections are not reachable
		// from the Python asset API, and a looping asset would loop everywhere it is ever used.
		if (UAnimInstance* Anim = Char->GetMesh() ? Char->GetMesh()->GetAnimInstance() : nullptr)
		{
			const FName FirstSection = BlockIdleMontage->GetSectionName(0);
			if (!FirstSection.IsNone())
			{
				Anim->Montage_SetNextSection(FirstSection, FirstSection, BlockIdleMontage);
			}
		}
	}

	// The slow is a GameplayEffect on MoveSpeedMultiplier rather than a write to MaxWalkSpeed, so it
	// compounds with a carry slow or a future root instead of racing them for the same field.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);

		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
			UGSGE_MoveSpeedScalar::StaticClass(), 1.f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(GSTags::Data_MoveSpeedScalar, BlockMoveSpeedScale);
			BlockSlowHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
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
	}

	// Remove by handle, never by class: whatever else is slowing this goblin - a sack, a root - is
	// none of the guard's business and must survive lowering it. The attribute aggregates the rest,
	// which is what the old cache-and-restore was reaching for and could not actually deliver.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (BlockSlowHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(BlockSlowHandle);
		}
	}
	BlockSlowHandle = FActiveGameplayEffectHandle();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
