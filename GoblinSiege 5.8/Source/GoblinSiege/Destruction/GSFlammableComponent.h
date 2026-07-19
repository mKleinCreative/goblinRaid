// Attach to anything that can burn. FireResistance implements material hardness on the burn axis
// (GDD §11.0): 0 = tinder, 1 = effectively immune (Dwarf Bunker). Reconstructed 2026-07-19 to
// match surviving callers (GSSpawnerActor binds OnBurnedDown; GSFireVolume/BTTask_Firefight call
// Ignite/Extinguish/IsBurning).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSFlammableComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnIgnited);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnExtinguished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnBurnedDown);

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSFlammableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSFlammableComponent();

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Fire")
	void Ignite();

	/** Firefighting defenders (BTTask_Firefight) and rain-of-the-future call this. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Fire")
	void Extinguish();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Fire")
	bool IsBurning() const { return bIsBurning; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Fire")
	float GetBurnProgress01() const
	{
		return BurnDurationSeconds > 0.f ? FMath::Clamp(BurnedSeconds / BurnDurationSeconds, 0.f, 1.f) : 0.f;
	}

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Fire")
	FGSOnIgnited OnIgnited;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Fire")
	FGSOnExtinguished OnExtinguished;

	/** Fired once when the burn completes - granaries collapse, Watch-Stations die, spawners
	 *  silence off this. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Fire")
	FGSOnBurnedDown OnBurnedDown;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void BurnTick();

	/** 0 = tinder, 1 = immune. Scales burn progress rate, per-material (GDD §11.0). */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Fire", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FireResistance = 0.f;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Fire")
	float BurnDurationSeconds = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Tuning")
	float BurnTickInterval = 0.5f;

	bool bIsBurning = false;
	float BurnedSeconds = 0.f;
	FTimerHandle BurnTimerHandle;
};
