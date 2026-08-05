#include "Weapons/Abilities/GSGA_SwordLight.h"
#include "Combat/GSGE_WeaponDamage.h"
#include "Combat/GSGameplayTags.h"
#include "Characters/GSCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
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

	// Hands full, or hands busy mid-channel (GDD §8, ruling 2026-08-04).
	ActivationBlockedTags.AddTag(GSTags::State_Carrying);
	ActivationBlockedTags.AddTag(GSTags::State_Interacting);

	// One default stage so a freshly-made Blueprint child swings before anyone fills the array in.
	Stages.Add(FGSSwingStage());
}

const FGSSwingStage& UGSGA_SwordLight::GetStage() const
{
	static const FGSSwingStage Fallback;
	return Stages.IsValidIndex(CurrentStage) ? Stages[CurrentStage] : Fallback;
}

void UGSGA_SwordLight::ApplyMoveSpeedScale(float Scale)
{
	ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UCharacterMovementComponent* Move = Char ? Char->GetCharacterMovement() : nullptr;
	if (!Move)
	{
		return;
	}

	// Cache ONCE per ability activation, not once per stage. Re-caching on stage 2 would capture
	// the already-scaled speed and multiply it again, so a three-hit chain would end at
	// 0.55^3 = 17% speed and the restore would put back a wrong value.
	if (CachedMaxWalkSpeed <= 0.f)
	{
		CachedMaxWalkSpeed = Move->MaxWalkSpeed;
	}

	Move->MaxWalkSpeed = CachedMaxWalkSpeed * FMath::Max(Scale, 0.f);
}

void UGSGA_SwordLight::RestoreMoveSpeed()
{
	if (CachedMaxWalkSpeed <= 0.f)
	{
		return;
	}

	if (ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
		{
			Move->MaxWalkSpeed = CachedMaxWalkSpeed;
		}
	}
	CachedMaxWalkSpeed = 0.f;
}

void UGSGA_SwordLight::ApplyLunge(const FGSSwingStage& S)
{
	if (S.LungeSpeed <= 0.f)
	{
		return;
	}

	ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Char)
	{
		return;
	}

	// XY only: a lunge that touches Z would launch a kick off a slope or cancel a fall. Override
	// rather than add, so the shove reads the same whether the character was standing still or
	// already running in - otherwise a sprinting attacker gets a much bigger lunge than a
	// stationary one from the identical input.
	Char->LaunchCharacter(Char->GetActorForwardVector() * S.LungeSpeed, true, false);
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

	// Committing to the swing costs mobility but never removes it. Re-applied per stage so a
	// combo can accelerate or plant harder as it chains.
	ApplyMoveSpeedScale(S.MoveSpeedScale);

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

	// The step and the strike start on the same frame, so the lunge reads as part of the attack
	// rather than a separate hop into it.
	ApplyLunge(GetStage());

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

		// Same race, no hit. Without this a patrol of militia crowding a doorway cuts itself down -
		// two swings kills at 30 HP, and placed defenders were quietly deleting each other before
		// the player ever arrived (observed 2026-08-04 in L_Tutorial_Island).
		if (const AGSCharacterBase* SelfChar = Cast<AGSCharacterBase>(Avatar))
		{
			if (!SelfChar->IsHostileTo(Target))
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

		// A guard break has to be resolved BEFORE the damage is applied, because dropping the
		// target's guard is what decides whether UGSDamageExecCalculation mitigates this same hit.
		// Break first and the kick lands on an open target; break after and the kick is politely
		// blocked by the guard it just destroyed.
		const bool bBrokeGuard = S.bBreaksGuard && BreakGuard(Target, TargetASC, S);

		// The Damage.* tag is BOTH the type marker and the SetByCaller key, and it must go on the
		// SPEC - the same tag sitting in a Blueprint effect's asset tags is invisible to
		// UGSDamageExecCalculation and silently deals zero. The sword rides Damage.Dagger because
		// no Damage.Sword tag exists yet; logged in the decision queue.
		SpecHandle.Data->AddDynamicAssetTag(GSTags::Damage_Dagger);
		SpecHandle.Data->SetSetByCallerMagnitude(GSTags::Damage_Dagger,
			bBrokeGuard ? S.Damage * S.GuardBreakDamageScale : S.Damage);

		SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);

		if (bShowDebug)
		{
			DrawDebugSphere(World, Target->GetActorLocation() + FVector(0, 0, 60.f), 45.f, 12,
				FColor::Red, false, 0.6f, 0, 3.f);
		}
	}
}

bool UGSGA_SwordLight::BreakGuard(AActor* Target, UAbilitySystemComponent* TargetASC,
	const FGSSwingStage& S)
{
	if (!Target || !TargetASC || !TargetASC->HasMatchingGameplayTag(GSTags::State_Blocking))
	{
		// Nothing to break. A guard break against an unguarded target is just a weak hit, which is
		// the correct answer - it should not become a universal stagger tool.
		return false;
	}

	// Cancel by TAG, never by class. The blocker may be a player running UGSGA_Block or a human
	// running something else entirely, and CancelAbilities(nullptr) would take their swing, their
	// dodge and everything else with it.
	FGameplayTagContainer CancelTags;
	CancelTags.AddTag(GSTags::State_Blocking);

	// A broken guard rips open a hold-E channel too: ActivationBlockedTags only refuses a START, so
	// without this a goblin who was already looting would loot straight through the stagger.
	CancelTags.AddTag(GSTags::State_Interacting);
	TargetASC->CancelAbilities(&CancelTags);

	if (S.GuardBreakStaggerSeconds > 0.f)
	{
		TargetASC->AddLooseGameplayTag(GSTags::State_GuardBroken);

		// Weak lambda against the TARGET's ASC, not this ability: the swing that broke the guard
		// will have ended long before the stagger expires, and a timer owned by a dead ability
		// instance would leave the victim permanently unable to block.
		TWeakObjectPtr<UAbilitySystemComponent> WeakASC(TargetASC);
		FTimerHandle Handle;
		Target->GetWorldTimerManager().SetTimer(Handle,
			FTimerDelegate::CreateWeakLambda(TargetASC, [WeakASC]()
			{
				if (UAbilitySystemComponent* ASC = WeakASC.Get())
				{
					ASC->RemoveLooseGameplayTag(GSTags::State_GuardBroken);
				}
			}),
			S.GuardBreakStaggerSeconds, false);
	}

	// Make the break legible. Without a distinct reaction the victim's guard silently evaporates
	// and the attacker has no idea the kick did anything.
	if (AGSCharacterBase* TargetCharacter = Cast<AGSCharacterBase>(Target))
	{
		TargetCharacter->PlayHitReact(GetAvatarActorFromActorInfo()
			? GetAvatarActorFromActorInfo()->GetActorLocation() - Target->GetActorLocation()
			: FVector::ZeroVector);
	}

	return true;
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

	// The blade is coming back: ease the slow off for the tail so the character can start
	// repositioning before the ability actually ends.
	ApplyMoveSpeedScale(GetStage().RecoveryMoveSpeedScale);

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
	// timer would keep dealing damage from a corpse, and a leaked speed scale would leave the
	// character permanently slowed after a swing that got interrupted.
	ClearAllTimers();
	RestoreMoveSpeed();

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
