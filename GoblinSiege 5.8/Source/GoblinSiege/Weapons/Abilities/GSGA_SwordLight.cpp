#include "Weapons/Abilities/GSGA_SwordLight.h"
#include "Combat/GSGE_WeaponDamage.h"
#include "Combat/GSGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "Animation/AnimMontage.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

UGSGA_SwordLight::UGSGA_SwordLight()
{
	// InstancedPerActor because this ability owns per-swing state (HitActorsThisSwing and four
	// timer handles). A NonInstanced ability would share that state across every goblin swinging.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	DamageEffectClass = UGSGE_WeaponDamage::StaticClass();

	// A corpse does not swing, and the dodge roll owns its own recovery (design doc §7).
	ActivationBlockedTags.AddTag(GSTags::State_Dead);
	ActivationBlockedTags.AddTag(GSTags::State_Dodging);
}

void UGSGA_SwordLight::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	HitActorsThisSwing.Reset();

	UWorld* World = GetWorld();
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!World || !Avatar)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Animation is optional on purpose - see the header. A missing montage must never stop the
	// swing from being testable.
	if (AttackMontage)
	{
		if (ACharacter* Char = Cast<ACharacter>(Avatar))
		{
			Char->PlayAnimMontage(AttackMontage, MontagePlayRate);
		}
	}

	if (WindupSeconds > 0.f)
	{
		World->GetTimerManager().SetTimer(WindupTimer, this, &UGSGA_SwordLight::OpenDamageWindow, WindupSeconds, false);
	}
	else
	{
		OpenDamageWindow();
	}
}

void UGSGA_SwordLight::OpenDamageWindow()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Sweep immediately, then on an interval. Without the immediate one, a window shorter than the
	// interval would never fire at all - which is exactly the kind of thing you would "fix" by
	// tuning damage numbers for an hour.
	DoSweep();
	World->GetTimerManager().SetTimer(SweepTimer, this, &UGSGA_SwordLight::DoSweep, SweepIntervalSeconds, true);
	World->GetTimerManager().SetTimer(WindowTimer, this, &UGSGA_SwordLight::CloseDamageWindow, DamageWindowSeconds, false);
}

void UGSGA_SwordLight::DoSweep()
{
	UWorld* World = GetWorld();
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!World || !Avatar)
	{
		return;
	}

	const FVector Forward = Avatar->GetActorForwardVector();
	const FVector Origin = Avatar->GetActorLocation()
		+ Forward * SweepForwardOffset
		+ FVector(0.f, 0.f, SweepHeightOffset - Avatar->GetSimpleCollisionHalfHeight());

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSSwordLight), false, Avatar);
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);

	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjParams,
		FCollisionShape::MakeSphere(SweepRadius), Params);

	if (bDrawDebugSweep)
	{
		DrawDebugSphere(World, Origin, SweepRadius, 16, FColor::Cyan, false, 0.35f, 0, 1.5f);
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	const float CosHalfArc = FMath::Cos(FMath::DegreesToRadians(SweepArcDegrees * 0.5f));

	for (const FOverlapResult& O : Overlaps)
	{
		AActor* Target = O.GetActor();
		if (!Target || Target == Avatar || HitActorsThisSwing.Contains(Target))
		{
			continue;
		}

		// Arc filter: a sphere centred in front still reaches slightly behind the shoulders, and
		// being hit by a swing aimed away from you reads as a bug even when the maths is fine.
		if (SweepArcDegrees < 360.f)
		{
			FVector ToTarget = Target->GetActorLocation() - Avatar->GetActorLocation();
			ToTarget.Z = 0.f;
			if (!ToTarget.IsNearlyZero() && FVector::DotProduct(ToTarget.GetSafeNormal(), Forward) < CosHalfArc)
			{
				continue;
			}
		}

		UAbilitySystemComponent* TargetASC =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		if (!TargetASC || !SourceASC)
		{
			continue; // props and non-GAS pawns simply aren't damageable
		}

		if (TargetASC->HasMatchingGameplayTag(GSTags::State_Dead)
			|| TargetASC->HasMatchingGameplayTag(GSTags::State_Invulnerable))
		{
			continue;
		}

		HitActorsThisSwing.Add(Target);

		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddInstigator(Avatar, Avatar);
		Context.AddSourceObject(this);

		const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.f, Context);
		if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
		{
			continue;
		}

		// The Damage.* tag is BOTH the type marker and the SetByCaller key (see
		// UGSDamageExecCalculation). It must go on the SPEC - a Damage.* tag sitting in a
		// Blueprint effect's asset tags is invisible to the exec calc and silently deals zero.
		// There is no Damage.Sword tag; the sword rides Damage.Dagger until the matchup tables
		// are re-cut. Logged in the decision queue so a "Dagger" row driving a sword is not a
		// mystery in three weeks.
		SpecHandle.Data->AddDynamicAssetTag(GSTags::Damage_Dagger);
		SpecHandle.Data->SetSetByCallerMagnitude(GSTags::Damage_Dagger, Damage);

		SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);

		if (bDrawDebugSweep)
		{
			DrawDebugSphere(World, Target->GetActorLocation() + FVector(0, 0, 60.f), 45.f, 12,
				FColor::Red, false, 0.6f, 0, 3.f);
		}
	}
}

void UGSGA_SwordLight::CloseDamageWindow()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SweepTimer);

		if (RecoverySeconds > 0.f)
		{
			World->GetTimerManager().SetTimer(RecoveryTimer, this, &UGSGA_SwordLight::FinishRecovery, RecoverySeconds, false);
			return;
		}
	}
	FinishRecovery();
}

void UGSGA_SwordLight::FinishRecovery()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGSGA_SwordLight::ClearAllTimers()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TM = World->GetTimerManager();
		TM.ClearTimer(WindupTimer);
		TM.ClearTimer(SweepTimer);
		TM.ClearTimer(WindowTimer);
		TM.ClearTimer(RecoveryTimer);
	}
}

void UGSGA_SwordLight::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Every path out of this ability comes through here, including cancellation by death or a
	// dodge. A leaked sweep timer would keep dealing damage from a corpse.
	ClearAllTimers();

	if (bWasCancelled && AttackMontage)
	{
		if (ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			Char->StopAnimMontage(AttackMontage);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
