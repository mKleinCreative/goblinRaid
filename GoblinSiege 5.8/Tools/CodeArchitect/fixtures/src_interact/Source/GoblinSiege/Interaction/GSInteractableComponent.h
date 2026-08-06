// The advertise half of the interact framework (GDD §12.1 system 8 — "blocking five other
// systems"). Anything a goblin can hold E on carries one of these: a loot stash, a takedown
// victim's proxy, the well, the extraction circle. The VERB IS DATA — a gameplay tag under
// Interact.* — so loot/takedown/foul-well/extract are rows of configuration, not subclasses,
// and a new verb ships without a new class (same argument as Objective.Burn.* being tags).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GSInteractableComponent.generated.h"

class UGSInteractionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnInteractionStarted, AActor*, Interactor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnInteractionAborted, AActor*, Interactor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnInteractionCompleted, AActor*, Interactor);

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSInteractableComponent();

	/** Which slice verb this is: Interact.Loot / Interact.Takedown / Interact.FoulWell /
	 *  Interact.Extract. Read by UI for the prompt text and by score/loot consumers on complete. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Interaction")
	FGameplayTag VerbTag;

	/** Hold-E channel length. Design doc placeholders: takedown 1.2s (signed), loot "several
	 *  seconds packing coin bit by bit". Per-instance so a strongbox can channel longer than a
	 *  market stall without a subclass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Interaction", meta = (ClampMin = "0.1"))
	float ChannelSeconds = 1.2f;

	/** Interactor must be inside this range of the owner for the whole channel — drifting out
	 *  aborts (stealth-spec abort rule "range break"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Interaction", meta = (ClampMin = "0.0"))
	float MaxRange = 220.f;

	/** Half-angle (degrees) the interactor must keep facing the owner within. 180 disables the
	 *  facing requirement (the extract circle doesn't care which way you look). The takedown's
	 *  120° BEHIND-cone is the victim's problem, not this component's — its interactable proxy
	 *  checks it in CanInteract before the channel ever starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Interaction", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float FacingHalfAngleDeg = 90.f;

	/** Master switch — a looted-out stash or an already-fouled well flips this off rather than
	 *  destroying the component. Replicated so client prompts vanish when the server says so. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "GoblinSiege|Interaction")
	bool bAvailable = true;

	/** Gate beyond bAvailable for subclass/Blueprint rules (the behind-cone, "only while
	 *  unaware", "only while portal open"). Base implementation checks bAvailable only. */
	UFUNCTION(BlueprintNativeEvent, Category = "GoblinSiege|Interaction")
	bool CanInteract(AActor* Interactor) const;

	/** Called by the interaction component driving the channel. Not for general use. */
	void NotifyChannelStarted(AActor* Interactor);
	void NotifyChannelAborted(AActor* Interactor);
	void NotifyChannelCompleted(AActor* Interactor);

	/** The framework owns the channel lifecycle ONLY — what completing actually does (grant
	 *  pouch coins, kill the victim, foul the well, bank the raid) belongs to the listener.
	 *  Those systems hook these when they land (score: Block G, runic site: Block E). */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Interaction")
	FGSOnInteractionStarted OnInteractionStarted;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Interaction")
	FGSOnInteractionAborted OnInteractionAborted;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Interaction")
	FGSOnInteractionCompleted OnInteractionCompleted;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
