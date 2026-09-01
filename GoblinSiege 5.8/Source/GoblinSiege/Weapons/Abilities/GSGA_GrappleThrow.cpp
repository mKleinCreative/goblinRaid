#include "Weapons/Abilities/GSGA_GrappleThrow.h"
#include "Weapons/GSGrappleHookProjectile.h"
#include "Components/ACFAbilitySystemComponent.h"
#include "Combat/GSAimComponent.h"
#include "Combat/GSGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"

// One warning per session for a broken reference, not one per goblin. This ability is
// InstancedPerActor, so a member latch would warn once per pawn - the same reasoning
// GSGA_TorchToss.cpp records for GThrowMontageResolveFailed.
static bool GGrappleMontageResolveFailed = false;

UGSGA_GrappleThrow::UGSGA_GrappleThrow()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// C++ default - see the header comment on HookProjectileClass for why this replaced a
	// TSoftClassPtr onto BP_GrappleHook (#390).
	HookProjectileClass = AGSGrappleHookProjectile::StaticClass();

	// This ability now derives UACFGameplayAbility (#390) but does not use ACF's cost/cooldown
	// pipeline - see the header comment. Leaving bAutoStartCooldown at its FActionConfig default
	// (true) would have EndAbility commit a cooldown GameplayEffect this ability never set up.
	ActionConfig.bAutoStartCooldown = false;

	// Shares the torch's throw animation until AM_GS_GrappleThrow exists. Soft, so recording it
	// here costs no package load at module time.
	ThrowMontage = TSoftObjectPtr<UAnimMontage>(
		FSoftObjectPath(TEXT("/Game/Characters/ScoutV2/Montages/AM_GS_ThrowTorch.AM_GS_ThrowTorch")));

	// A full-handed goblin drops what it is holding before it can throw (ruling 2026-08-04),
	// same as the torch.
	ActivationBlockedTags.AddTag(GSTags::State_Carrying);
	ActivationBlockedTags.AddTag(GSTags::State_Dead);

	// NOT self-blocking yet. That wants a State.Grappling tag in GSGameplayTags, which this ticket
	// does not claim - so a fast double-press can currently throw twice. Recorded rather than
	// silently reached into an unclaimed file.
}

void UGSGA_GrappleThrow::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Arms ACF's priority arbitration for this activation - see the header comment for why this is
	// called directly instead of Super::ActivateAbility (which runs a cost/warp/montage pipeline
	// this ability does not use, and would silently no-op the whole throw without a
	// UACFGASStatisticsComponent). GetACFAbilityComponent() resolves from OnAvatarSet, which this
	// ability does not override, so it is valid here.
	if (UACFAbilitySystemComponent* ACFComp = GetACFAbilityComponent())
	{
		ACFComp->OnAbilityStarted(this);
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Resolve the montage once per instance, and degrade to a poseless throw if it is missing -
	// the throw still works, it just has no animation. Same shape as the torch.
	if (!ResolvedThrowMontage && !GGrappleMontageResolveFailed)
	{
		if (UAnimMontage* Loaded = ThrowMontage.IsNull() ? nullptr : ThrowMontage.LoadSynchronous())
		{
			ResolvedThrowMontage = Loaded;
		}
		else
		{
			GGrappleMontageResolveFailed = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[GS.Grapple] ThrowMontage failed to resolve - the throw will have no pose."));
		}
	}

	if (ACharacter* Avatar = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (ResolvedThrowMontage)
		{
			Avatar->PlayAnimMontage(ResolvedThrowMontage, ThrowMontagePlayRate);
		}
	}

	// The spawn is driven by GrappleWindupSeconds, not by a montage notify. Same honest coupling
	// the torch documents: a notify is the right end state, but it has to be authored in the editor
	// and a C++ contract for an event no asset sends would be worse.
	//
	// UAbilityTask_WaitDelay rather than a world timer because the task is cancelled with the
	// ability and a raw timer is not - a goblin who dies mid-wind-up must not still throw.
	if (GrappleWindupSeconds > 0.f)
	{
		UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(this, GrappleWindupSeconds);
		Wait->OnFinish.AddDynamic(this, &UGSGA_GrappleThrow::OnGrappleWindupFinished);
		Wait->ReadyForActivation();
	}
	else
	{
		OnGrappleWindupFinished();
	}
}

void UGSGA_GrappleThrow::OnGrappleWindupFinished()
{
	ThrowHook();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGSGA_GrappleThrow::ThrowHook()
{
	ACharacter* Avatar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Avatar || !Avatar->HasAuthority())
	{
		return;
	}

	// HookProjectileClass is a C++ default now (AGSGrappleHookProjectile::StaticClass(), set in the
	// constructor) rather than a soft path resolved here - see the header comment. Still checked:
	// an EditDefaultsOnly property can be cleared to None in a Blueprint child, and that must still
	// fail loudly rather than throw nothing (#048/#088).
	if (!HookProjectileClass)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[GS.Grapple] HookProjectileClass is None - the grapple slot will throw nothing. "
				 "This is the #048/#088 failure; check for an override that cleared it."));
		return;
	}

	// The muzzle comes from UGSAimComponent, which is also what drew the arc the player aimed with.
	// Duplicating the constants here is what #004 removed for the torch, and an aim preview that
	// disagrees with the spawn is an indicator that lies about where the hook lands.
	//
	// FindComponentByClass rather than a cast to AGSPlayerCharacter, matching the torch: nothing
	// about throwing a hook is player-only.
	FTransform Muzzle;
	if (const UGSAimComponent* AimComp = Avatar->FindComponentByClass<UGSAimComponent>())
	{
		Muzzle = AimComp->GetMuzzleTransform();
	}
	else
	{
		const FRotator AimRotation = Avatar->GetControlRotation();
		Muzzle = FTransform(AimRotation,
			Avatar->GetActorLocation() + AimRotation.Vector() * 80.f + FVector(0.f, 0.f, 50.f));
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Avatar;
	SpawnParams.Instigator = Avatar;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Avatar->GetWorld()->SpawnActor<AGSGrappleHookProjectile>(
		HookProjectileClass, Muzzle.GetLocation(), Muzzle.GetRotation().Rotator(), SpawnParams);
}

void UGSGA_GrappleThrow::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Teardown lives here and not on the success path, so a cancel, a death or a blocking tag
	// mid-wind-up cleans up too. Nothing is held in hand yet for the grapple, but the montage is
	// stopped for the same reason the torch un-readies its prop on every exit.
	if (bWasCancelled)
	{
		if (ACharacter* Avatar = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			if (ResolvedThrowMontage)
			{
				Avatar->StopAnimMontage(ResolvedThrowMontage);
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
