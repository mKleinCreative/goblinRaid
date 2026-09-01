#include "Horde/GSHordeGoblin.h"

#include "Horde/GSHordeAIController.h"
#include "Horde/GSHordeSubsystem.h"
#include "Combat/GSGameplayTags.h"
#include "Combat/GSRaceDataAsset.h"
#include "Interaction/GSCarryComponent.h"
#include "Attributes/GSAttributeSetBase.h"
#include "Weapons/GSWeaponComponent.h"
#include "Weapons/GSWeaponDataAsset.h"
#include "GameFramework/CharacterMovementComponent.h"

AGSHordeGoblin::AGSHordeGoblin(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AIControllerClass = AGSHordeAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// The C++ fallback. DA_Race_Goblin overwrites this through InitializeFromArchetype, but a
	// goblin spawned before that asset exists still has to be non-hostile to the player - an unset
	// RaceTag hits everything (GSCharacterBase::IsHostileTo), so leaving it blank here would make
	// the first ten summoned goblins murder their own summoner.
	RaceTag = GSTags::Race_Goblin;

	// The same three lines AGSEnemyCharacter uses. bUseRVOAvoidance on its own is a no-op:
	// AvoidanceWeight defaults to 0, which means "never yield", and the whole crowd converges to
	// one point and shoves. Cheap avoidance first - DetourCrowd would mean changing this class's
	// controller base and is only worth it if ten goblins measurably need it.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bUseRVOAvoidance = true;
		Move->AvoidanceWeight = 0.5f;
		Move->AvoidanceConsiderationRadius = 600.f;
	}

	// A horde goblin is not a player and must never hold a PlayerState. AGSGameMode::HandleGoblinDeath
	// early-outs without one, which is exactly the "no lives" of repo GDD §5 - and if one were ever
	// given a PlayerState, every horde casualty would silently spend one of the player's five.
	bReplicates = true;

	// The same component the player and (since #097) the defenders carry. Without it a goblin cannot
	// hold anything, which is why the whole warband has been swinging bare fists at armoured men.
	WeaponComponent = CreateDefaultSubobject<UGSWeaponComponent>(TEXT("WeaponComponent"));
	// This goblin equips DA_Weapon_HordeGoblin in its own BeginPlay, after the component's. Say so
	// here, in the constructor, so the component does not report it unarmed in the meantime - a
	// warning that has twice been read as a real defect and twice nearly cost a working asset.
	WeaponComponent->bExpectsExternalEquip = true;

	// The courier's hands (#141). See the header for the socket caveat. This is the component that
	// makes NotifyCourierDelivered reachable at all - it has been written and callerless since #069.
	CarryComponent = CreateDefaultSubobject<UGSCarryComponent>(TEXT("CarryComponent"));
}

void AGSHordeGoblin::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		InitializeFromArchetype();
		GrantCombatAbilities();

		// Put the blade in his hand. Sword explicitly: a goblin has no weapon wheel to choose with,
		// and the horde carries no bow or torch of its own.
		if (WeaponComponent && DefaultWeapon)
		{
			WeaponComponent->EquipWeapon(DefaultWeapon);
			WeaponComponent->SetSlot(GSTags::WeaponSlot_Primary);
		}
	}

	// Frenzy, the "anything that attacks you" half of §2.5. Bound here rather than polled, and
	// routed through the subsystem so one goblin noticing a guard tells all ten.
	OnDamaged.AddDynamic(this, &AGSHordeGoblin::HandleDamagedForFrenzy);

	if (const UGSHordeSubsystem* Horde = UGSHordeSubsystem::Get(this))
	{
		if (AGSCharacterBase* Summoner = Cast<AGSCharacterBase>(Horde->GetFollowTargetFor(this)))
		{
			BoundSummoner = Summoner;
			Summoner->OnDealtDamage.AddDynamic(this, &AGSHordeGoblin::HandleSummonerDealtDamage);
			Summoner->OnDamaged.AddDynamic(this, &AGSHordeGoblin::HandleSummonerDamaged);
		}
	}
}

void AGSHordeGoblin::InitializeFromArchetype()
{
	if (!RaceData || !AttributeSetBase)
	{
		// Not an error worth shouting about on its own - a goblin with no race data simply keeps its
		// Blueprint defaults - but it does mean the 40 HP of the design doc is not being applied.
		return;
	}

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

		// Through SetBaseWalkSpeed, never onto MaxWalkSpeed directly: every slow in this game is a
		// GameplayEffect on MoveSpeedMultiplier, and a direct write would be erased by the next one.
		if (Archetype->MoveSpeed > 0.f)
		{
			SetBaseWalkSpeed(Archetype->MoveSpeed);
		}
		TurnRateRadPerSec = Archetype->TurnRateRadPerSec;

		bAttributesInitialized = true;
	}
}

void AGSHordeGoblin::HandleDeath()
{
	// Unbind before the base class starts tearing down, so a dying goblin cannot answer one last
	// delegate from a summoner it is no longer following.
	if (AGSCharacterBase* Summoner = BoundSummoner.Get())
	{
		Summoner->OnDealtDamage.RemoveDynamic(this, &AGSHordeGoblin::HandleSummonerDealtDamage);
		Summoner->OnDamaged.RemoveDynamic(this, &AGSHordeGoblin::HandleSummonerDamaged);
	}
	BoundSummoner = nullptr;

	// Permanent, and it does NOT debit the pool a second time - this goblin was already spent when
	// the horn summoned it (decision 40). The subsystem only stops counting it as active.
	if (UGSHordeSubsystem* Horde = UGSHordeSubsystem::Get(this))
	{
		Horde->NotifyGoblinDied(this);
	}

	Super::HandleDeath();
}

int32 AGSHordeGoblin::ConsumeLootSackDropValue()
{
	// Recoverable, not auto-banked (2026-08-30, Michael, reversing the instant-bank version this
	// replaced: "I didn't want it to bank on death, I want it to drop its potential points on the
	// ground so you have to go pick it up or have another goblin pick it up"). Just return the purse
	// and let AGSCharacterBase::HandleDeath hand it to SpawnLootSack, same as the player/guard death
	// drop - InitialiseAsCarryable sets bIsCarryable=true on the spawned pouch, so it is already a
	// legal Loot-order target for area-forage the same as any other carryable, and needs no changes
	// of its own for another goblin to pick it up.
	const int32 Value = PersonalPurse;
	PersonalPurse = 0;
	return Value;
}

void AGSHordeGoblin::HandleDamagedForFrenzy(AActor* Attacker, float Damage)
{
	if (UGSHordeSubsystem* Horde = UGSHordeSubsystem::Get(this))
	{
		if (Attacker && IsHostileTo(Attacker))
		{
			Horde->RegisterThreat(Attacker);
		}
	}
}

void AGSHordeGoblin::HandleSummonerDealtDamage(AActor* Victim)
{
	// "anything you attack ... gets swarmed automatically" (§2.5). The hostility test matters: the
	// player's own torch fire damages his horde, and without it a goblin singed by friendly fire
	// would put its neighbour on the threat list.
	if (UGSHordeSubsystem* Horde = UGSHordeSubsystem::Get(this))
	{
		if (Victim && IsHostileTo(Victim))
		{
			Horde->RegisterThreat(Victim);
		}
	}
}

void AGSHordeGoblin::HandleSummonerDamaged(AActor* Attacker, float Damage)
{
	if (UGSHordeSubsystem* Horde = UGSHordeSubsystem::Get(this))
	{
		if (Attacker && IsHostileTo(Attacker))
		{
			Horde->RegisterThreat(Attacker);
		}
	}
}
