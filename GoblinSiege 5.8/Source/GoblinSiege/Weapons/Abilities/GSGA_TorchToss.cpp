#include "Weapons/Abilities/GSGA_TorchToss.h"
#include "Combat/GSGameplayTags.h"
#include "Destruction/GSTorchProjectile.h"
#include "Weapons/GSWeaponComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/Character.h"

UGSGA_TorchToss::UGSGA_TorchToss()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// C++ default, 2026-08-01 - see the header. Null here meant every torch throw in the game
	// committed its cost and spawned nothing, which made fire (the only closed damage loop in the
	// project) unreachable in play.
	TorchProjectileClass = AGSTorchProjectile::StaticClass();

	// A full-handed goblin has to drop what it is holding before it can throw (ruling 2026-08-04).
	ActivationBlockedTags.AddTag(GSTags::State_Carrying);
}

void UGSGA_TorchToss::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Torch into the hand FIRST. Runs on every machine that activates the ability, not just the
	// server: the prop is cosmetic, and a client whose own torch only appears after a round trip
	// would feel the input as laggy even though the throw itself is server-authoritative.
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		if (UGSWeaponComponent* WeaponComp = Avatar->FindComponentByClass<UGSWeaponComponent>())
		{
			// FindComponentByClass rather than AGSPlayerCharacter::GetWeaponComponent(): torch toss
			// is a RACIAL verb granted to every goblin (design doc §5), and AI-controlled goblins
			// are not AGSPlayerCharacters. A pawn with no weapon component simply throws without a
			// visible wind-up prop, which is a degradation and not an error.
			WeaponComp->SetTorchReadied(true);
		}
	}

	// Wind-up, so the readied torch is actually seen before it leaves. Same shape as
	// UGSGA_DodgeRoll: a WaitDelay task that owns the ability's remaining lifetime, with a
	// straight-through fallback if the task can't be created.
	if (TorchWindupSeconds > 0.f)
	{
		if (UAbilityTask_WaitDelay* WindupTask = UAbilityTask_WaitDelay::WaitDelay(this, TorchWindupSeconds))
		{
			WindupTask->OnFinish.AddDynamic(this, &UGSGA_TorchToss::OnTorchWindupFinished);
			WindupTask->ReadyForActivation();
			return;
		}
	}

	ThrowTorch();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGSGA_TorchToss::OnTorchWindupFinished()
{
	ThrowTorch();
	// The held torch is dropped by EndAbility below, which is the single un-ready path for both
	// the successful throw and every cancellation.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGSGA_TorchToss::ThrowTorch()
{
	ACharacter* Avatar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (Avatar && TorchProjectileClass && Avatar->HasAuthority())
	{
		// Aim along control rotation (camera-forward for players, controller focal for AI).
		const FRotator AimRotation = Avatar->GetControlRotation();
		const FVector SpawnLocation = Avatar->GetActorLocation()
			+ AimRotation.Vector() * SpawnForwardOffset
			+ FVector(0.f, 0.f, 50.f); // roughly hand height on the goblin rig

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Avatar;
		SpawnParams.Instigator = Avatar;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		Avatar->GetWorld()->SpawnActor<AGSTorchProjectile>(
			TorchProjectileClass, SpawnLocation, AimRotation, SpawnParams);
	}
}

void UGSGA_TorchToss::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// One un-ready path for every exit: thrown, cancelled, interrupted, or failed to commit. The
	// held prop must never outlive the ability that put it in his hand.
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		if (UGSWeaponComponent* WeaponComp = Avatar->FindComponentByClass<UGSWeaponComponent>())
		{
			WeaponComp->SetTorchReadied(false);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
