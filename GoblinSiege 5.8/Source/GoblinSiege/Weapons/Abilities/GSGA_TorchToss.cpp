#include "Weapons/Abilities/GSGA_TorchToss.h"
#include "Combat/GSAimComponent.h"
#include "Combat/GSGameplayTags.h"
#include "Destruction/GSTorchProjectile.h"
#include "Weapons/GSWeaponComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"

// One warning per session for a broken montage reference, not one per goblin. See the use site.
static bool GThrowMontageResolveFailed = false;

UGSGA_TorchToss::UGSGA_TorchToss()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// C++ default, 2026-08-01 - see the header. Null here meant every torch throw in the game
	// committed its cost and spawned nothing, which made fire (the only closed damage loop in the
	// project) unreachable in play.
	TorchProjectileClass = AGSTorchProjectile::StaticClass();

	// The authored throw animation, which until now was referenced by nothing in C++. Soft, so
	// recording it here costs no package load at module time.
	ThrowMontage = TSoftObjectPtr<UAnimMontage>(
		FSoftObjectPath(TEXT("/Game/Characters/ScoutV2/Montages/AM_GS_ThrowTorch.AM_GS_ThrowTorch")));

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

	// The throw animation. AM_GS_ThrowTorch has existed since the montage pass and was referenced by
	// NOTHING in C++ - the throw was a timer and a spawn, with the goblin standing perfectly still.
	//
	// PlayAnimMontage on the character, matching UGSGA_SwordLight's stage montages rather than an
	// AbilityTask: the wind-up timer below already owns this ability's lifetime, and adding a second
	// task that also wants to own it is how an ability ends twice.
	//
	// The spawn is still driven by TorchWindupSeconds, NOT by a notify in the montage. Moving it onto
	// an AnimNotify is the right end state - it is what keeps the release synced if the animation is
	// ever retimed - but the notify has to be authored in the editor, and a C++ contract for an event
	// no asset sends would be worse than this honest coupling. Set TorchWindupSeconds to the montage's
	// release moment.
	if (!ResolvedThrowMontage && !GThrowMontageResolveFailed)
	{
		if (UAnimMontage* Loaded = ThrowMontage.IsNull() ? nullptr : ThrowMontage.LoadSynchronous())
		{
			ResolvedThrowMontage = Loaded;
		}
		else
		{
			// File-scope latch, not a member: this ability is InstancedPerActor, so a member would
			// warn once per goblin rather than once per session. Same reasoning as the projectile
			// latches fixed in #030 and #034, arrived at from the opposite direction.
			GThrowMontageResolveFailed = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[GoblinSiege] Torch toss has no usable throw montage (%s) - the throw will have "
					 "no animation. It still spawns, still flies, and still burns."),
				ThrowMontage.IsNull() ? TEXT("reference is unset") : *ThrowMontage.ToString());
		}
	}

	if (ResolvedThrowMontage)
	{
		if (ACharacter* AvatarChar = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			AvatarChar->PlayAnimMontage(ResolvedThrowMontage, ThrowMontagePlayRate);
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
	if (!Avatar || !TorchProjectileClass || !Avatar->HasAuthority())
	{
		return;
	}

	// 2026-08-04: the spawn transform now comes from UGSAimComponent, which is also what drew the
	// arc the player aimed with. Before this, these constants existed HERE and again in
	// AGSPlayerCharacter::DrawTorchAimArc, kept in step by a comment reading "Must match
	// UGSGA_TorchToss::ThrowTorch exactly, including the +50 hand-height fudge" - and a preview that
	// drifts out of step with the spawn is an aim indicator that lies about where the torch lands,
	// which is the one thing an aim indicator must never do.
	//
	// FindComponentByClass rather than a cast to AGSPlayerCharacter, matching this file's existing
	// reasoning for SetTorchReadied: torch toss is a RACIAL verb every goblin has, and AI-controlled
	// goblins are not AGSPlayerCharacters.
	FTransform Muzzle;
	if (const UGSAimComponent* AimComp = Avatar->FindComponentByClass<UGSAimComponent>())
	{
		Muzzle = AimComp->GetMuzzleTransform();
	}
	else
	{
		// A pawn with no aim component throws from control rotation at roughly hand height - the
		// pre-2026-08-04 behaviour exactly. Degradation, not an error.
		const FRotator AimRotation = Avatar->GetControlRotation();
		Muzzle = FTransform(AimRotation,
			Avatar->GetActorLocation() + AimRotation.Vector() * 80.f + FVector(0.f, 0.f, 50.f));
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Avatar;
	SpawnParams.Instigator = Avatar;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Avatar->GetWorld()->SpawnActor<AGSTorchProjectile>(
		TorchProjectileClass, Muzzle.GetLocation(), Muzzle.GetRotation().Rotator(), SpawnParams);
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
