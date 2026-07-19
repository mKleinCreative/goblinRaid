// One data asset per defender race (Human, Elf, Dwarf) plus one for Goblins themselves (all
// multipliers 1.0x - they're the baseline everything else is a deviation from, per
// race-design-goblins.md). Directly encodes the "Weapon Matchup Multipliers" table that appears
// at the bottom of every race-design-*.md doc, so a designer changing a number in the design doc
// has exactly one data asset to update in-editor, with no code change required.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GSRaceDataAsset.generated.h"

/** A single enemy archetype row - Militia/Archer/Knight, Scout/Ranger/Warden, Shieldbearer/
 *  Hammer-dwarf/Engineer. Deliberately data, not a class, per design doc's "generalists" and
 *  "formation and numbers, not individual power" philosophy - see GSEnemyCharacter's header comment. */
USTRUCT(BlueprintType)
struct FGSArchetypeDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float Health = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float Armor = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float MoveSpeed = 95.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float TurnRateRadPerSec = 6.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float Damage = 8.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float AttackCooldownSeconds = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float AttackRange = 32.f;

	/** e.g. "FrontlineFiller", "BacklineHarasser", "HeavyTank", "SupportAura" - selects Behavior Tree
	 *  and EQS query set in GSAIControllerBase. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	FGameplayTag RoleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	TSoftObjectPtr<class UBehaviorTree> BehaviorTree;
};

USTRUCT(BlueprintType)
struct FGSDamageMultiplierEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag DamageType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float Multiplier = 1.f;
};

UCLASS(BlueprintType)
class GOBLINSIEGE_API UGSRaceDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Race")
	FGameplayTag RaceTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Race")
	FText DisplayName;

	/** Keyed by archetype name, e.g. "Militia", "Archer", "Knight". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Race")
	TMap<FName, FGSArchetypeDefinition> Archetypes;

	/** The six-row (plus Blast) weapon-vs-race multiplier table from the race-design doc. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Race")
	TArray<FGSDamageMultiplierEntry> DamageMultipliers;

	/** Which building class this race's spawner is (Barracks / Watch-Station / Bunker) - see
	 *  GSSpawnerActor and each race doc's "Spawner Building" section. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Race")
	TSubclassOf<class AGSSpawnerActor> SpawnerClass;

	// Not UFUNCTION-exposed: UHT can't reflect a pointer to a USTRUCT as a Blueprint-callable
	// return value ("Inappropriate '*' on variable of type ... cannot have an exposed pointer to
	// this type"). This is a C++-only helper; current callers are GSAIControllerBase and
	// GSEnemyCharacter. If Blueprint access is ever needed, add a separate BlueprintCallable
	// bool FindArchetype(FName, FGSArchetypeDefinition& Out) overload instead of exposing this one.
	const FGSArchetypeDefinition* FindArchetype(FName ArchetypeRowName) const
	{
		return Archetypes.Find(ArchetypeRowName);
	}

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Race")
	float GetDamageMultiplier(FGameplayTag DamageType) const
	{
		for (const FGSDamageMultiplierEntry& Entry : DamageMultipliers)
		{
			if (Entry.DamageType == DamageType)
			{
				return Entry.Multiplier;
			}
		}
		return 1.f; // unlisted damage type vs this race defaults to baseline, matching the Human row's intent
	}
};
