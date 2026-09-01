#include "Weapons/Abilities/GSGA_DodgeRoll.h"
#include "Characters/GSPlayerCharacter.h"
#include "Characters/GSStaminaComponent.h"
#include "Combat/GSGameplayTags.h"
#include "Combat/GSGE_MoveSpeedScalar.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"

// The dodge path had NO instrument at all. #178 shipped four directional rolls, every static check
// passed - slots assigned, four distinct clips, the right slot name, facing decoupled, the picker
// maths correct by inspection - and it still played the forward roll in all four directions. Three
// investigation passes ended in a dead end because nothing anywhere reports what the picker was
// actually handed or what it chose. Same lesson as GS.Anim.Snapshot (#137): a system with no
// runtime readout is a system you argue about instead of measure.
static TAutoConsoleVariable<int32> CVarLogDodge(
	TEXT("GS.Combat.LogDodge"),
	0,
	TEXT("Log every dodge: move input, world dodge direction, the actor frame it is projected into, "
	     "both dot products, which montage was chosen and whether it actually played. 0 off, 1 on."),
	ECVF_Default);

UGSGA_DodgeRoll::UGSGA_DodgeRoll()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// Active exactly while the roll is in flight - GAS auto-manages add/remove for the ability's
	// lifetime, so nothing else needs to touch this tag manually.
	ActivationOwnedTags.AddTag(GSTags::State_Dodging);
}

