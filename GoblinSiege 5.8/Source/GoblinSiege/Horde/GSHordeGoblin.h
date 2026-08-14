// One allied goblin from the horn's horde (GDD §2.5, repo export §5). Summoned by UGSGA_Horn
// through UGSHordeSubsystem, which owns the 20-goblin raid pool and the cap of 10 active.
//
// Deliberately NOT an AGSEnemyCharacter. Reparenting would have handed this class the combat verbs
// in one line, but AGSEnemyCharacter is what five separate checks in this codebase mean by
// "defender": GSFireVolume's FriendlyFireScalar, GSTargetingComponent's soft-lock candidate list,
// GSBuffAuraComponent's aura targets, and two more. A horde goblin that answered yes to those would
// stop burning in its own torch fire, would be soft-locked onto by its own summoner, and would be
// buffed by human captains - none of it failing loudly. The verbs were hoisted to AGSCharacterBase
// instead (#069, 2026-08-07).
//
// Stats are data, not code: RaceData + ArchetypeRowName, same as a defender, so the ~40 HP of the
// design doc lives in DA_Race_Goblin and not in a literal here.
#pragma once

#include "CoreMinimal.h"
#include "Characters/GSCharacterBase.h"
#include "GSHordeGoblin.generated.h"

class UGSRaceDataAsset;
class UGSCarryComponent;
class UGSWeaponComponent;
class UGSWeaponDataAsset;

UCLASS()
class GOBLINSIEGE_API AGSHordeGoblin : public AGSCharacterBase
{
	GENERATED_BODY()

public:
	AGSHordeGoblin();

	const UGSRaceDataAsset* GetRaceData() const { return RaceData; }
	FName GetArchetypeRowName() const { return ArchetypeRowName; }

protected:
	virtual void BeginPlay() override;
	virtual void HandleDeath() override;

	/** Which race this goblin belongs to. Supplies RaceTag (Race.Goblin) and the stat row, exactly
	 *  the way DA_Race_Human supplies the six human defenders. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Horde")
	TObjectPtr<UGSRaceDataAsset> RaceData;

	/** Key into RaceData->Archetypes. One row for now: the rank-and-file horde goblin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Horde")
	FName ArchetypeRowName = TEXT("HordeGoblin");

	/**
	 * Lets a summoned goblin actually hold its blade.
	 *
	 * Same component the player and the defenders (#097) carry, so the horde's sword goes through
	 * the same data asset and socket resolution rather than a third path. The goblin rig is the
	 * player's own (GOB_Scout_v2_Skeleton) and already has `hand_r_weapon`, so unlike the humans
	 * this attaches to a real socket rather than to a bare bone.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Horde")
	TObjectPtr<UGSWeaponComponent> WeaponComponent;

	/** Equipped in BeginPlay. Unset leaves the goblin bare-handed, which was the state of the entire
	 *  warband until now. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Horde")
	TObjectPtr<UGSWeaponDataAsset> DefaultWeapon;

	/**
	 * Lets a summoned goblin shoulder a sack, a pig or a rope chain - the courier half of §2.7.
	 *
	 * The same component the player has carried since the carry framework was written; until #141 it
	 * existed only on AGSPlayerCharacter, which is why "point a horde goblin at a pen and watch it
	 * waddle off with a pig" had no code path at all. Everything about carrying - the State.Carrying
	 * tag, the speed penalty, the attach, the drop trace, dropping cargo on death - is already
	 * server-authoritative in there; the two courier BT tasks only call StartCarry and PutDown.
	 *
	 * KNOWN COSMETIC GAP: UGSCarryComponent::CarrySocketName defaults to `CarrySocket`, which the
	 * goblin rig does not have, so the component falls back to the mesh origin and the sack rides at
	 * the goblin's feet rather than on its shoulder. That is art work (add the socket to
	 * GOB_Scout_v2_Skeleton), not a code fix, and it is ugly rather than broken.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Horde")
	TObjectPtr<UGSCarryComponent> CarryComponent;

private:
	/** Applies the archetype row's stats. A near-copy of AGSEnemyCharacter's version rather than a
	 *  shared one, because that class's copy also carries defender-only concerns; if a third caller
	 *  appears this belongs on AGSCharacterBase. */
	void InitializeFromArchetype();

	/** Frenzy, half one: something hit me. */
	UFUNCTION()
	void HandleDamagedForFrenzy(AActor* Attacker, float Damage);

	/** Frenzy, half two: my summoner hit something. Bound to the summoning pawn, not to self. */
	UFUNCTION()
	void HandleSummonerDealtDamage(AActor* Victim);

	/** Frenzy, half three: something hit my summoner. */
	UFUNCTION()
	void HandleSummonerDamaged(AActor* Attacker, float Damage);

	/** Whoever we bound the two summoner delegates to, so they can be unbound on death. */
	TWeakObjectPtr<AGSCharacterBase> BoundSummoner;
};
