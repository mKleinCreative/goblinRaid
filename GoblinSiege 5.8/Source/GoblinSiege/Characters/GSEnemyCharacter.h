// Defender pawn (Militia/Archer/Knight and future Elf/Dwarf archetypes). Archetypes are data,
// not classes: one pawn class + a GSRaceDataAsset row picked by ArchetypeRowName (design doc §6,
// "formation and numbers, not individual power"). Reconstructed 2026-07-19 to match the surviving
// GSEnemyCharacter.cpp and its callers (GSAIControllerBase, GSSpawnerActor, GSDamageExecCalculation).
#pragma once

#include "CoreMinimal.h"
#include "Characters/GSCharacterBase.h"
#include "GSEnemyCharacter.generated.h"

class UGSRaceDataAsset;

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

protected:
	virtual void BeginPlay() override;

	/** Which race this defender belongs to - set on level-placed instances or via
	 *  InitializeFromArchetype for spawner-produced ones. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Enemy")
	TObjectPtr<UGSRaceDataAsset> RaceData;

	/** Key into RaceData->Archetypes, e.g. "Militia", "Archer", "Knight". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Enemy")
	FName ArchetypeRowName = TEXT("Militia");
};
