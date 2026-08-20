#include "Weapons/Abilities/GSGA_DodgeRoll.h"
#include "Characters/GSPlayerCharacter.h"
#include "Combat/GSGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/Character.h"
#include "Animation/AnimMontage.h"

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

	// Length of the roll animation, if one plays. Drives the commit window when
	// bCommitForFullMontage is set; stays 0 when no montage is assigned, in which case the ability
	// falls back to DodgeDurationSeconds and behaves exactly as it did before animation existed.
	float MontageSeconds = 0.f;

	ACharacter* Avatar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (Avatar && Avatar->HasAuthority())
	{
		const AGSPlayerCharacter* PlayerAvatar = Cast<AGSPlayerCharacter>(Avatar);
		const FVector DodgeDir = PlayerAvatar ? PlayerAvatar->GetDodgeDirection() : Avatar->GetActorForwardVector();

		Avatar->LaunchCharacter(DodgeDir * DodgeSpeed, true, false);

		// PlayAnimMontage returns 0 on a skeleton mismatch and says nothing about it
		// (AGSCharacterBase gates on this) - so read the returned length rather than assuming the
		// roll is on screen. A silent 0 here is the difference between a rolling goblin and a
		// goblin sliding across the floor at 900uu/s in its idle pose.
		if (UAnimMontage* Roll = PickDirectionalMontage(Avatar, DodgeDir))
		{
			const float Rate = FMath::IsNearlyZero(DodgeMontagePlayRate) ? 1.f : DodgeMontagePlayRate;
			MontageSeconds = Avatar->PlayAnimMontage(Roll, Rate);
			if (MontageSeconds <= 0.f)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[GS.Dodge] '%s' refused to play on %s - skeleton mismatch? The roll will "
					     "launch with no animation."),
					*Roll->GetName(), *Avatar->GetName());
			}
		}

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

	// Hold the commit for whichever is longer when asked to cover the animation, so State.Dodging
	// (and therefore the movement-input lock) does not release mid-roll and let the player walk
	// while a full-body montage is still playing.
	const float CommitSeconds = (bCommitForFullMontage && MontageSeconds > 0.f)
		? FMath::Max(DodgeDurationSeconds, MontageSeconds)
		: DodgeDurationSeconds;

	if (UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, CommitSeconds))
	{
		WaitTask->OnFinish.AddDynamic(this, &UGSGA_DodgeRoll::OnDodgeFinished);
		WaitTask->ReadyForActivation();
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

UAnimMontage* UGSGA_DodgeRoll::PickDirectionalMontage(const ACharacter* Avatar, const FVector& WorldDodgeDir) const
{
	if (!Avatar || WorldDodgeDir.IsNearlyZero())
	{
		return DodgeMontageForward.Get();
	}

	// Project the world-space dodge direction into the actor's own frame. GetDodgeDirection() is
	// camera-relative, but which roll to PLAY is a question about the body, not the camera.
	const FVector Flat = FVector(WorldDodgeDir.X, WorldDodgeDir.Y, 0.f).GetSafeNormal();
	const float Fwd = FVector::DotProduct(Flat, Avatar->GetActorForwardVector());
	const float Rgt = FVector::DotProduct(Flat, Avatar->GetActorRightVector());

	UAnimMontage* Chosen = (FMath::Abs(Fwd) >= FMath::Abs(Rgt))
		? (Fwd >= 0.f ? DodgeMontageForward : DodgeMontageBackward)
		: (Rgt >= 0.f ? DodgeMontageRight   : DodgeMontageLeft);

	// Any unassigned quadrant falls back to the forward roll rather than to no animation at all.
	// .Get() on both arms: mixing a raw UAnimMontage* with a TObjectPtr in one conditional is
	// ambiguous under MSVC (C2445).
	return Chosen ? Chosen : DodgeMontageForward.Get();
}

void UGSGA_DodgeRoll::OnDodgeFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
