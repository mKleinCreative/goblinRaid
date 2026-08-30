// Common interface for the tutorial's three burn objectives (design doc §6.3): the wheat FIELD
// (grid spread), the WINDMILL (window ignition -> dust detonation -> state swap) and the MARKET
// (stall-to-stall cluster spread). Written 2026-07-28 for Block C.
//
// Why a base class: the mission tracker, the HUD objective list, the Overlord's per-objective
// whispers and the score system all want "how far along is this, and is it done" without caring
// which of the three shapes it is. Completion is normalised 0..1 by every subclass.
//
// Objective points are COMPLETION-ONLY (decision 29) - nothing part-credits. OnBurnObjectiveCompleted
// fires exactly once, ever.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "GSBurnObjectiveBase.generated.h"

UENUM(BlueprintType)
enum class EGSBurnObjectiveType : uint8
{
	Field,
	Windmill,
	Market
};

/**
 * How an objective currently reads in the HUD list (2026-07-31, Q-37 - the state half of Q-32).
 *
 * Q-32 was ruled on 2026-07-31: the raid is won by burning ONE OF EACH TYPE, not all of everything.
 * That ruling needs somewhere to live, because "one of each type" means a carrier's importance
 * CHANGES DURING THE RAID - the second wheat field is a real objective right up until the first one
 * finishes, and then it is a bonus. A plain bool "done / not done" cannot express that, and the HUD
 * objective list is the one place the player reads the win condition off.
 *
 *   Required - burning this is still needed to win, because no carrier of its type has completed.
 *   Optional - a carrier of this type has already completed. Still burnable, still worth points
 *              (objective points are completion-only, decision 29), no longer on the critical path.
 *   Complete - this carrier itself is done. Terminal.
 *
 * The MARKET is never Optional, and that falls out of the rule rather than needing a special case:
 * demotion only ever happens to the OTHER carriers of a type that has just been completed, and
 * there is exactly one market, so it has no siblings to be demoted by.
 *
 * Deliberately separate from bCompleted rather than folded into it. bCompleted is this actor's own
 * physical fact - it burned - and drives HandleCompleted, the alarm and the score. ListState is a
 * statement about the raid's objective list as a whole, decided by looking at every carrier at
 * once, and only one of its three values coincides with bCompleted.
 */
UENUM(BlueprintType)
enum class EGSObjectiveListState : uint8
{
	Required,
	Optional,
	Complete
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnBurnObjectiveCompleted, AGSBurnObjectiveBase*, Objective);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnBurnObjectiveProgress, float, Completion01);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnObjectiveListStateChanged, EGSObjectiveListState, NewState);

UCLASS(Abstract)
class GOBLINSIEGE_API AGSBurnObjectiveBase : public AActor
{
	GENERATED_BODY()

public:
	AGSBurnObjectiveBase();

	/** 0..1. What "progress" means is the subclass's business; that it is comparable is not. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Objective")
	float GetCompletion01() const { return Completion01; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Objective")
	bool IsComplete() const { return bCompleted; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Objective")
	EGSBurnObjectiveType GetObjectiveType() const { return ObjectiveType; }

	/** Required / Optional / Complete - see EGSObjectiveListState (2026-07-31, Q-37). */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Objective")
	EGSObjectiveListState GetListState() const { return ListState; }

	/**
	 * Server-side setter for the list state (2026-07-31, Q-37).
	 *
	 * Public and BlueprintCallable because the DEMOTION DECISION does not belong to this actor: it
	 * needs to see every carrier in the level at once, which nothing here can do. See the hook
	 * comment in the .cpp.
	 *
	 * Authority-only and idempotent. Complete is treated as terminal - once a carrier has burned,
	 * nothing may demote it back to Optional, or a completed objective could disappear from the
	 * HUD's done-list when a sibling of its type finished afterwards.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Objective")
	void SetListState(EGSObjectiveListState NewState);

	/** Which burn TYPE this carrier counts as. See ObjectiveTypeTag. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Objective")
	FGameplayTag GetObjectiveTypeTag() const { return ObjectiveTypeTag; }

	/**
	 * Set this carrier's type tag and HUD name (2026-08-05).
	 *
	 * For SCRIPTED PLACEMENT only - a designer sets both in the Details panel, and that remains the
	 * normal path. This exists because editor Python cannot construct an FGameplayTag in this build
	 * at all (see UGSRaidLibrary), so without it a script can place a burn objective but can never
	 * make it count toward the win condition.
	 *
	 * Deliberately NOT callable once the raid is under way: retyping a carrier mid-raid would
	 * strand it in the director's per-type bucket under its old tag, so the demotion pass and the
	 * win check would disagree about what it is. Placement-time only.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Objective")
	void SetObjectiveIdentity(FGameplayTag InTypeTag, FText InDisplayName);

	/** Display name for the HUD objective list. No arrows, ever - names only (decision 11). */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Objective")
	FText GetObjectiveDisplayName() const { return ObjectiveDisplayName; }

