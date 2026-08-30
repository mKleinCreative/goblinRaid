#include "Weapons/Abilities/GSGA_SwordLight.h"
#include "Combat/GSGE_WeaponDamage.h"
#include "Combat/GSEngagementComponent.h"
#include "Raid/GSRaidLibrary.h"
#include "Combat/GSGameplayTags.h"
#include "Characters/GSCharacterBase.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/OverlapResult.h"
#include "Components/ACFDamageHandlerComponent.h"
// CollisionsManager reaches us transitively: it is a PUBLIC dependency of AscentCombatFramework,
// which AIFramework carries, so no Build.cs change is needed for this include.
#include "ACMCollisionsFunctionLibrary.h"
#include "Combat/GSDamageTypes.h"
#include "Combat/GSHitCameraShake.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/DamageEvents.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

// ---- THE TRANSPORT SWITCH (#237) -------------------------------------------------------------
//
// 1 routes the swing through UACFDamageHandlerComponent::TakeDamage, which is what makes ACF's
// ragdoll, hit-response actions and defense stance work - all three read a damage EVENT that only
// ACF's own pipeline produces. 0 restores the gameplay-effect path this project has always used.
//
// A cvar rather than a straight replacement because this changes how every hit resolves and the
// person who can see it is not the person who wrote it: if damage misbehaves mid-playtest, this
// flips back without a rebuild. DELETE IT once the ACF path has been watched and trusted - a
// permanent branch through the damage path is two combat systems to keep in step.
// DEFAULT 0 as of the first watched attempt. The ACF path computed damage correctly and applied
// none of it, so combat ran on the old path anyway once Michael flipped this. It stays off until the
// out-array fix above has been watched - working combat is the baseline, not the experiment.
static int32 GSUseACFDamage = 0;
static FAutoConsoleVariableRef CVarGSUseACFDamage(
	TEXT("GS.Combat.ACFDamage"),
	GSUseACFDamage,
	TEXT("1 = the axe swing delivers via ACF's damage handler (ragdoll, hit responses, defense "
		 "stance). 0 = the original gameplay-effect path."),
	ECVF_Default);


// Master switch for combat debug drawing. The per-ability bDrawDebugSweep stays as the "does this
// ability draw at all" opt-in; this is the global override, so a playtest can be made clean from
// the console without editing a Blueprint and without a rebuild.
//   GS.Combat.Debug 0   -> player view
//   GS.Combat.Debug 1   -> trace spheres, hit markers, combo stage readout
//
// DEFAULTS TO 0 (#120). It shipped as 1, which made this the only debug cvar in the project that
// was on unless someone remembered to turn it off - so every session, for every combatant, drew
// cyan trace spheres and red hit markers over the game. Michael's CombatBugs recording of 2026-08-09
// is full of them; that recording was made to show an animation bug, and the debug geometry was in
// every frame of it. A debug view you have to remember to disable is one you ship by accident.
// Turn it on with `GS.Combat.Debug 1`, or use `GS.PlayerView` to sweep every channel at once.
static int32 GSCombatDebug = 0;
static FAutoConsoleVariableRef CVarGSCombatDebug(
	TEXT("GS.Combat.Debug"),
	GSCombatDebug,
	TEXT("0 = hide all combat debug drawing (trace spheres, hit markers, combo stage text). 1 = show."),
	ECVF_Cheat);

bool GSCombatDebugEnabled()
{
	return GSCombatDebug > 0;
}

// THE TELEGRAPH. Set to exactly 0 or 1 rather than Add/RemoveLooseGameplayTag, because those are
// reference-counted and this window has three exits (the damage window opening, the ability ending
// normally, and the ability being cancelled mid-windup by a death or a dodge). A refcounted pair
// that gets removed twice on one of those paths leaves the count negative and the NEXT swing's
// telegraph invisible - a bug that would present as "the AI blocks sometimes" and cost an evening.
// Idempotent by construction is worth more here than symmetry.
static void GSSetWindupTelegraph(UAbilitySystemComponent* ASC, bool bWindingUp)
{
	if (ASC)
	{
		ASC->SetLooseGameplayTagCount(GSTags::State_Attacking_Windup, bWindingUp ? 1 : 0);
	}
}

