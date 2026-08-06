// The carry state (GDD §4 universal kit, §9 loot couriers): loot sacks, pigs, two weightless
// chickens. Owns WHAT IS CARRIED and the movement/combat consequences; picking up and putting
// down route through the interact channel like every other verb. Slows movement by a per-cargo
// multiplier and holds State.Carrying so attack abilities can block themselves (the chicken's
// one-handed-fighting exemption is a per-cargo bCargoAllowsCombat flag, not a special case in
// combat code). Replicates the cheap root state only (single-player slice, co-op-ready).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSCarryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnCarryChanged, AActor*, CarriedActor);

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSCarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSCarryComponent();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Carry")
	bool IsCarrying() const { return CarriedActor != nullptr; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Carry")
	AActor* GetCarriedActor() const { return CarriedActor; }

	/** Attach Cargo to the carry socket, apply the speed penalty, raise State.Carrying.
	 *  SpeedMultiplier and bAllowsCombat come from the cargo (sack 0.65, pig 0.6 squirming,
	 *  chicken 1.0 + combat allowed — placeholders for the tuning pass). */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Carry")
	bool PickUp(AActor* Cargo, float SpeedMultiplier = 0.65f, bool bAllowsCombat = false);

	/** Detach at the character's feet (courier drop, death drop, deliberate put-down). */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Carry")
	AActor* Drop();

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Carry")
	FName CarrySocketName = TEXT("CarrySocket");

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Carry")
	FGSOnCarryChanged OnCarryChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void ApplyCarryTag(bool bCarrying);
	void ApplySpeed(float Multiplier);

	UPROPERTY(Replicated)
	TObjectPtr<AActor> CarriedActor;

	float ActiveSpeedMultiplier = 1.f;
	bool bCargoAllowsCombat = false;
};
