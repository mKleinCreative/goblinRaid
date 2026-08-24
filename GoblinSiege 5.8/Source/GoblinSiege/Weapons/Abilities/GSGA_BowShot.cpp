#include "Weapons/Abilities/GSGA_BowShot.h"

#include "Animation/AnimMontage.h"
#include "Combat/GSAimComponent.h"
#include "Combat/GSGameplayTags.h"
#include "Weapons/GSArrowProjectile.h"
#include "Weapons/GSBowTimingComponent.h"
#include "Weapons/GSWeaponComponent.h"
#include "Weapons/GSWeaponDataAsset.h"
#include "Components/ACFInventoryComponent.h"
#include "Items/ACFItem.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

UGSGA_BowShot::UGSGA_BowShot()
{
	AIReleaseMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(
		TEXT("/Game/Characters/Humans/Montages/AM_Bow_Release_Hum.AM_Bow_Release_Hum")));

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

float UGSGA_BowShot::GetFireIntervalSeconds(const FGameplayAbilityActorInfo* ActorInfo) const
{
	// FindComponentByClass rather than a cast to AGSPlayerCharacter, for the same reason FireArrow
	// does it: an AI archer is not a player character, and it should obey the same rate of fire.
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		if (const UGSWeaponComponent* WeaponComp =
				ActorInfo->AvatarActor->FindComponentByClass<UGSWeaponComponent>())
		{
			if (const UGSWeaponDataAsset* Weapon = WeaponComp->GetEquippedWeapon())
			{
				// A zero or negative value in the data asset is treated as "not configured" rather
				// than "no limit". Unlimited rate of fire is the bug this exists to fix, so it is not
				// something a blank field should be able to ask for by accident.
				if (Weapon->RangedAttackCooldownSeconds > 0.f)
				{
					return Weapon->RangedAttackCooldownSeconds;
				}
			}
		}
	}

	return FallbackFireIntervalSeconds;
}

TSubclassOf<UACFItem> UGSGA_BowShot::GetAmmoItemClass(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		return nullptr;
	}
	const AActor* Avatar = ActorInfo->AvatarActor.Get();

	// Test 1 - the player half. See the header for why this is component presence and not
	// IsPlayerControlled(). An AI archer has nothing to ask and pays nothing.
	if (!Avatar->FindComponentByClass<UGSBowTimingComponent>())
	{
		return nullptr;
	}

	// Test 2 - the bow half. A bow that names no ammo shoots for free.
	const UGSWeaponComponent* WeaponComp = Avatar->FindComponentByClass<UGSWeaponComponent>();
	const UGSWeaponDataAsset* Weapon = WeaponComp ? WeaponComp->GetEquippedWeapon() : nullptr;
	if (!Weapon || !Weapon->ArrowItemClass)
	{
		return nullptr;
	}

	// Both tests passed, so this pawn pays for its arrows. If it is ALSO not player-controlled,
	// somebody has given an AI archer a timing component and just handed it a quiver it has no way
	// to refill - it will stop shooting and no behaviour tree will report why. Say so, once.
	// Latch pattern copied from UGSGA_TorchToss's montage-resolve warning.
	if (!Cast<APlayerController>(Avatar->GetInstigatorController()))
	{
		static bool bWarnedAIWithQuiver = false;
		if (!bWarnedAIWithQuiver)
		{
			bWarnedAIWithQuiver = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[GoblinSiege] '%s' is not player-controlled but has a UGSBowTimingComponent and "
					 "a bow with ArrowItemClass set, so it now SPENDS arrows it cannot pick up and "
					 "will stop shooting when they run out. Ruling 48 says AI archers do not run dry: "
					 "clear ArrowItemClass on its weapon, or take the timing component off it."),
				*GetNameSafe(Avatar));
		}
	}

	return Weapon->ArrowItemClass;
}

