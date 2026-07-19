// Weapon identity lives here - equip/swap is a data operation (design doc §5). Owns the
// Slasher's melee/ranged toggle (with the anti-cancel input lock) and the Blood Staff orb
// economy. Reconstructed 2026-07-19 to match the surviving GSPlayerCharacter.cpp caller.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSWeaponComponent.generated.h"

class UGSWeaponDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnOrbCountChanged, int32, NewCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnWeaponModeChanged, bool, bRangedMode);

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSWeaponComponent();

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Weapon")
	void EquipWeapon(UGSWeaponDataAsset* NewWeapon);

	/** Slasher dagger⇄bow toggle. Brief input lock so the swap can't be used as a frame-perfect
	 *  combat cancel (race-design-goblins.md weapon-swap rule). */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Weapon")
	void ToggleRangedMode();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Weapon")
	bool IsInRangedMode() const { return bRangedMode; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Weapon")
	UGSWeaponDataAsset* GetEquippedWeapon() const { return EquippedWeapon; }

	// --- Blood Staff orb economy (design doc §5: melee hits bank orbs, Blood Nova spends them) ---

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Weapon|BloodOrbs")
	void AddBloodOrb();

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Weapon|BloodOrbs")
	bool ConsumeBloodOrbs(int32 Count);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Weapon|BloodOrbs")
	int32 GetBloodOrbCount() const { return BloodOrbs; }

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Weapon")
	FGSOnOrbCountChanged OnOrbCountChanged;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Weapon")
	FGSOnWeaponModeChanged OnWeaponModeChanged;

protected:
	virtual void BeginPlay() override;

	void GrantAbilitiesFromWeapon();

	/** Starting kit - set on the character Blueprint; EquipWeapon() swaps at runtime. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Weapon")
	TObjectPtr<UGSWeaponDataAsset> EquippedWeapon;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Weapon|Tuning")
	float SwapInputLockSeconds = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Weapon|Tuning")
	int32 MaxBloodOrbs = 5;

	bool bRangedMode = false;
	bool bSwapLocked = false;
	int32 BloodOrbs = 0;
	FTimerHandle SwapLockTimerHandle;
};
