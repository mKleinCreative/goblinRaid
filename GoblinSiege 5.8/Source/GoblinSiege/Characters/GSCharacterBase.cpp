#include "Characters/GSCharacterBase.h"
#include "Attributes/GSAttributeSetBase.h"
#include "AbilitySystemComponent.h"
#include "Core/GSGameMode.h"

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

	// Ragdoll / death montage / drop-loot hooks belong in subclasses or a death-handling
	// GameplayAbility triggered by a "State.Dead" tag - kept out of this base on purpose.

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

	// A short-duration GameplayEffect granting a "State.Invulnerable" tag (checked by the damage
	// GameplayEffect's application requirements) is the idiomatic GAS way to implement the
	// invulnerability window - apply it here once that effect class exists as a data asset.
}
