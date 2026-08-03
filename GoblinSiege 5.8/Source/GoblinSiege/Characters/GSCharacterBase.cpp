#include "Characters/GSCharacterBase.h"
#include "Attributes/GSAttributeSetBase.h"
#include "Combat/GSGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "Animation/AnimMontage.h"
#include "Core/GSGameMode.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"

AGSCharacterBase::AGSCharacterBase()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSetBase = CreateDefaultSubobject<UGSAttributeSetBase>(TEXT("AttributeSetBase"));
}

void AGSCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Server: init ASC actor info now that we have a controller (needed for player-owned
	// abilities); AI-controlled defenders also route through here via their AIController.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void AGSCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Client: PlayerState arrives after possession on the owning client - re-init ASC actor info here.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void AGSCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UGSAttributeSetBase::GetHealthAttribute())
			.AddUObject(this, &AGSCharacterBase::HandleHealthChanged);
	}
}

void AGSCharacterBase::InitializeAttributesFromEffect(TSubclassOf<UGameplayEffect> InitEffectClass, float Level)
{
	if (!InitEffectClass || !AbilitySystemComponent || bAttributesInitialized)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(InitEffectClass, Level, EffectContext);
	if (SpecHandle.IsValid())
	{
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		bAttributesInitialized = true;
	}
}

float AGSCharacterBase::GetHealth() const
{
	return AttributeSetBase ? AttributeSetBase->GetHealth() : 0.f;
}

float AGSCharacterBase::GetMaxHealth() const
{
	return AttributeSetBase ? AttributeSetBase->GetMaxHealth() : 0.f;
}

bool AGSCharacterBase::IsBlocking() const
{
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(GSTags::State_Blocking);
}

void AGSCharacterBase::PlayHitReact(const FVector& FromDirection)
{
	if (!bEnableHitReact || bIsDead)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Two gates, and they do different jobs. The cooldown stops a combo restarting the montage on
	// every contact - three hits in 2.1s should read as three staggers, not as a vibration. The
	// tag check stops a flinch interrupting itself mid-play, which looks worse than no flinch.
	if (World->GetTimeSeconds() - LastHitReactTime < HitReactCooldownSeconds)
	{
		return;
	}
	if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(GSTags::State_HitReact))
	{
		return;
	}

	UAnimMontage* Chosen = nullptr;

	if (IsBlocking() && BlockReact)
	{
		// A blocked hit is absorbed by the guard, not felt in the body.
		Chosen = BlockReact;
	}
	else
	{
		Chosen = HitReactFront;

		// Directional pick. FromDirection points from us toward the attacker, so a positive dot
		// with our right vector means the blow came from our right.
		if (!FromDirection.IsNearlyZero())
		{
			FVector Flat = FromDirection;
			Flat.Z = 0.f;
			if (!Flat.IsNearlyZero())
			{
				const float SideDot = FVector::DotProduct(Flat.GetSafeNormal(), GetActorRightVector());
				if (SideDot < -0.5f && HitReactLeft)
				{
					Chosen = HitReactLeft;
				}
				else if (SideDot > 0.5f && HitReactRight)
				{
					Chosen = HitReactRight;
				}
			}
		}
	}

	if (!Chosen)
	{
		return; // no montage authored yet - silently fine, the damage still landed
	}

	LastHitReactTime = World->GetTimeSeconds();
	const float Length = PlayAnimMontage(Chosen, HitReactPlayRate);

	if (AbilitySystemComponent && Length > 0.f)
	{
		AbilitySystemComponent->AddLooseGameplayTag(GSTags::State_HitReact);
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (AbilitySystemComponent)
				{
					AbilitySystemComponent->RemoveLooseGameplayTag(GSTags::State_HitReact);
				}
			}),
			Length, false);
	}
}

void AGSCharacterBase::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	const float Delta = Data.NewValue - Data.OldValue;
	const float MaxHealth = GetMaxHealth();

	// The dynamic mirror of GAS's non-dynamic attribute delegate. Everything Blueprint-side - the
	// health bar above all - listens here, because the GAS delegate itself cannot be bound from
	// Blueprint or UMG at all.
	OnHealthChanged.Broadcast(Data.NewValue, MaxHealth, Delta);

	if (!bIsDead && Data.NewValue <= 0.f)
	{
		HandleDeath();
		return;
	}

	// Flinch on meaningful damage only. The fraction gate is what keeps a burning wheat field
	// from making everyone standing in it twitch four times a second.
	if (Delta < 0.f && MaxHealth > 0.f && (-Delta / MaxHealth) >= HitReactMinDamageFraction)
	{
		FVector FromAttacker = FVector::ZeroVector;
		if (Data.GEModData)
		{
			// Not named "Instigator": AActor already has a member of that name and this module
			// builds with warnings-as-errors, so the shadow is a hard build failure.
			if (const AActor* Attacker = Data.GEModData->EffectSpec.GetContext().GetInstigator())
			{
				FromAttacker = Attacker->GetActorLocation() - GetActorLocation();
			}
		}
		PlayHitReact(FromAttacker);
	}
}

