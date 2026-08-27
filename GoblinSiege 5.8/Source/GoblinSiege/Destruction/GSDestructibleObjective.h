// Destructible objective: a thing that is finished when it comes apart.
//
// #189's WARNING IS NOW RESOLVED (2026-08-25, #317). This class used to gate its fracture behind
// UGSFlammableComponent::OnBurnedDown, so a statue completed by BURNING - contradicting GDD 2.8,
// "The statue is the one target that doesn't burn: it has to be brought down, stone on stone."
// Michael settled it: "we don't want to burn statues, we break them with a grapple, we break and
// destroy buildings with a torch. the commonality is they get destroyed."
//
// So the burn trigger is gone. This objective is now finished by the CRUMBLE, whatever caused it -
// a rope on a monument, a torch on a building. It listens rather than decides, which is why the
// binding is to UGSCrumbleComponent::OnCrumbled and not to any particular way of dying.
//
// The old fracture code here was also simply broken, and had been since it was written: no
// authority guard, SetSimulatePhysics with no ObjectType, and no RemoveAllAnchors - two of the
// three traps its own module documents at length. It never worked, in single player, before co-op
// was ever a question. UGSCrumbleComponent owns that sequence now, in one place.
// Reconstructed 2026-07-19.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSDestructibleObjective.generated.h"

class UGSFlammableComponent;
class UGSBurnFXComponent;
class UGeometryCollectionComponent;
class UGSCrumbleComponent;

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

	/** Bound to the crumble, not to any one cause of it. Fires on every machine, so bDestroyed and
	 *  the objective delegate land on clients too without either being replicated. */
	UFUNCTION()
	void HandleCrumbled();

	/** Owns the release: swap, promote to dynamic, un-anchor, shove. See UGSCrumbleComponent. */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Objective")
	TObjectPtr<UGSCrumbleComponent> CrumbleComponent;

	/** The fractured objective mesh - dormant (no simulation) until it is released. */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Objective")
	TObjectPtr<UGeometryCollectionComponent> GeometryCollectionComponent;

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Objective")
	TObjectPtr<UGSFlammableComponent> FlammableComponent;

	/**
	 * 2026-07-31, Michael's two rulings off the field-fire PIE pass: fire has to leave a mark, and
	 * the smoke has to stay behind once the fire has moved on. An objective that burns to a Chaos
	 * heap of clean, un-scorched timber tells a player sweeping back through the village that
	 * nothing ever happened here - the raid has no memory. This rides the FlammableComponent
	 * above, chars the mesh as the burn runs, and leaves a persistent smoulder on the wreckage.
	 */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Objective")
	TObjectPtr<UGSBurnFXComponent> BurnFXComponent;

	/** Not replicated, and it does not need to be: HandleCrumbled runs on every machine because the
	 *  release that triggers it is what replicates. */
	bool bDestroyed = false;
};
