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

	/**
	 * Eligibility to be LOOKED AT, as opposed to acted on. Everything CanInteract tests except
	 * availability.
	 *
	 * These are deliberately two questions. Focus answers "what am I looking at" and must include the
	 * locked crate - otherwise pressing F at one is indistinguishable from pressing F at empty air, and
	 * the refusal shake (#187) would either never fire or fire at nothing. Interaction answers "may I
	 * do this", and must still refuse it. Contention is excluded from BOTH: an interactable somebody
	 * else is already channelling is not yours to be refused for, it is simply not yours.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GoblinSiege|Interaction")
	bool CanFocus(AActor* Interactor) const;
	virtual bool CanFocus_Implementation(AActor* Interactor) const;

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

	/**
	 * On completion, drop the owner's root primitive into physics so a looted container visibly
	 * slumps.
	 *
	 * A looted chest currently looks EXACTLY like an unlooted one - `bConsumeOnComplete` only flips
	 * an availability flag, so the only feedback a player gets is that the prompt stops appearing.
	 * That is a real gap and not a cosmetic one: the first playtest of this framework could not tell
	 * a successful loot from a failed one, and neither could I.
	 *
	 * Physics rather than a mesh swap or a fracture, deliberately. A swap needs a second authored
	 * mesh per container, and `UGSBreakableComponent`'s fracture path needs a GeometryCollection per
	 * container; neither exists for the chest, and both are art tasks. A topple costs nothing, works
	 * on any mesh, and reads instantly. Swap to a fracture later where the art justifies it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Interaction")
	bool bCollapseOnComplete = false;

	/** Sideways shove applied with the collapse, so it topples instead of settling straight down.
	 *  Scaled by mass inside ApplyCollapse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Interaction",
		meta = (EditCondition = "bCollapseOnComplete", ClampMin = "0.0"))
	float CollapseImpulse = 250.f;

private:
	/** Server-side; the physics state replicates from there. */
	void ApplyCollapse();

	/** Whoever is mid-channel, and the reason a second goblin cannot loot the same chest. Weak so a
	 *  dead interactor cannot hold the lock. */
	TWeakObjectPtr<AActor> ChannellingInteractor;
};