void AGSCharacterBase::HandleDeath()
{
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;

	// 2026-08-02: this used to be a comment saying ragdoll belonged in subclasses. Nothing ever
	// implemented it, so until today death was invisible AND you kept full control of your corpse -
	// you could walk around at 0 HP indefinitely. Three things happen here now.
	//
	// 1. The State.Dead loose tag. This is the one with reach: AGSFireVolume::ApplyFireDamageTo
	//    already checks it (GSFireVolume.cpp:367) and skips corpses, so adding the tag retroactively
	//    makes an existing guard start working. Loose rather than a GameplayEffect because a corpse
	//    should never lose the tag to a duration expiring.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(GSTags::State_Dead);
		AbilitySystemComponent->CancelAllAbilities();
	}

	// 2. Stop driving the corpse. Movement off before physics, or the movement component keeps
	//    fighting the simulation and the body skates.
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}
	if (AController* C = GetController())
	{
		C->SetIgnoreMoveInput(true);
		C->SetIgnoreLookInput(true);
	}

	// 3. Ragdoll. Capsule collision must go first: leave it blocking and the capsule holds the
	//    corpse up in the air while the mesh flops underneath it, which is the classic "floating
	//    dead body" bug. Mesh needs a PhysicsAsset - if one is missing the engine warns and the
	//    body simply stays in its last pose, which is survivable rather than fatal.
	if (bRagdollOnDeath)
	{
		if (UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			MeshComp->SetCollisionProfileName(RagdollCollisionProfile);
			MeshComp->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
			MeshComp->SetAllBodiesSimulatePhysics(true);
			MeshComp->SetSimulatePhysics(true);
			MeshComp->WakeAllRigidBodies();
			MeshComp->bBlendPhysics = true;

			if (DeathImpulse > 0.f)
			{
				MeshComp->AddImpulse(GetActorForwardVector() * -DeathImpulse, NAME_None, true);
			}
		}
	}

	if (CorpseLifespan > 0.f)
	{
		SetLifeSpan(CorpseLifespan);
	}

	OnDied.Broadcast();

	if (HasAuthority())
	{
		if (AGSGameMode* GM = GetWorld()->GetAuthGameMode<AGSGameMode>())
		{
			GM->HandleGoblinDeath(this, GetController());
		}
	}
}

void AGSCharacterBase::ApplyRespawnState(float HealthFraction, float InvulnerabilitySeconds)
{
	bIsDead = false;

	if (AttributeSetBase)
	{
		AttributeSetBase->SetHealth(AttributeSetBase->GetMaxHealth() * FMath::Clamp(HealthFraction, 0.f, 1.f));
	}

	// 2026-08-02: HandleDeath now ragdolls and locks input, so respawn has to put all of that back.
	// Before today this function only reset bIsDead and the health number, which was correct when
	// death did nothing - and would now hand you a respawned character who is a limp body with no
	// controls. Order matters: un-simulate before re-enabling the capsule, or the mesh snaps to the
	// capsule while still simulating and the character launches.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(GSTags::State_Dead);
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (MeshComp->IsSimulatingPhysics())
		{
			MeshComp->SetSimulatePhysics(false);
			MeshComp->bBlendPhysics = false;
			MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			MeshComp->SetCollisionProfileName(TEXT("CharacterMesh"));
			MeshComp->AttachToComponent(GetCapsuleComponent(),
				FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			MeshComp->SetRelativeTransform(
				GetClass()->GetDefaultObject<ACharacter>()->GetMesh()->GetRelativeTransform());
		}
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Walking);
	}

	if (AController* C = GetController())
	{
		C->SetIgnoreMoveInput(false);
		C->SetIgnoreLookInput(false);
	}

	// A short-duration GameplayEffect granting a "State.Invulnerable" tag (checked by the damage
	// GameplayEffect's application requirements) is the idiomatic GAS way to implement the
	// invulnerability window - apply it here once that effect class exists as a data asset.
}

void AGSCharacterBase::SetTurnRateRadPerSec(float NewTurnRateRadPerSec)
{
	TurnRateRadPerSec = NewTurnRateRadPerSec;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->RotationRate = FRotator(0.f, FMath::RadiansToDegrees(TurnRateRadPerSec), 0.f);
	}
}
