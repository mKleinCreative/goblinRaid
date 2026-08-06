// Defender pawn (Militia/Archer/Knight and future Elf/Dwarf archetypes). Archetypes are data,
// not classes: one pawn class + a GSRaceDataAsset row picked by ArchetypeRowName (design doc §6,
// "formation and numbers, not individual power"). Reconstructed 2026-07-19 to match the surviving
// GSEnemyCharacter.cpp and its callers (GSAIControllerBase, GSSpawnerActor, GSDamageExecCalculation).
#pragma once

#include "CoreMinimal.h"
#include "Characters/GSCharacterBase.h"
#include "GSEnemyCharacter.generated.h"

class UGSRaceDataAsset;
class UGameplayAbility;

UCLASS()
class GOBLINSIEGE_API AGSEnemyCharacter : public AGSCharacterBase
{
	GENERATED_BODY()

public:
	AGSEnemyCharacter();

	/** Server-only. Applies the archetype row's stats (HP/Armor/MoveSpeed/TurnRate) to this pawn.
	 *  Called by GSSpawnerActor after spawn, or from BeginPlay for level-placed enemies. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Enemy")
	void InitializeFromArchetype(UGSRaceDataAsset* InRaceData, FName InArchetypeRowName);

	const UGSRaceDataAsset* GetRaceData() const { return RaceData; }
	FName GetArchetypeRowName() const { return ArchetypeRowName; }

	// ---- combat verbs (2026-08-04) -------------------------------------------------------
	// Deliberately the SAME abilities the player runs, not an AI-only reimplementation. If a
	// defender's swing were its own code path it would drift from the player's within a week -
	// different reach, different windup, different feel - and every combat tuning pass would have
	// to be done twice. These just activate the granted ability and report whether it took.
	//
	// Which verbs a given character HAS is data, not code: allied goblins get light and heavy,
	// humans additionally get the guard break (Michael's ruling, 2026-08-04). Leave a class unset
	// and that character simply cannot do that thing.

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Enemy|Combat")
	bool TryLightAttack();

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Enemy|Combat")
	bool TryHeavyAttack();

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Enemy|Combat")
	bool TryGuardBreak();

	/** Raises the guard and leaves it up until StopBlocking. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Enemy|Combat")
	bool StartBlocking();

	/** Drops the guard. Cancels by tag, never by class, so a swing in flight survives. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Enemy|Combat")
	void StopBlocking();

	/** True when this character has a guard break available - the one verb allied goblins lack. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Enemy|Combat")
	bool CanGuardBreak() const { return GuardBreakAbilityClass != nullptr; }

protected:
	virtual void BeginPlay() override;

	/** Which race this defender belongs to - set on level-placed instances or via
	 *  InitializeFromArchetype for spawner-produced ones. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Enemy")
	TObjectPtr<UGSRaceDataAsset> RaceData;

	/** Key into RaceData->Archetypes, e.g. "Militia", "Archer", "Knight". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Enemy")
	FName ArchetypeRowName = TEXT("Militia");

	/** UGSGA_SwordLight Blueprint child - the multi-stage combo. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Enemy|Combat")
	TSubclassOf<UGameplayAbility> LightAttackAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Enemy|Combat")
	TSubclassOf<UGameplayAbility> HeavyAttackAbilityClass;

	/** Left unset on allied goblins on purpose - the guard break is a human answer to turtling,
	 *  and giving it to everyone would make blocking worthless for both sides. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Enemy|Combat")
	TSubclassOf<UGameplayAbility> GuardBreakAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Enemy|Combat")
	TSubclassOf<UGameplayAbility> BlockAbilityClass;

private:
	/** Grants a class if set and returns the spec, so BeginPlay reads as a list rather than four
	 *  copies of the same null check. */
	void GrantIfSet(TSubclassOf<UGameplayAbility> AbilityClass);

	bool TryActivate(TSubclassOf<UGameplayAbility> AbilityClass);
};
