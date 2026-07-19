#include "Weapons/Abilities/GSGA_TorchToss.h"
#include "Destruction/GSTorchProjectile.h"
#include "GameFramework/Character.h"

UGSGA_TorchToss::UGSGA_TorchToss()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
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

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