// Lock/unlock this attacker's grant on every victim it currently holds one from. The attacker does
// not track who it reserved against - the ledger lives on the victim - so this asks the character
// it is about to hit. In practice that is the one target it is swinging at; sweeping all of them
// would need a registry for no benefit at this scale.
static void GSSetTokenLocked(AActor* Avatar, bool bLocked)
{
	const AGSCharacterBase* Attacker = Cast<AGSCharacterBase>(Avatar);
	if (!Attacker)
	{
		return;
	}

	// Ask the AI what it is attacking. A player swing locks nothing, which is correct: the player
	// never holds a token (nothing rations HIM), he is rationed by what may attack him.
	const AController* Controller = Attacker->GetController();
	const AAIController* AICon = Cast<AAIController>(Controller);
	const UBlackboardComponent* BB = AICon ? AICon->GetBlackboardComponent() : nullptr;
	if (!BB)
	{
		return;
	}

	AActor* Target = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
	if (!IsValid(Target))
	{
		return;
	}

	if (UGSEngagementComponent* Engagement = Target->FindComponentByClass<UGSEngagementComponent>())
	{
		Engagement->SetTokenLocked(const_cast<AGSCharacterBase*>(Attacker), bLocked);
	}
}

UGSGA_SwordLight::UGSGA_SwordLight()
{
	// InstancedPerActor: this ability owns per-chain state (stage index, buffer flag, hit set,
	// four timers). NonInstanced would share all of that across every goblin swinging at once.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	DamageEffectClass = UGSGE_WeaponDamage::StaticClass();

	// "This character is mid-swing", held by GAS for exactly the ability's lifetime. Coarse on
	// purpose - it spans recovery and the whole combo chain, so it answers "is he busy" rather than
	// "is a hit coming". UBTTask_Block reads it to avoid raising a guard in the middle of its own
	// swing; the Windup child below is what a defender actually reacts to.
	ActivationOwnedTags.AddTag(GSTags::State_Attacking);

	// The asset tag ACF's combat behaviour addresses this ability BY (#353). The UACFAbilitySet
	// resolves Actions.Defender.Melee to this class at grant time; #343 reused State.Attacking for
	// that job as a stopgap because it was the only registered tag already here. Both tags are kept
	// on the CDO - State.Attacking still describes what the character is doing and other code reads
	// it; this one is purely the AI's handle on the verb. SetAssetTags, not the deprecated
	// AbilityTags member (C4996 in 5.8).
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(GSTags::State_Attacking);
	AssetTags.AddTag(GSTags::Actions_Defender_Melee);
	SetAssetTags(AssetTags);

	ActivationBlockedTags.AddTag(GSTags::State_Dead);
	ActivationBlockedTags.AddTag(GSTags::State_Dodging);

	// Hands full, or hands busy mid-channel (GDD §8, ruling 2026-08-04).
	ActivationBlockedTags.AddTag(GSTags::State_Carrying);
	ActivationBlockedTags.AddTag(GSTags::State_Interacting);

	// Your last swing was turned aside. Half of the punish window is this refusal - the other half
	// is UGSGA_Block refusing too, so a blocked attacker can neither swing again nor hide.
	ActivationBlockedTags.AddTag(GSTags::State_Recoil);

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

	// ---- PICK THE ANIMATION, THEN MAKE IT FIT (#349) ------------------------------------------
	//
	// Variety without a combo: one attack that does not look identical every time. The primary
	// Montage and every entry in MontageVariants are equally likely, so leaving the array empty
	// keeps the old single-animation behaviour exactly.
	// Decide this BEFORE the montage starts: once a root-motion montage is playing it owns the
	// character's movement, and in the air that means gravity stops.
	SuppressRootMotionIfAirborne();

	if (UAnimMontage* Chosen = PickStageMontage(S))
	{
		if (ACharacter* Char = Cast<ACharacter>(Avatar))
		{
			float Rate = S.MontagePlayRate;

			// Fit the animation to the stage rather than letting the stage cut the animation off.
			// AM_HU_Atk_Light is 1.32s against a stage that ran 0.90s, so a third of the swing was
			// never rendered - and with variants of different lengths (A1 1.32s, C1 1.50s) no single
			// fixed rate can fit both. Deriving it per swing is the only way both finish cleanly.
			const float StageSeconds = S.WindupSeconds + S.DamageWindowSeconds + S.RecoverySeconds;
			const float Len = Chosen->GetPlayLength();
			if (S.bFitMontageToStage && StageSeconds > KINDA_SMALL_NUMBER && Len > KINDA_SMALL_NUMBER)
			{
				Rate = Len / StageSeconds;
			}

			// PlayAnimMontage returns 0 on a skeleton mismatch and says nothing about it. The swing
			// is timer-driven, so a refused montage still deals full damage - an invisible hit that
			// nothing downstream can notice. Say so instead.
			const float Played = Char->PlayAnimMontage(Chosen, Rate);
			if (Played <= 0.f)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[GS.Combat] '%s' refused to play on %s (skeleton mismatch?) - the swing "
						 "will still hit, invisibly."),
					*Chosen->GetName(), *GetNameSafe(Avatar));
			}
			else if (GSCombatDebugEnabled())
			{
				UE_LOG(LogTemp, Log,
					TEXT("[GS.Combat] %s swings '%s' (%.2fs) at rate %.2f to fill a %.2fs stage"),
					*GetNameSafe(Avatar), *Chosen->GetName(), Len, Rate, StageSeconds);
			}
		}
	}

	if (S.WindupSeconds > 0.f)
	{
		// The swing is now committed and the pose has started. Raise the telegraph for exactly the
		// windup: this is the frame the player sees the arm go back, so it is the frame a defender
		// is allowed to know about. Cleared in OpenDamageWindow - see GSSetWindupTelegraph.
		GSSetWindupTelegraph(GetAbilitySystemComponentFromActorInfo(), true);

		// Lock the attack token for the committed frames. An on-screen attacker may preempt an
		// off-screen one, but never one that is already swinging - cancelling an animation the
		// player is watching to save someone else a wait looks far worse than the wait.
		GSSetTokenLocked(Avatar, true);

		World->GetTimerManager().SetTimer(WindupTimer, this, &UGSGA_SwordLight::OpenDamageWindow, S.WindupSeconds, false);
	}
	else
	{
		// A zero-windup stage is unreactable by design (the guard break is the intended user). No
		// telegraph is raised at all rather than one raised and cleared in the same frame, which
		// nothing sampling at any rate could observe anyway.
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

	// The windup is over - the blade is live. Anyone who has not decided to block by now is late,
	// which is exactly the property that makes a short windup unblockable without a probability roll.
	GSSetWindupTelegraph(GetAbilitySystemComponentFromActorInfo(), false);

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

void UGSGA_SwordLight::SuppressRootMotionIfAirborne()
{
	ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Char || bRootMotionSuppressed)
	{
		return;
	}

	const UCharacterMovementComponent* Move = Char->GetCharacterMovement();
	if (!Move || !Move->IsFalling())
	{
		return;   // grounded: keep the authored root motion, it is the lunge
	}

	USkeletalMeshComponent* Mesh = Char->GetMesh();
	UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		return;
	}

	CachedRootMotionMode = Anim->RootMotionMode;
	Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	bRootMotionSuppressed = true;

	if (GSCombatDebugEnabled())
	{
		UE_LOG(LogTemp, Log,
			TEXT("[GS.Combat] %s swings while airborne - root motion ignored so gravity keeps working"),
			*GetNameSafe(Char));
	}
}

