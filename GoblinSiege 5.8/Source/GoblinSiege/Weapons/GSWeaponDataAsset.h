// Weapon-as-data: one asset per weapon kit (Dual Daggers, Great-club, Shadow Staff, Blood Staff).
// Equip/swap is a data operation on UGSWeaponComponent - no per-weapon pawn classes (design doc
// §5). Reconstructed 2026-07-19.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GSWeaponDataAsset.generated.h"

class UGameplayEffect;
class UGameplayAbility;

UCLASS(BlueprintType)
class GOBLINSIEGE_API UGSWeaponDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon")
	FText DisplayName;

	/** One Damage.* tag (GSGameplayTags) - the column this weapon uses in the race matchup matrix. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon")
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon")
	float BaseDamage = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon")
	float AttackCooldownSeconds = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon")
	float AttackRange = 100.f;

	/** Per-weapon turn-rate identity (Brute turns like a barge - design doc "Turn rate"). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon")
	float TurnRateRadPerSec = 8.f;

	/** Sets the class's Health/Armor/MoveSpeed baseline on equip (Slasher 110 HP fast, Brute
	 *  140 HP slow, Shaman 90 HP - race-design-goblins.md class table). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Init")
	TSubclassOf<UGameplayEffect> InitialAttributesEffect;

	/** The kit: light/heavy attack + the "E" ability, granted on equip and revoked on swap. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Init")
	TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;

	// --- Ranged mode (the Slasher's dagger⇄bow toggle; template for future flex weapons) ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|RangedMode")
	bool bHasRangedMode = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|RangedMode",
		meta = (EditCondition = "bHasRangedMode"))
	FGameplayTag RangedDamageTypeTag;

	/** Ranged trades damage-per-second for range/safety (race-design-goblins.md swap rule). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|RangedMode",
		meta = (EditCondition = "bHasRangedMode"))
	float RangedBaseDamage = 6.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|RangedMode",
		meta = (EditCondition = "bHasRangedMode"))
	float RangedAttackCooldownSeconds = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|RangedMode",
		meta = (EditCondition = "bHasRangedMode"))
	float RangedAttackRange = 900.f;
};
