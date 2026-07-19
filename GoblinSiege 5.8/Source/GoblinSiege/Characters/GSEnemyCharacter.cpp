#include "Characters/GSEnemyCharacter.h"
#include "Combat/GSRaceDataAsset.h"
#include "AbilitySystemComponent.h"
#include "Attributes/GSAttributeSetBase.h"
#include "GameFramework/CharacterMovementComponent.h"

AGSEnemyCharacter::AGSEnemyCharacter()
{
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
