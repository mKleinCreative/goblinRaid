// Granary-style burn objective: Chaos fracture gated behind burn-complete - the collapse moment
// (design doc §7). Tagged "Objective.Granary" so GSObjective_BurnGranaries counts what it finds.
// Reconstructed 2026-07-19.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSDestructibleObjective.generated.h"

class UGSFlammableComponent;
class UGSBurnFXComponent;
class UGeometryCollectionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnObjectiveDestroyed);

UCLASS()
class GOBLINSIEGE_API AGSDestructibleObjective : public AActor
{
	GENERATED_BODY()

public:
	AGSDestructibleObjective();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Objective")
	bool IsDestroyed() const { return bDestroyed; }

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Objective")
	FGSOnObjectiveDestroyed OnObjectiveDestroyed;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleBurnedDown();

	/** The fractured granary mesh - dormant (no simulation) until the burn completes. */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Objective")
	TObjectPtr<UGeometryCollectionComponent> GeometryCollectionComponent;

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Objective")
	TObjectPtr<UGSFlammableComponent> FlammableComponent;

	/**
	 * 2026-07-31, Michael's two rulings off the field-fire PIE pass: fire has to leave a mark, and
	 * the smoke has to stay behind once the fire has moved on. A granary that burns to a Chaos
	 * heap of clean, un-scorched timber tells a player sweeping back through the village that
	 * nothing ever happened here - the raid has no memory. This rides the FlammableComponent
	 * above, chars the mesh as the burn runs, and leaves a persistent smoulder on the wreckage.
	 */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Objective")
	TObjectPtr<UGSBurnFXComponent> BurnFXComponent;

	bool bDestroyed = false;
};
