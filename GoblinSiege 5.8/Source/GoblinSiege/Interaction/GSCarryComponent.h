// Carry state (GDD §8): the goblin who is holding something moves slower and cannot swing. Both
// halves are one rule - State.Carrying is applied for exactly as long as the object is held, and
// attack abilities block on it. Pick-up and put-down are not owned here: they are channels, and
// they route through UGSInteractionComponent like every other verb.
#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "Templates/SubclassOf.h"
#include "GSCarryComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnCarriedActorChanged, AActor*, CarriedActor);

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSCarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSCarryComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Called by UGSInteractionComponent when a carryable's channel completes. SERVER ONLY - the
	 *  carried actor is replicated state, and a client that wrote it would be overwritten by the next
	 *  update while the server never learned anything happened. Clients pick it up via OnRep. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Carry")
	bool StartCarry(AActor* Object);

	/** Sets the object down in front of the carrier. Returns whatever was dropped, so the caller can
	 *  hand it straight to an extraction point. SERVER ONLY, same reasoning as StartCarry. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Carry")
	AActor* PutDown();

	/**
	 * Destroy what is being carried instead of putting it down.
	 *
	 * Added for drowning (Michael, 2026-08-06: "loss of whatever's on you"). Dying on land drops your
	 * cargo where you fell and you can go back for it; drowning takes it with you. Without this the
	 * normal death path would leave a sack floating in deep water - visible loot the player may have
	 * no way to reach, which reads as a bug rather than a penalty.
	 *
	 * Safe to call when carrying nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Carry")
	void DestroyCarried();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Carry")
	bool IsCarrying() const { return IsValid(CarriedActor); }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Carry")
	AActor* GetCarriedActor() const { return CarriedActor; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Carry")
	float GetPutDownSeconds() const { return PutDownSeconds; }

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Carry")
	FGSOnCarriedActorChanged OnCarriedActorChanged;

protected:
	/** A corpse should not keep the sack glued to its hand. Bound to AGSCharacterBase::OnDied. */
	UFUNCTION()
	void HandleOwnerDied();

	UFUNCTION()
	void OnRep_CarriedActor(AActor* OldCarried);

	void AttachCarried();
	void ApplyCarryState();
	void ClearCarryState();
	void SuppressCarriedCollision(AActor* Object);
	void RestoreCarriedCollision(AActor* Object);
	FVector FindDropLocation() const;
	UAbilitySystemComponent* GetOwnerASC() const;

	/** Socket on the carrier's mesh. Falls back to the mesh origin if the socket does not exist. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Carry")
	FName CarrySocketName = TEXT("CarrySocket");

	/** Fine-tune on top of the socket, so one socket serves a sack, a barrel, and a body. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Carry")
	FTransform CarryRelativeTransform = FTransform::Identity;

	/** Hands full is supposed to cost you something (GDD §8). Fed to UGSGE_MoveSpeedScalar as the
	 *  SetByCaller magnitude rather than written onto MaxWalkSpeed, so it composes with a block slow
	 *  or a future root instead of racing them. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Carry", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float CarrySpeedMultiplier = 0.55f;

	/** C++-defaulted to UGSGE_MoveSpeedScalar; overridable so a heavier cargo class can supply its own. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Carry")
	TSubclassOf<UGameplayEffect> CarrySlowEffectClass;

	/** Put-down channel length, used by UGSInteractionComponent when the hands are full. Shorter than
	 *  a pick-up: dropping a thing is easier than lifting it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Carry", meta = (ClampMin = "0.0"))
	float PutDownSeconds = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Carry", meta = (ClampMin = "0.0"))
	float DropForwardOffset = 90.f;

	/** Trace down on drop so the object lands on the floor rather than at hip height. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Carry")
	bool bDropTraceToGround = true;

	/** Cheap replicated root state (multiplayer posture): a partner sees what you are hauling, and
	 *  nothing about it is predicted. */
	UPROPERTY(ReplicatedUsing = OnRep_CarriedActor, BlueprintReadOnly, Category = "GoblinSiege|Carry")
	TObjectPtr<AActor> CarriedActor;

private:
	bool bCarryStateApplied = false;
	bool bCarriedSimulatedPhysics = false;

	/** Server-side handle to the active slow. Clients never hold one - they see the slow because
	 *  MoveSpeedMultiplier is a replicated attribute. */
	FActiveGameplayEffectHandle CarrySlowHandle;
};
