#include "Weapons/Abilities/GSGA_SwordLight.h"
#include "Combat/GSGE_WeaponDamage.h"
#include "Combat/GSGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "Animation/AnimMontage.h"
#include "Engine/OverlapResult.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

// Master switch for combat debug drawing. The per-ability bDrawDebugSweep stays as the "does this
// ability draw at all" opt-in; this is the global override, so a playtest can be made clean from
// the console without editing a Blueprint and without a rebuild.
//   GS.Combat.Debug 0   -> player view
//   GS.Combat.Debug 1   -> trace spheres, hit markers, combo stage readout
static int32 GSCombatDebug = 1;
static FAutoConsoleVariableRef CVarGSCombatDebug(
	TEXT("GS.Combat.Debug"),
	GSCombatDebug,
	TEXT("0 = hide all combat debug drawing (trace spheres, hit markers, combo stage text). 1 = show."),
	ECVF_Cheat);

bool GSCombatDebugEnabled()
{
	return GSCombatDebug > 0;
}

UGSGA_SwordLight::UGSGA_SwordLight()
{
	// InstancedPerActor: this ability owns per-chain state (stage index, buffer flag, hit set,
	// four timers). NonInstanced would share all of that across every goblin swinging at once.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	DamageEffectClass = UGSGE_WeaponDamage::StaticClass();

	ActivationBlockedTags.AddTag(GSTags::State_Dead);
	ActivationBlockedTags.AddTag(GSTags::State_Dodging);

	// One default stage so a freshly-made Blueprint child swings before anyone fills the array in.
	Stages.Add(FGSSwingStage());
}

const FGSSwingStage& UGSGA_SwordLight::GetStage() const
{
	static const FGSSwingStage Fallback;
	return Stages.IsValidIndex(CurrentStage) ? Stages[CurrentStage] : Fallback;
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

	if (Stages.Num() == 0 || !GetWorld() || !GetAvatarActorFromActorInfo())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CurrentStage = 0;
	bComboQueued = false;
	RunStage();
}

void UGSGA_SwordLight::RunStage()
{
	UWorld* World = GetWorld();
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!World || !Avatar)
	{
		FinishRecovery();
		return;
	}

	// Per-stage, not per-ability: a three-hit chain should land three times on the same target.
	HitActorsThisSwing.Reset();
	bComboQueued = false;
	bBufferOpen = !bBufferOpensAtDamageWindow;

	const FGSSwingStage& S = GetStage();

	if (S.Montage)
	{
		if (ACharacter* Char = Cast<ACharacter>(Avatar))
		{
			Char->PlayAnimMontage(S.Montage, S.MontagePlayRate);
		}
	}

	if (S.WindupSeconds > 0.f)
	{
		World->GetTimerManager().SetTimer(WindupTimer, this, &UGSGA_SwordLight::OpenDamageWindow, S.WindupSeconds, false);
	}
	else
	{
		OpenDamageWindow();
	}
}

bool UGSGA_SwordLight::BufferComboInput()
{
	if (!bBufferOpen || !Stages.IsValidIndex(CurrentStage + 1))
	{
		return false;
	}
	bComboQueued = true;
	return true;
}

void UGSGA_SwordLight::OpenDamageWindow()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bBufferOpen = true;

	// Sweep immediately AND on an interval. Without the immediate one, any window shorter than
	// the sweep interval would never fire - a bug you would misdiagnose as bad damage numbers.
	DoSweep();
	World->GetTimerManager().SetTimer(SweepTimer, this, &UGSGA_SwordLight::DoSweep, SweepIntervalSeconds, true);
	World->GetTimerManager().SetTimer(WindowTimer, this, &UGSGA_SwordLight::CloseDamageWindow,
		GetStage().DamageWindowSeconds, false);
}

