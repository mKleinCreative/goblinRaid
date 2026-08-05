// The drive half of the interact framework: lives on the character, finds the best candidate,
// and OWNS THE CHANNEL. The channel lives here rather than inside the ability because the HUD's
// channel bar and the abort rules both need a per-frame authority that survives GAS's activation
// quirks; UGSGA_Interact gates entry (tag rules) and holds State.Interacting, this component does
// the work. Abort rules per the stealth spec: damage aborts, release aborts, range/facing break
// aborts — damage arrives through AGSCharacterBase::OnHealthChanged (negative delta), which
// exists precisely so systems can hear damage without touching GAS internals.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSInteractionComponent.generated.h"

class UGSInteractableComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSOnChannelProgress, UGSInteractableComponent*, Interactable, float, Progress01);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnChannelEnded, UGSInteractableComponent*, Interactable);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnFocusChanged, UGSInteractableComponent*, NewFocus);

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Candidate search radius. Slightly larger than a typical interactable's MaxRange so the
	 *  prompt appears as you close in rather than popping at arm's length. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Interaction", meta = (ClampMin = "0.0"))
	float SearchRadius = 300.f;

	/** Current best candidate — what the "hold E" prompt points at. Updated on tick. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	UGSInteractableComponent* GetFocusedInteractable() const { return Focused.Get(); }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	bool IsChanneling() const { return Active != nullptr; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	float GetChannelProgress01() const;

	/** Begin the hold-E channel on the focused interactable. Returns false if nothing eligible.
	 *  Called by UGSGA_Interact on input press. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Interaction")
	bool BeginInteract();

	/** Release-side abort: E let go, or the ability was cancelled from outside. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Interaction")
	void EndInteract();

	/** Damage abort — bound to the owner's OnHealthChanged in BeginPlay; any negative delta
	 *  while channeling aborts (design: "interruptible like every channel"). */
	UFUNCTION()
	void HandleOwnerHealthChanged(float NewHealth, float MaxHealth, float Delta);

	/** HUD hooks: bar fill, bar hide, prompt swap. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Interaction")
	FGSOnChannelProgress OnChannelProgress;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Interaction")
	FGSOnChannelEnded OnChannelCompleted;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Interaction")
	FGSOnChannelEnded OnChannelAborted;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Interaction")
	FGSOnFocusChanged OnFocusChanged;

protected:
	virtual void BeginPlay() override;

	/** Sphere-overlap for interactable components, filtered by CanInteract + range + the
	 *  interactable's own facing cone, scored by (angle, distance) — nearest-most-centered wins.
	 *  The overlap is cheap at slice scale (a village, not a city); revisit if profiling says so. */
	UGSInteractableComponent* FindBestInteractable() const;

	/** True while the interactor still satisfies the active interactable's range + facing rules. */
	bool StillEligible(const UGSInteractableComponent* Interactable) const;

private:
	void Abort();

	TWeakObjectPtr<UGSInteractableComponent> Focused;

	/** Non-null only while channeling. */
	UPROPERTY()
	TObjectPtr<UGSInteractableComponent> Active;

	float ChannelElapsed = 0.f;
};
