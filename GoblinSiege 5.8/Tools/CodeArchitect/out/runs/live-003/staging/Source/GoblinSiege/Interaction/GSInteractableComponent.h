// The advertised half of the hold-E framework (GDD §8): a chest, a well, a downed guard, or a
// crate becomes interactable by owning one of these. The VERB is a gameplay tag, never a subclass -
// loot / takedown / foul-well / extract are authored as data, so a fifth verb costs a tag rather
// than a class. This component owns advertisement plus the completion hook only; the channel
// lifecycle lives on UGSInteractionComponent.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GSInteractableComponent.generated.h"

class UGSInteractableComponent;

/** Fired once the channel matured. Loot grants, extraction banking, and takedown kills hook here. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSOnInteractionCompleted, UGSInteractableComponent*, Interactable, AActor*, Interactor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSOnInteractionAborted, UGSInteractableComponent*, Interactable, AActor*, Interactor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSOnInteractableAvailabilityChanged, UGSInteractableComponent*, Interactable, bool, bAvailable);

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSInteractableComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Eligibility as the interactable sees it: available, and nobody else already mid-channel on
	 *  me. Range and facing are the interactor's half (UGSInteractionComponent) - deliberately not
	 *  duplicated here. Override in Blueprint for verb-specific gates, e.g. a takedown that only
	 *  accepts a guard who has not noticed you. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GoblinSiege|Interaction")
	bool CanInteract(AActor* Interactor) const;
	virtual bool CanInteract_Implementation(AActor* Interactor) const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	FGameplayTag GetVerbTag() const { return VerbTag; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	float GetChannelSeconds() const { return ChannelSeconds; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	FText GetPromptText() const { return PromptText; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	bool IsAvailable() const { return bIsAvailable; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	bool IsCarryable() const { return bIsCarryable; }

	/** The point range and facing are measured against - the mouth of a chest, not its pivot. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	FVector GetInteractionLocation() const;

	/** Used by whichever system owns the verb: a looted chest turns itself off, a fouled well
	 *  re-opens after its cooldown. SERVER ONLY - clients receive it through OnRep. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Interaction")
	void SetAvailable(bool bNewAvailable);

	// ---- driven by UGSInteractionComponent, server side; not gameplay-facing -------------
	void NotifyChannelStarted(AActor* Interactor);
	void NotifyChannelAborted(AActor* Interactor);

	/** The payout. Server only. */
	void CompleteInteraction(AActor* Interactor);

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Interaction")
	FGSOnInteractionCompleted OnInteractionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Interaction")
	FGSOnInteractionAborted OnInteractionAborted;

	/** For the world prompt: a chest that goes dark should stop advertising itself. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Interaction")
	FGSOnInteractableAvailabilityChanged OnAvailabilityChanged;

protected:
	/** The effect. Empty in C++ on purpose: pouch grants, extraction banking, and takedown kills are
	 *  their own systems and hook in later - the framework only guarantees this fires exactly once,
	 *  on completion, and never on an abort. */
	UFUNCTION(BlueprintNativeEvent, Category = "GoblinSiege|Interaction")
	void ReceiveInteractionCompleted(AActor* Interactor);
	virtual void ReceiveInteractionCompleted_Implementation(AActor* Interactor);

	UFUNCTION()
	void OnRep_IsAvailable();

	/** Interact.Loot / Interact.Takedown / Interact.FoulWell / Interact.Carry. (Interact.Extract is
	 *  declared but unused - extraction auto-banks on a circle rather than channelling, 2026-08-04.) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Interaction")
	FGameplayTag VerbTag;

	/** Hold time. 0 is legal and still routes through the channel, so the HUD bar and the abort
	 *  rules stay in exactly one place. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Interaction", meta = (ClampMin = "0.0"))
	float ChannelSeconds = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Interaction")
	FText PromptText = NSLOCTEXT("GoblinSiege", "InteractPromptDefault", "Interact");

	/** Owner-space offset to the interaction point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Interaction")
	FVector InteractionOffset = FVector::ZeroVector;

	/** Cheap root state, replicated so a co-op partner sees a looted chest as looted. Nothing here
	 *  is predicted (multiplayer posture). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_IsAvailable, Category = "GoblinSiege|Interaction")
	bool bIsAvailable = true;

	/** Go unavailable on completion - right for loot and extract, wrong for a well you can foul
	 *  again next objective phase. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Interaction")
	bool bConsumeOnComplete = true;

	/** Completing hands the owning actor to the interactor's UGSCarryComponent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Interaction")
	bool bIsCarryable = false;

private:
	/** Whoever is mid-channel, and the reason a second goblin cannot loot the same chest. Weak so a
	 *  dead interactor cannot hold the lock. */
	TWeakObjectPtr<AActor> ChannellingInteractor;
};
