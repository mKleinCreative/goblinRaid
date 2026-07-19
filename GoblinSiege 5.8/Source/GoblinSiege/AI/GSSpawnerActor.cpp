#include "AI/GSSpawnerActor.h"
#include "Destruction/GSFlammableComponent.h"
#include "Combat/GSRaceDataAsset.h"
#include "Characters/GSEnemyCharacter.h"
#include "Core/GSGameState.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AGSSpawnerActor::AGSSpawnerActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// Constructed conditionally per-instance via bHasFlammableComponent since not every subclass
	// wants one (Barracks doesn't; Watch-Station and, arguably, a heavily-resisted Bunker do).
	FlammableComponent = CreateDefaultSubobject<UGSFlammableComponent>(TEXT("FlammableComponent"));
}

void AGSSpawnerActor::BeginPlay()
{
	Super::BeginPlay();

	CurrentStructuralHealth = StructuralHealth;

	if (bHasFlammableComponent && FlammableComponent)
	{
		FlammableComponent->OnBurnedDown.AddDynamic(this, &AGSSpawnerActor::HandleFlammableBurnedDown);
	}
	else if (FlammableComponent)
	{
		FlammableComponent->DestroyComponent();
		FlammableComponent = nullptr;
	}

	if (AGSGameState* GS = Cast<AGSGameState>(UGameplayStatics::GetGameState(this)))
	{
		GS->OnHordeWaveTriggered.AddDynamic(this, &AGSSpawnerActor::HandleHordeWaveTriggered);
		GS->OnDistrictRazedChanged.AddDynamic(this, &AGSSpawnerActor::HandleDistrictRazedChanged);
	}

	if (HasAuthority())
	{
		StartSpawnTimer();
	}
}

void AGSSpawnerActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void AGSSpawnerActor::StartSpawnTimer()
{
	if (bIsSilenced)
	{
		return;
	}

	// Higher reinforcement tiers spawn faster - a simple 10%-per-tier reduction; tune per race.
	const float TierScalar = 1.f - (0.1f * static_cast<uint8>(CurrentTier));
	const float Interval = FMath::Max(1.5f, BaseSpawnIntervalSeconds * TierScalar);

	GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &AGSSpawnerActor::SpawnUnit, Interval, true);
}

void AGSSpawnerActor::SpawnUnit()
{
	if (bIsSilenced || !HasAuthority() || !EnemyCharacterClass || SpawnableArchetypeRows.Num() == 0 || !RaceData)
	{
		return;
	}

	const FName ChosenRow = SpawnableArchetypeRows[FMath::RandRange(0, SpawnableArchetypeRows.Num() - 1)];

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	if (AGSEnemyCharacter* NewEnemy = GetWorld()->SpawnActor<AGSEnemyCharacter>(
			EnemyCharacterClass, GetActorLocation(), GetActorRotation(), SpawnParams))
	{
		NewEnemy->InitializeFromArchetype(RaceData, ChosenRow);
		// GSAIControllerBase possesses automatically via AGameModeBase::SpawnDefaultController
		// (AI characters set AutoPossessAI = PlacedInWorldOrSpawned) and picks its Behavior Tree
		// from the archetype's RoleTag - see AI module.
	}
}

void AGSSpawnerActor::ApplyStructuralDamage(float Amount)
{
	if (bIsSilenced || !HasAuthority() || Amount <= 0.f)
	{
		return;
	}

	CurrentStructuralHealth = FMath::Max(0.f, CurrentStructuralHealth - Amount);
	if (CurrentStructuralHealth <= 0.f)
	{
		Silence();
	}
}

void AGSSpawnerActor::HandleFlammableBurnedDown()
{
	Silence();
}

void AGSSpawnerActor::Silence()
{
	if (bIsSilenced)
	{
		return;
	}
	bIsSilenced = true;

	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	OnSpawnerSilenced.Broadcast();

	if (HasAuthority())
	{
		if (AGSGameState* GS = Cast<AGSGameState>(UGameplayStatics::GetGameState(this)))
		{
			// "Destroying it doesn't reduce the current Alarm meter, but does raise it briefly"
			// (design doc §5) - deliberate risk/reward, not a reward for killing the spawner.
			GS->AddAlarm(AlarmSpikeOnDestroy, EGSAlarmSource::BarracksDestroyed);
		}
	}

	// Chaos fracture / collapse visuals for the building mesh are left to a Blueprint event bound
	// to OnSpawnerSilenced, matching AGSDestructibleObjective's pattern for granaries.
}

void AGSSpawnerActor::HandleHordeWaveTriggered(EGSReinforcementTier NewTier)
{
	CurrentTier = NewTier;
	if (!bIsSilenced && HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		StartSpawnTimer();
	}
}

void AGSSpawnerActor::HandleDistrictRazedChanged(bool bRazed)
{
	// "A razed city stops producing defenders ... spawners go quiet" (design doc §4). External
	// relief patrols are a separate, dedicated spawn-point actor (not this class) that only
	// activates once bRazed is true - kept separate because patrols march in from map edges
	// rather than emitting from a building.
	if (bRazed && HasAuthority())
	{
		Silence();
	}
}