bool UGSGA_BowShot::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// The rate limit lives here rather than in ActivateAbility so a too-early click costs nothing at
	// all: no CommitAbility, no ability instance, no wind-up task started and immediately cancelled.
	// The press simply is not a shot, which is also what makes held-button autofire behave - the
	// character re-tries activation and is refused until the interval has passed.
	if (LastFireTimeSeconds >= 0.f && ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		if (const UWorld* World = ActorInfo->AvatarActor->GetWorld())
		{
			if (World->GetTimeSeconds() - LastFireTimeSeconds < GetFireIntervalSeconds(ActorInfo))
			{
				return false;
			}
		}
	}

	// Out of arrows (ruling 46). Here rather than in ActivateAbility for the same reason the rate
	// limit is: the press simply is not a shot. No CommitAbility, no ability instance, no wind-up
	// task started and immediately cancelled, and held-fire stops firing instead of stuttering.
	if (const TSubclassOf<UACFItem> AmmoClass = GetAmmoItemClass(ActorInfo))
	{
		const UACFInventoryComponent* Inventory =
			ActorInfo->AvatarActor->FindComponentByClass<UACFInventoryComponent>();
		if (!Inventory || Inventory->GetTotalCountOfItemsByClass(AmmoClass) <= 0)
		{
			UE_LOG(LogTemp, Verbose,
				TEXT("[GoblinSiege] %s cannot shoot - no arrows left."),
				*GetNameSafe(ActorInfo->AvatarActor.Get()));
			return false;
		}
	}

	return true;
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

	// Stamped at activation, not at FireArrow, so the interval measures shot-to-shot rather than
	// arrow-to-press. Otherwise ReleaseDelaySeconds would quietly be added to every gap and the
	// number in the data asset would not be the number the player feels.
	if (const UWorld* World = GetWorld())
	{
		LastFireTimeSeconds = World->GetTimeSeconds();
	}

	// Erika's recoil. Gated on the component's ABSENCE so the player, whose animation the timing
	// component already drives, is never double-driven into a second montage on top of its own.
	if (ACharacter* Avatar = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (!Avatar->FindComponentByClass<UGSBowTimingComponent>())
		{
			if (UAnimMontage* Recoil = AIReleaseMontage.LoadSynchronous())
			{
				Avatar->PlayAnimMontage(Recoil);
			}
		}
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

	AGSArrowProjectile* Arrow = Avatar->GetWorld()->SpawnActor<AGSArrowProjectile>(
		ArrowProjectileClass, Muzzle.GetLocation(), Muzzle.GetRotation().Rotator(), SpawnParams);

	// THE ONE LINE THAT MAKES THE MINIGAME PLAYER-ONLY.
	//
	// This ability is shared - BP_ErikaArcher's RangedAttackAbilityClass and the player's
	// BowShotAbilityClass are both this class, with no Blueprint child between them. Asking the
	// AVATAR for a timing component rather than branching on IsPlayerControlled() means an AI archer
	// simply has nothing to ask, and its arrow keeps the default multiplier of 1.0. There is no
	// condition here to get wrong later.
	//
	// ConsumeReleaseQuality was already called at input release, so this reads a value the player
	// judged rather than sampling 80ms of release recoil later; see the component's header.
	if (Arrow)
	{
		if (const UGSBowTimingComponent* Timing = Avatar->FindComponentByClass<UGSBowTimingComponent>())
		{
			Arrow->SetDrawQualityMultiplier(Timing->GetLastReleaseQuality());
			Arrow->SetLaunchSpeedScale(Timing->GetLastReleaseSpeedScale());
		}

		// SPEND THE ARROW LAST, AND THAT ORDER IS NOT COSMETIC (ruling 46).
		//
		// After the spawn, so a spawn that failed cannot eat an arrow. And after the draw-quality
		// read above, because a consume placed earlier with an early return on failure would skip
		// that block and silently give EVERY player arrow the default 1.0 multiplier - the timing
		// minigame would become decoration and nothing would report it.
		//
		// Inside FireArrow's existing HasAuthority() guard: ConsumeItems is a Server RPC, so from a
		// client it would marshal and do nothing locally. The client's CanActivateAbility reads a
		// replicated count that can be one shot stale; the server is the real gate, and closing
		// that window by consuming client-side would break authority.
		if (const TSubclassOf<UACFItem> AmmoClass = GetAmmoItemClass(CurrentActorInfo))
		{
			if (UACFInventoryComponent* Inventory = Avatar->FindComponentByClass<UACFInventoryComponent>())
			{
				Inventory->ConsumeItems({ FBaseItem(AmmoClass, 1) });
			}
		}
	}
}
