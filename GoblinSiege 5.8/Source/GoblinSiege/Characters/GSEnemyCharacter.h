// Defender pawn (Militia/Archer/Knight and future Elf/Dwarf archetypes). Archetypes are data,
// not classes: one pawn class + a GSRaceDataAsset row picked by ArchetypeRowName (design doc §6,
// "formation and numbers, not individual power"). Reconstructed 2026-07-19 to match the surviving
// GSEnemyCharacter.cpp and its callers (GSAIControllerBase, GSSpawnerActor, GSDamageExecCalculation).
#pragma once

#include "CoreMinimal.h"
#include "Characters/GSCharacterBase.h"
// For EGSWeaponSlot, which DefaultSlot holds by value - a UENUM used in a UPROPERTY cannot be
// forward-declared, so the whole header comes in and the component needs no forward decl.
#include "Weapons/GSWeaponComponent.h"
#include "GSEnemyCharacter.generated.h"

class UGSRaceDataAsset;
class UGSWeaponDataAsset;

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

	// The combat verbs (TryLightAttack / TryHeavyAttack / TryGuardBreak / Start+StopBlocking,
	// CanGuardBreak) and the four ability-class UPROPERTYs that back them moved DOWN to
	// AGSCharacterBase on 2026-08-07 (#069) so AGSHordeGoblin could have them without being
	// reparented to this class. They are inherited now, so callers are unchanged.
	//
	// MUST BE VERIFIED IN-EDITOR, not assumed: the four EditDefaultsOnly ability slots on the six
	// adversary Blueprints are expected to survive because UE resolves serialised properties by
	// NAME within the class hierarchy and the names did not change - only the class declaring
	// them moved one level up. That is the normal behaviour for hoisting a property to a parent,
	// but this project has been bitten before by "the reparent preserved everything" assumptions.
	// Open BP_CastleGuard01 after the build and confirm LightAttack/Heavy/GuardBreak/Block are
	// still assigned before trusting any PIE result. If they came back empty, #048 is the
	// precedent for a retroactive re-assignment pass.

protected:
	virtual void BeginPlay() override;

	/** Which race this defender belongs to - set on level-placed instances or via
	 *  InitializeFromArchetype for spawner-produced ones. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Enemy")
	TObjectPtr<UGSRaceDataAsset> RaceData;

	/** Key into RaceData->Archetypes, e.g. "Militia", "Archer", "Knight". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Enemy")
	FName ArchetypeRowName = TEXT("Militia");

	/**
	 * Lets a defender actually HOLD its weapon.
	 *
	 * Until 2026-08-09 UGSWeaponComponent existed only on AGSPlayerCharacter, so every guard in the
	 * game fought bare-handed - swinging, blocking and dying with nothing in their hands. The
	 * component is the same one the player uses, so a defender's sword goes through the same data
	 * asset, the same socket resolution and the same holster logic rather than a parallel path.
	 *
	 * Deliberately on AGSEnemyCharacter rather than hoisted to AGSCharacterBase: the player already
	 * creates its own in its constructor, and adding a second on the base would give him two. Moving
	 * his is a bigger change than this ticket, and AGSHordeGoblin wants the same treatment
	 * separately.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Enemy")
	TObjectPtr<UGSWeaponComponent> WeaponComponent;

	/** Equipped in BeginPlay. Leave unset and the defender is unarmed, which is the old behaviour
	 *  and correct for a civilian. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Enemy")
	TObjectPtr<UGSWeaponDataAsset> DefaultWeapon;

	/**
	 * Which hand the defender fights with, chosen once at BeginPlay.
	 *
	 * A defender has no weapon wheel, so the slot has to be decided for it. Sword is the honest
	 * default for a garrison; the archer sets Bow. Before this existed BeginPlay hardcoded Sword,
	 * which put Erika's bow on RangedHolsterSocket - `back_bow`, a socket the human skeleton does
	 * not have - so it would have attached at the actor root and floated at her feet.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Enemy")
	EGSWeaponSlot DefaultSlot = EGSWeaponSlot::Sword;
};