void UGSGA_SwordLight::DoSweep()
{
	UWorld* World = GetWorld();
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!World || !Avatar)
	{
		return;
	}

	const FGSSwingStage& S = GetStage();
	const FVector Forward = Avatar->GetActorForwardVector();
	const FVector Origin = Avatar->GetActorLocation()
		+ Forward * S.SweepForwardOffset
		+ FVector(0.f, 0.f, S.SweepHeightOffset - Avatar->GetSimpleCollisionHalfHeight());

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSSwordLight), false, Avatar);
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);

	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjParams,
		FCollisionShape::MakeSphere(S.SweepRadius), Params);

	const bool bShowDebug = bDrawDebugSweep && GSCombatDebugEnabled();

	if (bShowDebug)
	{
		// Colour by stage so a chain is legible on screen without reading a log.
		static const FColor StageColours[] = { FColor::Cyan, FColor::Green, FColor::Magenta };
		DrawDebugSphere(World, Origin, S.SweepRadius, 16,
			StageColours[CurrentStage % 3], false, 0.35f, 0, 1.5f);
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	const float CosHalfArc = FMath::Cos(FMath::DegreesToRadians(S.SweepArcDegrees * 0.5f));

	for (const FOverlapResult& O : Overlaps)
	{
		AActor* Target = O.GetActor();
		if (!Target || Target == Avatar || HitActorsThisSwing.Contains(Target))
		{
			continue;
		}

		// A sphere centred in front still reaches slightly behind the shoulders. Being hit by a
		// swing aimed away from you reads as a bug even when the maths is right.
		if (S.SweepArcDegrees < 360.f)
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
			continue;
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

		// The Damage.* tag is BOTH the type marker and the SetByCaller key, and it must go on the
		// SPEC - the same tag sitting in a Blueprint effect's asset tags is invisible to
		// UGSDamageExecCalculation and silently deals zero. The sword rides Damage.Dagger because
		// no Damage.Sword tag exists yet; logged in the decision queue.
		SpecHandle.Data->AddDynamicAssetTag(GSTags::Damage_Dagger);
		SpecHandle.Data->SetSetByCallerMagnitude(GSTags::Damage_Dagger, S.Damage);

		SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);

		if (bShowDebug)
		{
			DrawDebugSphere(World, Target->GetActorLocation() + FVector(0, 0, 60.f), 45.f, 12,
				FColor::Red, false, 0.6f, 0, 3.f);
		}
	}
}

void UGSGA_SwordLight::CloseDamageWindow()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		FinishRecovery();
		return;
	}

	World->GetTimerManager().ClearTimer(SweepTimer);

	const float Recovery = GetStage().RecoverySeconds;
	if (Recovery > 0.f)
	{
		World->GetTimerManager().SetTimer(RecoveryTimer, this, &UGSGA_SwordLight::FinishRecovery, Recovery, false);
		return;
	}
	FinishRecovery();
}

void UGSGA_SwordLight::FinishRecovery()
{
	// The chain advances here and nowhere else. Buffering during the swing only sets a flag;
	// this is the single point that decides whether the flag becomes another swing. That is what
	// keeps "am I mid-combo" from being answerable in two places that can disagree.
	if (bComboQueued && Stages.IsValidIndex(CurrentStage + 1))
	{
		++CurrentStage;
		if (bDrawDebugSweep && GSCombatDebugEnabled() && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow,
				FString::Printf(TEXT("[combo] stage %d/%d"), CurrentStage + 1, Stages.Num()));
		}
		RunStage();
		return;
	}

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
	// Every exit runs through here, including cancellation by death or a dodge. A leaked sweep
	// timer would keep dealing damage from a corpse.
	ClearAllTimers();

	if (bWasCancelled)
	{
		if (ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			if (const UAnimMontage* M = GetStage().Montage)
			{
				Char->StopAnimMontage(const_cast<UAnimMontage*>(M));
			}
		}
	}

	CurrentStage = 0;
	bComboQueued = false;
	bBufferOpen = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