	/** The player's torch lands here. Subclasses decide what that means - a kitbashed building
	 *  refuses fire on its walls and overrides this to do nothing there (see AGSBuildingObjective).
	 *  This is only ever REACHED for a given actor if ContainsWorldLocation (below) says yes first -
	 *  see that function's comment; #362 is the record of what happens when a subclass changes this
	 *  body without checking that. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Objective")
	virtual void IgniteAtLocation(const FVector& WorldLocation);

	/**
	 * Does this objective consider WorldLocation to be "inside" it?
	 *
	 * A torch thrown into wheat hits the LANDSCAPE, not the field actor, so hit-actor lookup alone
	 * can never light a field. The torch instead asks every burn objective whether the impact point
	 * belongs to it. THIS is the actual gate on whether IgniteAtLocation is ever called for a given
	 * objective - not that function's own body. A kitbashed building answers false for its walls on
	 * purpose (see AGSBuildingObjective); a subclass that wants to accept fire from anywhere on its
	 * own geometry, like the mill, has to override this to say so, or FindObjectiveAtLocation below
	 * will never select it no matter what IgniteAtLocation does (#362).
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Objective")
	virtual bool ContainsWorldLocation(const FVector& WorldLocation) const;

	/** Returns the objective owning this location, or null. Used by the torch. */
	static AGSBurnObjectiveBase* FindObjectiveAtLocation(const UObject* WorldContextObject, const FVector& WorldLocation);

	/** GS.Burn.Debug - drives the debug-draw overlays on all three burn types. */
	static bool IsBurnDebugEnabled();

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Objective")
	FGSOnBurnObjectiveCompleted OnBurnObjectiveCompleted;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Objective")
	FGSOnBurnObjectiveProgress OnBurnObjectiveProgress;

	/**
	 * Required -> Optional -> Complete transitions (2026-07-31, Q-37). Fires on the server and on
	 * clients (via OnRep_ListState), so the HUD objective list can re-style an entry - greying a
	 * demoted carrier, striking through a burned one - without polling every objective every frame.
	 */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Objective")
	FGSOnObjectiveListStateChanged OnObjectiveListStateChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	/** Subclasses call this as their internal state moves. Clamps, replicates, broadcasts, and
	 *  fires completion exactly once when CompletionThreshold01 is crossed. */
	void SetCompletion01(float NewCompletion01);

	/** Override for the collapse/detonation/burnout moment. Called once, server-side. */
	virtual void HandleCompleted();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Debug overlay, polled on a timer while GS.Burn.Debug is on. Subclasses draw their own state.
	 * This is the actual playtest instrument: the field's cells are data with no meshes, so
	 * coloured boxes are the only way to see a spread rate before any FX exist.
	 */
	virtual void DrawDebugState() const {}

	void DebugTick();

	FTimerHandle DebugTimerHandle;

	UFUNCTION()
	void OnRep_Completion01();

	UFUNCTION()
	void OnRep_Completed();

	UFUNCTION()
	void OnRep_ListState();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Objective")
	EGSBurnObjectiveType ObjectiveType = EGSBurnObjectiveType::Field;

	/**
	 * Which burn TYPE this placed carrier declares itself to be (2026-07-31, Q-37) - one of
	 * Objective.Burn.Mill / Objective.Burn.Field / Objective.Burn.Market, the native tags the
	 * burn-types spec §5 has owed since it was written (see Combat/GSGameplayTags.h).
	 *
	 * A tag rather than the EGSBurnObjectiveType enum sitting right above it, and the distinction
	 * is worth keeping. ObjectiveType is a C++ switch value - it drives the debug overlay and the
	 * base's own branching, and adding to it is a recompile. The type tag is the LOOKUP KEY for
	 * "have we burned one of these yet", which is a data question the Raid Loop's demotion pass and
	 * the HUD both ask, and which a fourth burn type (a granary, a tannery) should be able to
	 * answer by being placed in a level rather than by editing an enum.
	 *
	 * EditAnywhere, per instance and not per class: the point of Q-32 is that several carriers of
	 * the same type coexist, and a designer placing the second field has to be able to say so.
	 * Empty by default - an untagged carrier is simply invisible to the demotion pass, which is the
	 * safe failure (it stays Required) rather than one that silently satisfies the win condition.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Objective")
	FGameplayTag ObjectiveTypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Objective")
	FText ObjectiveDisplayName;

	/** Field ships at 0.70 (decision 32, placeholder). Mill and market override. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Objective", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CompletionThreshold01 = 0.7f;

	/** Alarm added the instant this objective completes - a finished burn is unmissable. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Objective|Tuning")
	float AlarmOnCompletion = 25.f;

	/**
	 * True  - completing this objective promotes the town straight to Raid (ReportFireSeenByHuman).
	 * False - it only arms the unseen-fire fuse, letting the existing timer decide.
	 *
	 * DESIGN CALL awaiting ratification: true asserts that a finished burn is unmissable even if
	 * it is razed in the far outskirts at night with every human indoors. Flip to false if the
	 * fuse should get to do its job there.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Objective|Tuning")
	bool bHardPromoteOnCompletion = true;

	/** Placement anchor. Without a RootComponent these actors cannot be moved at all, and the
	 *  field's grid - which is derived from the actor transform - would be pinned to world origin. */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Objective")
	TObjectPtr<USceneComponent> ObjectiveRoot;

	UPROPERTY(ReplicatedUsing = OnRep_Completion01)
	float Completion01 = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Completed)
	bool bCompleted = false;

	/**
	 * Q-32's Required -> Optional -> Complete carrier state (2026-07-31, Q-37).
	 *
	 * Replicated because it is a HUD-facing fact and the HUD runs on every machine. Starts Required
	 * on every carrier: an objective is on the critical path until something proves otherwise, and
	 * a demotion pass that has not run yet must never leave a carrier reading as a bonus.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_ListState)
	EGSObjectiveListState ListState = EGSObjectiveListState::Required;
};
