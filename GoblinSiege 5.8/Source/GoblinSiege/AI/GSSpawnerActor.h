// Shared base for every race's spawner building - Human Barracks (stone, not flammable, 220 HP,
// melee/ranged only), Elf Watch-Station (wooden, highly flammable, meant to die to a single
// torch), Dwarf Bunker/Forge (fire-resistant, wants explosives). The "destroy to silence" pattern
// and alarm-tier-driven spawn escalation are identical across all three; only the data
// (HP/flammability/spawn table) and visuals differ, so this is a single base class with race-
// specific data assets, per race-design-humans.md's "Roadmap notes for the Unreal build".
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Alarm/GSAlarmTypes.h"
#include "GSSpawnerActor.generated.h"

class UGSFlammableComponent;
class UGSRaceDataAsset;
class AGSEnemyCharacter;
class AGSGameState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSpawnerSilenced);

UCLASS(Abstract)
class GOBLINSIEGE_API AGSSpawnerActor : public AActor
{
	GENERATED_BODY()

public:
	AGSSpawnerActor();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Spawner")
	bool IsSilenced() const { return bIsSilenced; }

	/** Server-only. Melee/ranged damage path (Barracks, per race-design-humans.md, "must be
	 *  melee'd/ranged down"). Fire damage instead goes through the optional FlammableComponent. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Spawner")
	void ApplyStructuralDamage(float Amount);

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Spawner")
	FOnSpawnerSilenced OnSpawnerSilenced;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Which archetype(s) this spawner produces and how often - keyed into the owning RaceData's
	 *  Archetypes map (e.g. "Militia"/"Archer" for a Barracks; "Scout" then throttled "Ranger" for
	 *  a Watch-Station per race-design-elves.md's partial-damage-throttle behavior). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Spawner")
	TObjectPtr<UGSRaceDataAsset> RaceData;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Spawner")
	TArray<FName> SpawnableArchetypeRows;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Spawner")
	TSubclassOf<AGSEnemyCharacter> EnemyCharacterClass;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Spawner|Tuning")
	float BaseSpawnIntervalSeconds = 9.f; // Barracks default per design doc §5

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Spawner|Tuning")
	float StructuralHealth = 220.f; // Barracks default per design doc §5

	/** If true, this building can also be ignited (Watch-Station); Barracks/Bunker leave this off
	 *  and rely on ApplyStructuralDamage / high FireResistance instead. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Spawner")
	bool bHasFlammableComponent = false;

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Spawner", meta = (EditCondition = "bHasFlammableComponent"))
	TObjectPtr<UGSFlammableComponent> FlammableComponent;

	/** Destroying a spawner doesn't reduce Alarm but raises it briefly - "a Barracks falling is
	 *  loud" (design doc §5). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Spawner|Tuning")
	float AlarmSpikeOnDestroy = 8.f;

	UFUNCTION()
	virtual void HandleFlammableBurnedDown();

	UFUNCTION()
	virtual void HandleHordeWaveTriggered(EGSReinforcementTier NewTier);

	UFUNCTION()
	virtual void HandleDistrictRazedChanged(bool bRazed);

	void SpawnUnit();
	void Silence();
	void StartSpawnTimer();

	float CurrentStructuralHealth = 0.f;
	bool bIsSilenced = false;
	EGSReinforcementTier CurrentTier = EGSReinforcementTier::Tier0_Baseline;
	FTimerHandle SpawnTimerHandle;
};
