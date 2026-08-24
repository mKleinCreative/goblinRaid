// Walk over it and you have arrows (ruling 50, #279).
//
// WHY THIS EXISTS RATHER THAN ONE OF THE THREE THINGS THAT LOOK LIKE IT:
//
// - NOT ACF's AACFPickup. Its bPickOnOverlap is honoured only inside
//   AACFPickup::OnInteractableRegisteredByPawn, whose sole caller in the whole plugin is ACF's OWN
//   UACFInteractionComponent - and we use UGSInteractionComponent. Its PickUpCapsule is created in
//   the constructor and never attached to the root and never bound to an overlap delegate. A
//   BP_ACFPickup dropped in a level does nothing and LOGS NOTHING. This was verified in the plugin
//   source before the decision, and recorded in the ledger, because it costs a session to rediscover.
//
// - NOT UGSCarryComponent. Carrying is one-object-in-hands and applies State.Carrying, which
//   UGSGA_BowShot blocks on. A quiver you cannot fight while holding is not a quiver.
//
// - NOT a new Interact.* verb on UGSInteractionComponent. That channel is hold-to-channel and
//   ABORTS ON DAMAGE, which is exactly wrong for restocking in the middle of a fight.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSAmmoPickup.generated.h"

class UACFItem;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class GOBLINSIEGE_API AGSAmmoPickup : public AActor
{
	GENERATED_BODY()

public:
	AGSAmmoPickup();

protected:
	virtual void BeginPlay() override;

	/** What this bundle gives. Null is a no-op that says so once rather than silently doing nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Ammo")
	TSubclassOf<UACFItem> ArrowItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Ammo", meta = (ClampMin = "1"))
	int32 ArrowCount = 10;

	/** Generous on purpose: this is a walk-over, not a precision pickup, and the player is usually
	 *  moving through it rather than at it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Ammo", meta = (ClampMin = "10.0"))
	float PickupRadius = 90.f;

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Ammo")
	TObjectPtr<UStaticMeshComponent> BundleMesh;

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Ammo")
	TObjectPtr<USphereComponent> PickupSphere;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	/**
	 * Hand the arrows over. Returns true if the count actually moved.
	 *
	 * IT CHECKS. AddItemToInventoryByClass returns void, and ACF's Internal_AddItem returns -1 with
	 * NO LOG when the add is refused by the weight budget or the slot cap. Without a before/after
	 * read, a full player walking over a bundle would see it vanish and gain nothing, and there
	 * would be no line anywhere saying why.
	 */
	bool TryGiveTo(APawn* Pawn);
};
