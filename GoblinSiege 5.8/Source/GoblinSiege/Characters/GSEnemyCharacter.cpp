#include "Characters/GSEnemyCharacter.h"
#include "Combat/GSRaceDataAsset.h"
#include "AI/GSAIControllerBase.h"
#include "AbilitySystemComponent.h"
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
