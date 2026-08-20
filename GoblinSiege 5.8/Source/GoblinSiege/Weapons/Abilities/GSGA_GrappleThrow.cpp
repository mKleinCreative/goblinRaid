#include "Weapons/Abilities/GSGA_GrappleThrow.h"
#include "Combat/GSAimComponent.h"
#include "Combat/GSGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"

// One warning per session for a broken reference, not one per goblin. This ability is
// InstancedPerActor, so a member latch would warn once per pawn - the same reasoning
// GSGA_TorchToss.cpp records for GThrowMontageResolveFailed.
static bool GGrappleHookResolveFailed = false;
static bool GGrappleMontageResolveFailed = false;

UGSGA_GrappleThrow::UGSGA_GrappleThrow()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// Soft C++ default. The prototype hook is a Blueprint, so this cannot be a StaticClass() the
	// way UGSGA_TorchToss defaults to AGSTorchProjectile - but the reason for defaulting it in code
	// at all is identical: #048 and #088 both shipped a verb whose class was never assigned on the
	// CDO, and both looked exactly like "the key does nothing".
	HookProjectileClassPath = TSoftClassPtr<AActor>(
		FSoftObjectPath(TEXT("/Game/Blueprints/Grapple/BP_GrappleHook.BP_GrappleHook_C")));

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

	if (!HookProjectileClass && !GGrappleHookResolveFailed)
	{
		if (UClass* Loaded = HookProjectileClassPath.IsNull() ? nullptr
			: HookProjectileClassPath.LoadSynchronous())
		{
			HookProjectileClass = Loaded;
		}
		else
		{
			GGrappleHookResolveFailed = true;
			UE_LOG(LogTemp, Error,
				TEXT("[GS.Grapple] HookProjectileClassPath failed to resolve - the grapple slot "
					 "will throw nothing. This is the #048/#088 failure; check the path."));
		}
	}
	if (!HookProjectileClass)
	{
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

	Avatar->GetWorld()->SpawnActor<AActor>(
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
