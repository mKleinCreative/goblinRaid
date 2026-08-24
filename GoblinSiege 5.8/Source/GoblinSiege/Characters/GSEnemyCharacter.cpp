#include "Characters/GSEnemyCharacter.h"
#include "Combat/GSRaceDataAsset.h"
#include "AI/GSAIControllerBase.h"
#include "Weapons/GSWeaponComponent.h"
#include "Weapons/GSWeaponDataAsset.h"
#include "Combat/GSGameplayTags.h"
#include "Attributes/GSAttributeSetBase.h"
#include "GameFramework/CharacterMovementComponent.h"

AGSEnemyCharacter::AGSEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 2026-08-02: until today this constructor was empty, which meant every defender was possessed
	// by a stock AAIController - so AGSAIControllerBase's perception setup had never actually run on
	// an enemy in this project, ever. AGSHordeGoblin already did this (GSHordeGoblin.cpp:6-7); the
	// enemy class simply never got the same two lines.
	AIControllerClass = AGSAIControllerBase::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Capsules already block each other, so defenders never literally interpenetrate - but without
	// avoidance the pathfinder does not know the other two exist. Three of them steer at the same
	// point, arrive shoulder to shoulder and shove, which reads as a single merged blob of guards.
	// RVO makes them steer around each other on the way in and settle in an arc instead of a pile.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bUseRVOAvoidance = true;

		// Weight is how much this agent yields. 0.5 = everyone gives way equally; at 0 (the default,
		// which is what made avoidance a no-op even if it had been switched on) nobody yields.
		Move->AvoidanceWeight = 0.5f;
		Move->AvoidanceConsiderationRadius = 600.f;
	}

	// The same component the player carries. A defender with no weapon component cannot hold
	// anything at all - which is why every guard in the game has been fighting bare-handed.
	WeaponComponent = CreateDefaultSubobject<UGSWeaponComponent>(TEXT("WeaponComponent"));

	// This class arms itself from DefaultWeapon in BeginPlay (below), which runs AFTER the
	// component's - so without this flag the component's null-EquippedWeapon warning fires on every
	// correctly-authored defender in the game. It did, on all six, and #272 was opened to chase it.
	//
	// CONSTRUCTOR, not BeginPlay: component BeginPlay runs first, so a flag set any later arrives
	// after the check it exists to suppress. AGSHordeGoblin learned the same thing in #268
	// (GSHordeGoblin.cpp:47); this class simply never got the line.
	WeaponComponent->bExpectsExternalEquip = true;

	// Was an inline `= EGSWeaponSlot::Sword` on the header until #274. A native gameplay tag is not
	// a constant expression, so the default moves here. BP_ErikaArcher overrides it to Bow; every
	// other adversary rides this default.
	DefaultSlot = GSTags::WeaponSlot_Primary;
}

void AGSEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (RaceData && HasAuthority())
	{
		InitializeFromArchetype(RaceData, ArchetypeRowName);
	}

	// The four grants and the verbs that use them live on AGSCharacterBase since #069; this is the
	// one call that replaced them here.
	GrantCombatAbilities();

	// Put the weapon in his hand. The slot is chosen per Blueprint rather than by the component,
	// because a defender has no weapon wheel to choose with: DefaultSlot defaults to Sword for a
	// garrison, and BP_ErikaArcher sets Bow. That "once it has bow content of its own" is now.
	if (WeaponComponent && DefaultWeapon)
	{
		WeaponComponent->EquipWeapon(DefaultWeapon);
		WeaponComponent->SetSlot(DefaultSlot);
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

	// Adopt the race so melee knows who not to hit. Doing it here rather than per-Blueprint means
	// one field on the data asset covers every defender that references it.
	if (RaceData->RaceTag.IsValid())
	{
		RaceTag = RaceData->RaceTag;
	}

	if (const FGSArchetypeDefinition* Archetype = RaceData->FindArchetype(ArchetypeRowName))
	{
		AttributeSetBase->InitHealth(Archetype->Health);
		AttributeSetBase->InitMaxHealth(Archetype->Health);
		AttributeSetBase->InitArmor(Archetype->Armor);
		AttributeSetBase->InitMoveSpeedMultiplier(1.f);

		// Through SetBaseWalkSpeed, not straight onto MaxWalkSpeed: the archetype owns this pawn's
		// BASELINE speed, while blocks, carries and future roots are multipliers on top of it. Writing
		// the movement component directly would erase any slow already active and be erased by the
		// next one.
		//
		// Zero means the archetype has no opinion and the Blueprint's own value stands. Roles are
		// shared by bodies of different sizes - the six human defenders each derive walk speed from
		// their height - and one row's number would flatten all of them.
		if (Archetype->MoveSpeed > 0.f)
		{
			SetBaseWalkSpeed(Archetype->MoveSpeed);
		}
		TurnRateRadPerSec = Archetype->TurnRateRadPerSec;

		bAttributesInitialized = true;

		// Archetype->Role (e.g. "FrontlineFiller", "BacklineHarasser", "SupportAura") should drive
		// which Behavior Tree this AI's controller runs - see AI module (GSAIControllerBase).
	}
}
