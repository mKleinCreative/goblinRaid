#include "Characters/GSEnemyCharacter.h"
#include "Combat/GSRaceDataAsset.h"
#include "AI/GSAIControllerBase.h"
#include "AbilitySystemComponent.h"
#include "Combat/GSGameplayTags.h"
#include "Abilities/GameplayAbility.h"
#include "Attributes/GSAttributeSetBase.h"
#include "GameFramework/CharacterMovementComponent.h"

AGSEnemyCharacter::AGSEnemyCharacter()
{
	// 2026-08-02: until today this constructor was empty, which meant every defender was possessed
	// by a stock AAIController - so AGSAIControllerBase's perception setup had never actually run on
	// an enemy in this project, ever. AGSHordeGoblin already did this (GSHordeGoblin.cpp:6-7); the
	// enemy class simply never got the same two lines.
	AIControllerClass = AGSAIControllerBase::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AGSEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (RaceData && HasAuthority())
	{
		InitializeFromArchetype(RaceData, ArchetypeRowName);
	}

	if (HasAuthority())
	{
		GrantIfSet(LightAttackAbilityClass);
		GrantIfSet(HeavyAttackAbilityClass);
		GrantIfSet(GuardBreakAbilityClass);
		GrantIfSet(BlockAbilityClass);
	}
}

void AGSEnemyCharacter::GrantIfSet(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (AbilityClass && AbilitySystemComponent)
	{
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
	}
}

bool AGSEnemyCharacter::TryActivate(TSubclassOf<UGameplayAbility> AbilityClass)
{
	return AbilityClass && AbilitySystemComponent
		&& AbilitySystemComponent->TryActivateAbilityByClass(AbilityClass);
}

bool AGSEnemyCharacter::TryLightAttack()
{
	// No extra gating here on purpose. GAS already refuses to re-activate an ability that is
	// running, and UGSGA_SwordLight treats that refusal as the combo buffer - so an AI that
	// spams this gets the same chained combo a player gets by mashing, for free.
	return TryActivate(LightAttackAbilityClass);
}

bool AGSEnemyCharacter::TryHeavyAttack()
{
	return TryActivate(HeavyAttackAbilityClass);
}

bool AGSEnemyCharacter::TryGuardBreak()
{
	return TryActivate(GuardBreakAbilityClass);
}

bool AGSEnemyCharacter::StartBlocking()
{
	return TryActivate(BlockAbilityClass);
}

void AGSEnemyCharacter::StopBlocking()
{
	if (!AbilitySystemComponent)
	{
		return;
	}
	// By tag, never CancelAbilities(nullptr) - that would also kill a swing or a dodge in flight.
	FGameplayTagContainer BlockTags;
	BlockTags.AddTag(GSTags::State_Blocking);
	AbilitySystemComponent->CancelAbilities(&BlockTags);
}

void AGSEnemyCharacter::InitializeFromArchetype(UGSRaceDataAsset* InRaceData, FName InArchetypeRowName)
{
	if (!InRaceData || !AttributeSetBase)
	{
		return;
	}

	RaceData = InRaceData;
	ArchetypeRowName = InArchetypeRowName;

	if (const FGSArchetypeDefinition* Archetype = RaceData->FindArchetype(ArchetypeRowName))
	{
		AttributeSetBase->InitHealth(Archetype->Health);
		AttributeSetBase->InitMaxHealth(Archetype->Health);
		AttributeSetBase->InitArmor(Archetype->Armor);
		AttributeSetBase->InitMoveSpeedMultiplier(1.f);

		GetCharacterMovement()->MaxWalkSpeed = Archetype->MoveSpeed;
		TurnRateRadPerSec = Archetype->TurnRateRadPerSec;

		bAttributesInitialized = true;

		// Archetype->Role (e.g. "FrontlineFiller", "BacklineHarasser", "SupportAura") should drive
		// which Behavior Tree this AI's controller runs - see AI module (GSAIControllerBase).
	}
}