void UGSGA_DodgeRoll::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Stamina first, BEFORE CommitAbility: a refused dodge should cost nothing at all, and
	// committing an ability we are about to cancel would burn any cost or cooldown attached to it.
	// FindComponentByClass rather than reaching through AGSPlayerCharacter - the component is only
	// on the player (AI melee is paced by its own BT cooldown), so a null here is a legitimate
	// "this pawn has no stamina model" rather than an error, and it must not refuse the dodge.
	if (DodgeStaminaCost > 0.f)
	{
		if (const ACharacter* StamAvatar = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			if (UGSStaminaComponent* Stamina = StamAvatar->FindComponentByClass<UGSStaminaComponent>())
			{
				if (!Stamina->TryConsume(DodgeStaminaCost))
				{
					if (CVarLogDodge.GetValueOnAnyThread() != 0)
					{
						UE_LOG(LogTemp, Log,
							TEXT("[GS.Dodge] refused - stamina %.1f of %.1f, needs %.1f"),
							Stamina->GetStamina(), Stamina->GetMaxStamina(), DodgeStaminaCost);
					}
					EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
					return;
				}
			}
		}
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Roll to put yourself out (2026-08-30, #370/#371): the ONLY way UGSGE_Burning's clinging burn
	// ends is here, by the tag it grants - see UGSGE_Burning's header. A no-op when nothing is
	// burning, so this costs nothing on every other roll.
	if (UAbilitySystemComponent* BurningASC = GetAbilitySystemComponentFromActorInfo())
	{
		BurningASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(GSTags::State_Burning));
	}

	// Length of the roll animation, if one plays. Drives the commit window when
	// bCommitForFullMontage is set; stays 0 when no montage is assigned, in which case the ability
	// falls back to DodgeDurationSeconds and behaves exactly as it did before animation existed.
	float MontageSeconds = 0.f;

	ACharacter* Avatar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (Avatar && Avatar->HasAuthority())
	{
		const AGSPlayerCharacter* PlayerAvatar = Cast<AGSPlayerCharacter>(Avatar);
		const FVector DodgeDir = PlayerAvatar ? PlayerAvatar->GetDodgeDirection() : Avatar->GetActorForwardVector();

		// Friction goes to zero BEFORE the launch, so the impulse is not already being eaten on the
		// frame it is applied (#345).
		SuppressFriction(Avatar);

		// And the speed CEILING comes off before the launch too, for the same reason: walking mode
		// re-clamps velocity toward MaxWalkSpeed (measured at 470 on the player), so without this
		// the launch is throttled within a frame no matter how large DodgeSpeed is.
		if (UAbilitySystemComponent* SpeedASC = GetAbilitySystemComponentFromActorInfo())
		{
			if (RollSpeedCapMultiplier > 1.f)
			{
				FGameplayEffectContextHandle Context = SpeedASC->MakeEffectContext();
				Context.AddSourceObject(this);
				const FGameplayEffectSpecHandle Spec = SpeedASC->MakeOutgoingSpec(
					UGSGE_MoveSpeedScalar::StaticClass(), 1.f, Context);
				if (Spec.IsValid())
				{
					Spec.Data->SetSetByCallerMagnitude(GSTags::Data_MoveSpeedScalar, RollSpeedCapMultiplier);
					RollSpeedHandle = SpeedASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
				}
			}
		}

		Avatar->LaunchCharacter(DodgeDir * DodgeSpeed, true, false);

		// PlayAnimMontage returns 0 on a skeleton mismatch and says nothing about it
		// (AGSCharacterBase gates on this) - so read the returned length rather than assuming the
		// roll is on screen. A silent 0 here is the difference between a rolling goblin and a
		// goblin sliding across the floor at 900uu/s in its idle pose.
		if (UAnimMontage* Roll = PickDirectionalMontage(Avatar, DodgeDir))
		{
			const float Rate = FMath::IsNearlyZero(DodgeMontagePlayRate) ? 1.f : DodgeMontagePlayRate;
			MontageSeconds = Avatar->PlayAnimMontage(Roll, Rate);
			if (MontageSeconds <= 0.f)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[GS.Dodge] '%s' refused to play on %s - skeleton mismatch? The roll will "
					     "launch with no animation."),
					*Roll->GetName(), *Avatar->GetName());
			}
		}

		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			if (IFrameEffectClass)
			{
				FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
				Context.AddSourceObject(this);
				const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(IFrameEffectClass, 1.f, Context);
				if (SpecHandle.IsValid())
				{
					ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
				}
			}
		}
	}

	// Hold the commit for whichever is longer when asked to cover the animation, so State.Dodging
	// (and therefore the movement-input lock) does not release mid-roll and let the player walk
	// while a full-body montage is still playing.
	const float CommitSeconds = (bCommitForFullMontage && MontageSeconds > 0.f)
		? FMath::Max(DodgeDurationSeconds, MontageSeconds)
		: DodgeDurationSeconds;

	if (UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, CommitSeconds))
	{
		WaitTask->OnFinish.AddDynamic(this, &UGSGA_DodgeRoll::OnDodgeFinished);
		WaitTask->ReadyForActivation();
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

UAnimMontage* UGSGA_DodgeRoll::PickDirectionalMontage(const ACharacter* Avatar, const FVector& WorldDodgeDir) const
{
	if (!Avatar || WorldDodgeDir.IsNearlyZero())
	{
		// GetDodgeDirection() returns GetActorForwardVector() when LastMoveInput is zero, so this
		// arm is also reached by "dodged without a direction held" - which looks identical to a
		// broken picker from outside.
		if (CVarLogDodge.GetValueOnAnyThread() != 0)
		{
			UE_LOG(LogTemp, Log,
				TEXT("[GS.Dodge] no usable direction (avatar=%s, dir=%s) -> Forward by fallback"),
				Avatar ? TEXT("ok") : TEXT("NULL"), *WorldDodgeDir.ToCompactString());
		}
		return DodgeMontageForward.Get();
	}

	// Project the world-space dodge direction into the actor's own frame. GetDodgeDirection() is
	// camera-relative, but which roll to PLAY is a question about the body, not the camera.
	const FVector Flat = FVector(WorldDodgeDir.X, WorldDodgeDir.Y, 0.f).GetSafeNormal();
	const float Fwd = FVector::DotProduct(Flat, Avatar->GetActorForwardVector());
	const float Rgt = FVector::DotProduct(Flat, Avatar->GetActorRightVector());

	UAnimMontage* Chosen = (FMath::Abs(Fwd) >= FMath::Abs(Rgt))
		? (Fwd >= 0.f ? DodgeMontageForward : DodgeMontageBackward)
		: (Rgt >= 0.f ? DodgeMontageRight   : DodgeMontageLeft);

	if (CVarLogDodge.GetValueOnAnyThread() != 0)
	{
		// Print the INPUTS as well as the verdict. "It chose Forward" on its own cannot tell you
		// whether the direction was wrong or the projection was - which is exactly the ambiguity
		// that cost three passes.
		UE_LOG(LogTemp, Log,
			TEXT("[GS.Dodge] dir=(%.2f,%.2f) actorFwd=(%.2f,%.2f) actorRgt=(%.2f,%.2f) "
			     "dotFwd=%.3f dotRgt=%.3f -> %s%s"),
			Flat.X, Flat.Y,
			Avatar->GetActorForwardVector().X, Avatar->GetActorForwardVector().Y,
			Avatar->GetActorRightVector().X, Avatar->GetActorRightVector().Y,
			Fwd, Rgt,
			Chosen ? *Chosen->GetName() : TEXT("<null slot>"),
			Chosen ? TEXT("") : TEXT(" (falls back to Forward)"));
	}

	// Any unassigned quadrant falls back to the forward roll rather than to no animation at all.
	// .Get() on both arms: mixing a raw UAnimMontage* with a TObjectPtr in one conditional is
	// ambiguous under MSVC (C2445).
	return Chosen ? Chosen : DodgeMontageForward.Get();
}

void UGSGA_DodgeRoll::SuppressFriction(ACharacter* Avatar)
{
	if (!bSuppressFrictionDuringRoll || !Avatar)
	{
		return;
	}
	UCharacterMovementComponent* Move = Avatar->GetCharacterMovement();
	if (!Move)
	{
		return;
	}

	// Cache once. A second suppress without an intervening restore would otherwise store the
	// already-zeroed values and the restore would make the zero permanent - the same compounding
	// bug UGSGA_SwordLight::ApplyMoveSpeedScale documents for MaxWalkSpeed.
	if (CachedGroundFriction < 0.f)
	{
		CachedGroundFriction = Move->GroundFriction;
		CachedBrakingDeceleration = Move->BrakingDecelerationWalking;
	}

	Move->GroundFriction = 0.f;
	Move->BrakingDecelerationWalking = 0.f;
	FrictionAvatar = Avatar;
}

void UGSGA_DodgeRoll::RestoreFriction()
{
	if (CachedGroundFriction < 0.f)
	{
		return; // never suppressed, or already restored
	}

	if (ACharacter* Avatar = FrictionAvatar.Get())
	{
		if (UCharacterMovementComponent* Move = Avatar->GetCharacterMovement())
		{
			Move->GroundFriction = CachedGroundFriction;
			Move->BrakingDecelerationWalking = CachedBrakingDeceleration;
		}
	}

	CachedGroundFriction = -1.f;
	CachedBrakingDeceleration = -1.f;
	FrictionAvatar = nullptr;
}

void UGSGA_DodgeRoll::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Every exit runs through here - the timed finish, the montage-length finish, a cancel from
	// death or an interrupt - which is exactly why the restore lives here and not in
	// OnDodgeFinished. A cancelled roll that kept zero friction would slide forever, and one that
	// kept the speed cap lifted would leave the goblin permanently sprinting.
	RestoreFriction();

	if (RollSpeedHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveActiveGameplayEffect(RollSpeedHandle);
		}
		RollSpeedHandle = FActiveGameplayEffectHandle();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGSGA_DodgeRoll::OnDodgeFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