void UGSGA_SwordLight::RestoreRootMotionMode()
{
	if (!bRootMotionSuppressed)
	{
		return;
	}
	bRootMotionSuppressed = false;

	ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : nullptr;
	if (UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr)
	{
		Anim->SetRootMotionMode(CachedRootMotionMode);
	}
}

UAnimMontage* UGSGA_SwordLight::PickStageMontage(const FGSSwingStage& S) const
{
	// The primary Montage is one of the candidates, not a fallback - otherwise adding a single
	// variant would silently halve how often the original animation is seen.
	TArray<UAnimMontage*> Candidates;
	if (S.Montage)
	{
		Candidates.Add(S.Montage);
	}
	for (const TObjectPtr<UAnimMontage>& M : S.MontageVariants)
	{
		if (M)
		{
			Candidates.Add(M);
		}
	}

	if (Candidates.Num() == 0)
	{
		return nullptr;   // a stage with no animation still swings and still hits, by design
	}
	return Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
}

bool UGSGA_SwordLight::ResolveImpact(AActor* Target, const FVector& SweepOrigin, FHitResult& OutHit) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Target || !Avatar)
	{
		return false;
	}

	// Trace from the sweep centre THROUGH the target rather than stopping at its origin: a ray that
	// ends exactly at the actor location can terminate inside the capsule before it ever reaches the
	// mesh, which returns a hit with no bone - the very thing this function exists to recover.
	const FVector ToTarget = Target->GetActorLocation() - SweepOrigin;
	const FVector Dir = ToTarget.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		return false;
	}
	const FVector TraceStart = SweepOrigin;
	const FVector TraceEnd = Target->GetActorLocation() + Dir * 150.f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSSwordImpact), /*bTraceComplex*/ true, Avatar);
	// The material matters: without it PlayImpactEffect has nothing to pick an effect from and every
	// surface sounds the same.
	Params.bReturnPhysicalMaterial = true;

	// ---- TRACE THE MESH COMPONENT, NOT A COLLISION CHANNEL --------------------------------------
	//
	// MEASURED: the character mesh answers ECC_Visibility with ECR_IGNORE, so the obvious
	// ActorLineTraceSingle(..., ECC_Visibility, ...) can NEVER hit it - the first version of this
	// function did exactly that and logged "trace missed the mesh, bone='None'" on a hit that had
	// plainly connected. Channel responses are the wrong instrument here anyway: the overlap has
	// already decided this target is hit, so the only question left is WHERE, and asking the mesh
	// whether it wants to answer a visibility query is beside the point.
	//
	// LineTraceComponent goes straight at the component's physics asset and ignores channel
	// responses, which is what returns a per-bone hit.
	if (const ACharacter* TargetChar = Cast<ACharacter>(Target))
	{
		if (USkeletalMeshComponent* Mesh = TargetChar->GetMesh())
		{
			if (Mesh->LineTraceComponent(OutHit, TraceStart, TraceEnd, Params))
			{
				return true;
			}
		}
	}

	// Non-character targets (breakables, props) still get a channel trace against their own
	// components - they have no physics asset to interrogate.
	return Target->ActorLineTraceSingle(OutHit, TraceStart, TraceEnd, ECC_Visibility, Params);
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
		const float SwingDamage = bBrokeGuard ? S.Damage * S.GuardBreakDamageScale : S.Damage;

		// ---- THE REAL IMPACT (#349) ----------------------------------------------------------
		//
		// Try for an actual contact point, normal, bone and physical material. This used to be a
		// fabricated hit at the target's actor location - the old comment here admitted as much -
		// and that fabrication is why melee could never place impact FX or tell a head from an arm.
		//
		// The synthesised hit is KEPT as the fallback, not deleted: a capsule-only target, or a mesh
		// the ray grazes past, must still take the damage the overlap already granted. Cosmetics may
		// degrade; a connected swing may not stop connecting.
		const FVector ShotDirection =
			(Target->GetActorLocation() - Avatar->GetActorLocation()).GetSafeNormal();

		FHitResult Hit;
		const bool bRealImpact = ResolveImpact(Target, Origin, Hit);
		if (!bRealImpact)
		{
			Hit = FHitResult();
			Hit.ImpactPoint = Target->GetActorLocation();
			Hit.Location = Hit.ImpactPoint;
			Hit.ImpactNormal = -ShotDirection;
			Hit.Normal = Hit.ImpactNormal;
			Hit.HitObjectHandle = FActorInstanceHandle(Target);
		}

		if (GSCombatDebugEnabled())
		{
			UE_LOG(LogTemp, Log,
				TEXT("[GS.Combat] %s hit %s%s bone='%s' at (%.0f,%.0f,%.0f)"),
				*GetNameSafe(Avatar), *GetNameSafe(Target),
				bRealImpact ? TEXT("") : TEXT(" [SYNTHESISED - trace missed the mesh]"),
				*Hit.BoneName.ToString(),
				Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z);
		}

		// Per-material impact VFX and sound, placed at the real contact point. ACF picks the effect
		// from the damage type plus the physical material under the blade, which is exactly what
		// ResolveImpact just recovered - and what the fabricated hit could never supply.
		if (bRealImpact)
		{
			UACMCollisionsFunctionLibrary::PlayImpactEffect(
				UGSDamageType_Axe::StaticClass(), Hit, Avatar);
		}

		if (GSUseACFDamage > 0)
		{
			if (UACFDamageHandlerComponent* Handler =
					Target->FindComponentByClass<UACFDamageHandlerComponent>())
			{
				// ACF reads hitDirection for the ragdoll impulse and for which hit reaction to play,
				// so a zero vector here would make every corpse fall the same way.
				const FPointDamageEvent DamageEvent(SwingDamage, Hit, ShotDirection,
					UGSDamageType_Axe::StaticClass());

				Handler->TakeDamage(Target, SwingDamage, DamageEvent,
					Avatar->GetInstigatorController(), Avatar);
			}
		}
		else
		{
			// The Damage.* tag is BOTH the type marker and the SetByCaller key, and it must go on the
			// SPEC - the same tag sitting in a Blueprint effect's asset tags is invisible to
			// UGSDamageExecCalculation and silently deals zero.
			SpecHandle.Data->AddDynamicAssetTag(GSTags::Damage_Dagger);
			SpecHandle.Data->SetSetByCallerMagnitude(GSTags::Damage_Dagger, SwingDamage);

			SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);
		}

		// Frenzy, "anything you attack" (GDD §2.5). Announced from the swing rather than from the
		// attribute path, because only the swing knows both ends of the hit - the victim's
		// HandleHealthChanged can name its attacker but nothing downstream can name the attacker's
		// victim. The hostility test three blocks up has already run, so a friendly-fire hit on your
		// own horde can never reach here and put a goblin on the threat list.
		if (AGSCharacterBase* AttackerChar = Cast<AGSCharacterBase>(Avatar))
		{
			AttackerChar->NotifyDealtDamage(Target);
		}

		// ---- WAS IT DEFLECTED? (#355) ------------------------------------------------------
		//
		// The damage calc has just run (inside ApplyGameplayEffectSpecToTarget above) and, if the
		// blow hit plate head-on, it set State.LastHitDeflected on US. Read it now and clear it
		// now, on the same frame, so it can never leak into the next swing's verdict. This is the
		// one signal that lets "my hit was refused" look different from "my hit did nothing" -
		// which is the whole of why Michael's knight read as a sponge.
		bool bDeflected = false;
		if (SourceASC && SourceASC->HasMatchingGameplayTag(GSTags::State_LastHitDeflected))
		{
			bDeflected = true;
			SourceASC->SetLooseGameplayTagCount(GSTags::State_LastHitDeflected, 0);
		}

		// ---- HITSTOP (#353) ----------------------------------------------------------------
		//
		// Both parties freeze for a few frames on contact, scaled by the weight of the blow. The
		// numbers are per-stage data so a light, a heavy and a guard-break can each land
		// differently, and the whole thing is off when a stage authors 0.
		//
		// A DEFLECTED blow gets the attacker's stop but NOT the victim's: the plate absorbed it, so
		// the knight should barely register while the attacker's blade sticks on the steel. That
		// asymmetry is most of the cue. Here rather than in the damage exec calculation because only
		// the SWING knows both ends of the hit and which stage it was.
		if (S.HitstopSeconds > 0.f)
		{
			const float Hold = bBrokeGuard ? S.HitstopSeconds * S.GuardBreakHitstopScale : S.HitstopSeconds;
			if (AGSCharacterBase* AttackerChar = Cast<AGSCharacterBase>(Avatar))
			{
				AttackerChar->ApplyHitstop(Hold, S.HitstopScale);
			}
			if (!bDeflected)
			{
				if (AGSCharacterBase* VictimChar = Cast<AGSCharacterBase>(Target))
				{
					VictimChar->ApplyHitstop(Hold, S.HitstopScale);
				}
			}
		}

		// ---- CAMERA SHAKE (#355) -----------------------------------------------------------
		//
		// The attacker's own camera, so only a player-controlled attacker ever has one to shake.
		// A deflect uses the LIGHT shake regardless of stage weight - a heavy that bounces off
		// plate should feel like it bounced, not like it landed.
		if (S.bCameraShakeOnHit)
		{
			if (const APawn* AttackerPawn = Cast<APawn>(Avatar))
			{
				if (APlayerController* PC = Cast<APlayerController>(AttackerPawn->GetController()))
				{
					if (PC->PlayerCameraManager)
					{
						const TSubclassOf<UCameraShakeBase> ShakeClass = (S.bHeavyShake && !bDeflected)
							? TSubclassOf<UCameraShakeBase>(UGSHitCameraShake_Heavy::StaticClass())
							: TSubclassOf<UCameraShakeBase>(UGSHitCameraShake_Light::StaticClass());
						PC->PlayerCameraManager->StartCameraShake(ShakeClass);
					}
				}
			}
		}

		if (bDeflected && GSCombatDebugEnabled())
		{
			UE_LOG(LogTemp, Log, TEXT("[GS.Combat] %s's blow was DEFLECTED by %s's plate - attacker-only stop, light shake"),
				*GetNameSafe(Avatar), *GetNameSafe(Target));
		}

		if (bShowDebug)
		{
			DrawDebugSphere(World, Target->GetActorLocation() + FVector(0, 0, 60.f), 45.f, 12,
				FColor::Red, false, 0.6f, 0, 3.f);
		}
	}

	// ---- props (#165) ------------------------------------------------------------------------
	//
	// A SECOND overlap, deliberately, rather than adding WorldStatic to the one above. Everything
	// between here and the top of the loop - the hostility check, the ASC lookup, guard break,
	// recoil, the damage effect - is about characters, and a crate has no business entering any of
	// it. Widening the first query would have put props into all of it at once.
	//
	// Same HitActorsThisSwing set, so a swing that clips a crate and a militiaman hits each once, and
	// a three-hit chain gets three cracks at the same crate.
	if (S.SmashDamage > 0)
	{
		const int32 Smashed = UGSRaidLibrary::SmashBreakablesInArc(this, Avatar, Origin, S.SweepRadius,
			Forward, S.SweepArcDegrees, S.SmashDamage, HitActorsThisSwing);

		if (Smashed > 0 && bShowDebug)
		{
			DrawDebugString(World, Origin + FVector(0.f, 0.f, 90.f),
				FString::Printf(TEXT("smashed %d"), Smashed), nullptr, FColor::Yellow, 0.6f, true);
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
			: FVector::ZeroVector, GetAvatarActorFromActorInfo());
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

	// And a leaked root-motion mode would be worse than either: the character would ignore root
	// motion for every animation afterwards, so grounded attacks would quietly lose their lunge for
	// the rest of the raid. Unconditional - it no-ops when the swing never suppressed anything.
	RestoreRootMotionMode();

	// A swing cancelled DURING its windup must not strand the telegraph on the actor - every
	// defender in earshot would hold a guard against a hit that is never coming, and on a corpse it
	// would never clear at all. Unconditional because the normal path has already zeroed it.
	GSSetWindupTelegraph(GetAbilitySystemComponentFromActorInfo(), false);

	// Same reasoning for the lock: a swing that ended any way at all is no longer committed, and a
	// grant left locked is one the preemption pass can never reclaim.
	GSSetTokenLocked(GetAvatarActorFromActorInfo(), false);

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
