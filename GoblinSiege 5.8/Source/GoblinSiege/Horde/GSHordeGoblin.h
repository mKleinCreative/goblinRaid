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
	// AACFCharacter has NO default constructor - it takes an FObjectInitializer (ACFCharacter.h:54),
	// so the whole chain must pass one down (#223).
	AGSHordeGoblin(const FObjectInitializer& ObjectInitializer);

	const UGSRaceDataAsset* GetRaceData() const { return RaceData; }
	FName GetArchetypeRowName() const { return ArchetypeRowName; }

	/**
	 * THIS GOBLIN'S PLACE IN THE FOLLOW FORMATION, AND IT NEVER MOVES (#239).
	 *
	 * The slot used to be the goblin's INDEX in the summoner's roster array, recomputed on every
	 * query. UGSHordeSubsystem::RemoveFromActive compacts that array with RemoveAll, so the moment
	 * one goblin died every goblin behind it inherited the slot in front - and the whole horde
	 * shuffled forward one place at once. A death is supposed to leave a gap, not reorder the band.
	 *
	 * Assigned once, when the goblin joins the roster, as the lowest slot no living sibling holds.
	 * So a casualty frees its slot, everyone else keeps theirs, and the next summon fills the hole.
	 */
	int32 GetFollowSlot() const { return FollowSlot; }
	void SetFollowSlot(int32 InSlot) { FollowSlot = InSlot; }

	/**
	 * A gold-type item looted in place (BTTask_LootInPlace) - a chest, a coin pouch - banks HERE
	 * instead of straight to UGSScoreSubsystem (2026-08-30, Michael: "they only return with loot if
	 * it's livestock, otherwise accumulate the gold on themselves as their personal purse"). Livestock
	 * is unaffected: it goes through UGSCarryComponent + GSLootBankComponent's existing
	 * carry-and-deliver path, which still banks to the score subsystem directly on delivery, same as
	 * before this ticket.
	 *
	 * Server-authoritative. Two things consume it, for two different endings (2026-08-30, Michael:
	 * "keeps that money with them, until they either die... or they make it back to spawn and bank
	 * it"): ConsumeLootSackDropValue reads-and-resets it on death, for AGSCharacterBase::HandleDeath
	 * to hand to SpawnLootSack as a recoverable pouch; BankPersonalPurse reads-and-resets it on a live
	 * delivery, for UGSLootBankComponent::BankPersonalPurse to add straight to the score subsystem.
	 * Kept as two separate methods rather than one, despite the identical bodies, because they answer
	 * different questions (what does dying drop vs. what does arriving bank) and the day one of them
	 * needs to diverge - a partial death-drop, say - sharing one method would be the wrong coupling.
	 */
	void AddToPersonalPurse(int32 Value) { PersonalPurse += Value; }

	/** See AddToPersonalPurse's comment for why this is not ConsumeLootSackDropValue. */
	int32 BankPersonalPurse() { const int32 Value = PersonalPurse; PersonalPurse = 0; return Value; }

protected:
	virtual void BeginPlay() override;
	virtual void HandleDeath() override;
	virtual int32 ConsumeLootSackDropValue() override;

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
	/** See GetFollowSlot. INDEX_NONE until the subsystem assigns one. */
	int32 FollowSlot = INDEX_NONE;

	/** See AddToPersonalPurse / ConsumeLootSackDropValue. Not replicated - nothing client-side reads
	 *  it, and it only ever changes and is consumed on the server. */
	int32 PersonalPurse = 0;

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
