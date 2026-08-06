#include "Weapons/Abilities/GSGA_BowShot.h"
#include "Combat/GSAimComponent.h"
#include "Combat/GSGameplayTags.h"
#include "Weapons/GSArrowProjectile.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

UGSGA_BowShot::UGSGA_BowShot()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	ArrowProjectileClass = AGSArrowProjectile::StaticClass();

	// No asset tags. There is deliberately nothing to put in them: this project's tag set has no
	// Ability.* namespace, and State.Aiming - the only near-fit - is a LOOSE tag owned by
	// AGSPlayerCharacter::UpdateRotationMode describing the character, not a label for this ability.
	// Borrowing it here would make a CancelAbilities(State.Aiming) call cancel a shot in flight, and
	// that is the kind of coupling that is invisible until the day something adds that call.
	//
	// Recorded here because the obvious way to add tags later is the AbilityTags member, and that is
	// deprecated in 5.8 (C4996): use SetAssetTags(). GSGA_Block and GSGA_Interact still use the old
	// member and are already on the migration list (AGENT_STATE.md) - do not add a third.

	ActivationBlockedTags.AddTag(GSTags::State_Dead);
	ActivationBlockedTags.AddTag(GSTags::State_Dodging);

	// Hands full, hands busy mid-channel, or staggered with the guard kicked open. The same four
	// gates UGSGA_SwordLight and UGSGA_Interact use - a bow is not a loophole around a stagger.
	ActivationBlockedTags.AddTag(GSTags::State_Carrying);
	ActivationBlockedTags.AddTag(GSTags::State_Interacting);
	ActivationBlockedTags.AddTag(GSTags::State_GuardBroken);
}

void UGSGA_BowShot::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ReleaseDelaySeconds > 0.f)
	{
		if (UAbilityTask_WaitDelay* ReleaseTask = UAbilityTask_WaitDelay::WaitDelay(this, ReleaseDelaySeconds))
		{
			ReleaseTask->OnFinish.AddDynamic(this, &UGSGA_BowShot::OnReleaseFinished);
			ReleaseTask->ReadyForActivation();
			return;
		}
	}

	FireArrow();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGSGA_BowShot::OnReleaseFinished()
{
	FireArrow();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGSGA_BowShot::FireArrow()
{
	ACharacter* Avatar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Avatar || !ArrowProjectileClass || !Avatar->HasAuthority())
	{
		return;
	}

	// Ask the aim component where the shot comes from. This is the SAME call the arc preview made
	// while the player was holding the button, which is what guarantees the arrow leaves from the
	// line they were shown. FindComponentByClass rather than a cast to AGSPlayerCharacter, matching
	// UGSGA_TorchToss's reasoning: an AI archer is not a player character.
	FTransform Muzzle;
	if (const UGSAimComponent* AimComp = Avatar->FindComponentByClass<UGSAimComponent>())
	{
		Muzzle = AimComp->GetMuzzleTransform();
	}
	else
	{
		// No aim component (an AI pawn that was never given one). Fall back to the pawn's own
		// control rotation from roughly hand height - degraded, not broken.
		const FRotator AimRotation = Avatar->GetControlRotation();
		Muzzle = FTransform(AimRotation,
			Avatar->GetActorLocation() + AimRotation.Vector() * 80.f + FVector(0.f, 0.f, 50.f));
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Avatar;
	SpawnParams.Instigator = Avatar;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Avatar->GetWorld()->SpawnActor<AGSArrowProjectile>(
		ArrowProjectileClass, Muzzle.GetLocation(), Muzzle.GetRotation().Rotator(), SpawnParams);
}
