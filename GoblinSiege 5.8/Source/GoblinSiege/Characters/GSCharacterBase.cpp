#include "Characters/GSCharacterBase.h"
#include "Attributes/GSAttributeSetBase.h"
#include "Combat/GSGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Core/GSGameMode.h"
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

void AGSCharacterBase::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (!bIsDead && Data.NewValue <= 0.f)
	{
		HandleDeath();
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
