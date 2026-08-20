// The interactor half of the hold-E framework (GDD §8): lives on the SCOUT, finds what is in front
// of them, and owns the channel state machine - duration, 0..1 progress for the HUD bar, and every
// abort rule from the stealth spec (damage, key release, leaving range, breaking facing).
// UGSGA_Interact drives it; it does not drive itself, so the GAS side owns activation gating and
// this side owns the seconds.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "GSInteractionComponent.generated.h"

class UGSCarryComponent;
class UGSInteractableComponent;

/** Why a channel stopped. Carried on the ended delegate so the HUD can say "you moved" instead of
 *  silently clearing the bar - and so debugging an abort does not need a breakpoint. */
UENUM(BlueprintType)
enum class EGSInteractEndReason : uint8
{
	Completed,
	InputReleased,
	OutOfRange,
	LostFacing,
	Damaged,
	TargetLost,
	Cancelled
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnInteractFocusChanged, UGSInteractableComponent*, NewFocus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGSOnInteractChannelStarted, UGSInteractableComponent*, Interactable, FGameplayTag, VerbTag, float, DurationSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnInteractChannelProgress, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSOnInteractChannelEnded, bool, bCompleted, EGSInteractEndReason, Reason);
/** A press that was refused before any channel began - the locked crate case (#187). Carries the
 *  interactable that said no, or null when the press found nothing at all. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnInteractRefused, UGSInteractableComponent*, Interactable);

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSInteractionComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** What a prompt widget should be showing. Null when nothing is in range or in the cone. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	UGSInteractableComponent* GetFocusedInteractable() const { return FocusedInteractable.Get(); }

	/** 0..1 - the HUD channel bar. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	float GetChannelProgress() const { return ChannelProgress; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	bool IsChannelling() const { return bIsInteracting; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	FGameplayTag GetActiveVerbTag() const { return ActiveVerbTag; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	float GetActiveChannelDuration() const { return ChannelDuration; }

	/** Called by UGSGA_Interact on activation. False means there was nothing to channel and the
	 *  ability should end immediately. Bind OnChannelEnded BEFORE calling this: a zero-second verb
	 *  finishes inside this call.
	 *
	 *  Runs on the owning client for feel (bar, focus, abort) and forwards to the server, which runs
	 *  the same state machine authoritatively and is the only side that pays out. On a listen server
	 *  the two are the same machine and no RPC is sent. */
	bool BeginChannel();

	/** Any non-completion end. Safe to call when not channelling. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Interaction")
	void AbortChannel(EGSInteractEndReason Reason);

	/** The player let go of E. Interruptible always (stealth spec) - there is no committed window. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Interaction")
	void ReleaseInteractInput();

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Interaction")
	FGSOnInteractFocusChanged OnFocusChanged;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Interaction")
	FGSOnInteractChannelStarted OnChannelStarted;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Interaction")
	FGSOnInteractChannelProgress OnChannelProgress;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Interaction")
	FGSOnInteractChannelEnded OnChannelEnded;

	/**
	 * Fired when an interact press could not start a channel.
	 *
	 * Michael, 2026-08-18, choosing this over a "Locked" caption: "is it possible to make the UI
	 * reticule jiggle slightly to give you the indication you can't interact with the item."
	 *
	 * Only fires when something was actually LOOKED AT and refused, never on a press into empty air -
	 * a shake every time F is tapped while running would be noise, and would teach the player to
	 * ignore the one signal that means something.
	 */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Interaction")
	FGSOnInteractRefused OnInteractRefused;

protected:
	/** Damage aborts (stealth spec). Bound to AGSCharacterBase::OnHealthChanged rather than a new
	 *  event route - the existing damage flow already reports a signed delta on every write. */
	UFUNCTION()
	void HandleOwnerHealthChanged(float NewHealth, float MaxHealth, float Delta);

	/** The authoritative channel. Re-validates rather than trusting the client's pick: the target has
	 *  to pass CanInteract, range and facing on the server too, or a modified client loots the map. */
	UFUNCTION(Server, Reliable)
	void ServerBeginChannel(UGSInteractableComponent* Target, bool bPutDown);

	UFUNCTION(Server, Reliable)
	void ServerAbortChannel(EGSInteractEndReason Reason);

	/** Which verb this press means. Focused interactable wins over put-down, so full hands can still
	 *  extract, loot, or foul a well - dropping is what E means only when nothing is focused. */
	bool ResolveChannelTarget(UGSInteractableComponent*& OutTarget, bool& bOutPutDown);
	bool StartChannelInternal(UGSInteractableComponent* Target, bool bPutDown);

	void RefreshFocus();
	void SetFocus(UGSInteractableComponent* NewFocus);
	void TickChannel(float DeltaTime);
	void CompleteChannel();
	void ClearChannelState();

	/** True on the machine driving this pawn's input - the only one that should scan for focus. */
	bool IsLocalInteractor() const;
	bool HasChannelAuthority() const;

	/** Range (3D) and facing (2D dot against the interactor's forward) for one candidate. */
	void EvaluateGeometry(const UGSInteractableComponent* Interactable, float& OutFacingDot, float& OutDistance) const;
	bool HasLineOfSight(const UGSInteractableComponent* Interactable) const;
	FVector GetInteractorForward() const;
	UGSCarryComponent* GetCarryComponent() const;
	void DrawDebugState() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Interaction", meta = (ClampMin = "10.0"))
	float InteractRange = 250.f;

	/** Cosine of the half-angle of the facing cone. 0.5 = 60° either side, which is generous enough
	 *  that a chest you are plainly looking at qualifies. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Interaction", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float FacingDotMin = 0.5f;

	/** Hysteresis on the abort checks. Without it a single footstep or a mouse twitch at the edge of
	 *  the cone cancels a channel you were plainly still performing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Interaction", meta = (ClampMin = "0.0"))
	float RangeSlack = 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Interaction", meta = (ClampMin = "0.0"))
	float FacingSlack = 0.15f;

	/** Focus is a prompt, not a hit test - 10 Hz is invisible to the player and free. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Interaction", meta = (ClampMin = "0.0"))
	float FocusScanInterval = 0.1f;

	/** Blocks looting through a wall. Off for verbs authored on props that hide their own pivot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Interaction")
	bool bRequireLineOfSight = true;

	/** Aim decides the cone when there is a controller - the camera is where the player is looking,
	 *  and the pawn may still be turning toward it (tech doc §16 rotation modes). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Interaction")
	bool bUseControlRotationForFacing = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Interaction")
	TArray<TEnumAsByte<EObjectTypeQuery>> OverlapObjectTypes;

	/** Cheap replicated root state: a co-op partner can see that you are mid-channel. The progress
	 *  itself is local - nobody needs a replicated float ticking at frame rate.
	 *
	 *  COND_SkipOwner: the owning client sets this the moment it presses E and must keep its own view
	 *  of it, or a round trip's worth of server replication would flicker the HUD bar off. Everyone
	 *  else gets the server's value, which is the true one. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "GoblinSiege|Interaction")
	bool bIsInteracting = false;

private:
	TWeakObjectPtr<UGSInteractableComponent> FocusedInteractable;
	TWeakObjectPtr<UGSInteractableComponent> ActiveInteractable;

	/** A put-down channel has no interactable - the object is already in your hands. */
	bool bActivePutDown = false;

	FGameplayTag ActiveVerbTag;
	float ChannelDuration = 0.f;
	float ChannelProgress = 0.f;
	float FocusScanAccumulator = 0.f;
};
