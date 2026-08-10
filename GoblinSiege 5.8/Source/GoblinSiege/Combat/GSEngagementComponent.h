// Everything about attacking THIS actor: who is allowed to swing (tokens) and where they are
// allowed to stand while doing it (ring slots).
//
// ---- why one component and not two -----------------------------------------------------------
// The plan called these out separately, and the research is emphatic that permission and position
// are different reservations - DOOM's token bank and the "kung-fu circle" ring are independent
// systems. They are still ONE component here because they share a lifetime, an owner and every
// release condition: the same three events (the attacker died, it changed target, it was staggered)
// must give back both a token and a slot, and on this machine a second UCLASS costs a full
// editor-closed build. Keeping them together means "let go of this victim" is one call that cannot
// half-happen. The two concerns stay separate in the API.
//
// ---- why the budget lives on the VICTIM ------------------------------------------------------
// This is the single most-cited mechanism in the crowd-combat literature and it is always the same
// shape: an attacker must reserve from a pool owned by the thing it is attacking. DOOM (2016) -
// "each type of attack has a limited number of tokens available"; an enemy "had to acquire a token
// from that pool before it could attack ... then returned that token when it was finished". Batman:
// Arkham visibly permits two or three simultaneous attackers out of a crowd of a dozen.
//
// Putting it on the attacker instead does not work: N attackers each rationing themselves still
// produces N simultaneous swings on one victim. Only the victim knows how many blades are already
// pointed at it.
//
// ---- weighted, not counted -------------------------------------------------------------------
// Michael Dawe's Kingdoms of Amalur chapter (Game AI Pro ch.28): the pool "doesn't specify a fixed
// number of attackers, but rather controls how many can attack simultaneously based on the total
// weight of attacks". So a budget of 2 buys two light swings OR one heavy, never both, and the
// incoming damage ceiling is a designed number rather than a dice roll. That is what stops a
// defender occasionally evaporating for reasons no one can reconstruct.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSEngagementComponent.generated.h"

/** One granted reservation. Held by weak pointer so a killed attacker cannot keep its own token
 *  alive - the watchdog sweep treats a stale entry as released. */
USTRUCT()
struct FGSAttackGrant
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Holder;

	/** Weight this grant is consuming, not a count of grants. See the header note. */
	int32 Cost = 0;

	/** Committed frames. A locked grant cannot be stolen by a higher-priority attacker - cancelling
	 *  a swing that is already in its damage window would rewind an animation the player is
	 *  watching. Set when the wind-up starts, cleared at recovery. */
	bool bLocked = false;

	/** For the watchdog. A grant that outlives any plausible attack has leaked. */
	float GrantedTime = 0.f;
};

/** A claimed angular position on the ring around this actor. */
USTRUCT()
struct FGSRingSlot
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Claimant;

	/** Last time this claim showed progress, so a claimant that never arrives can be evicted. Named
	 *  for when the claim was made, but re-stamped while the claimant is closing - see ClaimRingSlot. */
	float ClaimedTime = 0.f;

	/** Nearest this claimant has ever got to the slot. The eviction test is "not arrived AND not
	 *  closing", not "time since claim", so an agent standing correctly on its slot is never evicted
	 *  while one wedged on geometry still is. */
	float ClosestApproach = TNumericLimits<float>::Max();
};

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSEngagementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSEngagementComponent();

	// ------------------------------------------------------------------ permission (tokens)

	/**
	 * Reserve Cost weight against this victim's budget. ALL OR NOTHING - a partial reservation
	 * would let a heavy attack start on half a budget, which is the whole thing this prevents.
	 *
	 * Re-requesting when you already hold a grant succeeds and re-prices it, so an attacker moving
	 * from a light to a heavy does not have to release first and risk losing its place.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Engagement")
	bool TryAcquireToken(AActor* Requester, int32 Cost);

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Engagement")
	void ReleaseToken(AActor* Requester);

	/** Mark the grant uninterruptible for the committed frames of a swing. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Engagement")
	void SetTokenLocked(AActor* Requester, bool bLocked);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Engagement")
	bool HoldsToken(AActor* Requester) const;

	/**
	 * False when this victim may not be attacked at all right now, whoever is asking.
	 *
	 * The single most valuable line in the component. DOOM freezes new attacks during a glory kill;
	 * the same rule here means a character who is flinching, guard-broken, recoiling from a blocked
	 * swing or dead cannot be piled on. One check, applied to every attacker at once - which is the
	 * difference between a defender stumbling and recovering, and a defender deleted in 0.4s by four
	 * simultaneous sweeps that all passed their own range checks.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Engagement")
	bool CanBeAttacked() const;

	/** Weight currently reserved. For GS.Combat.CrowdStats - a cap you cannot observe is a cap you
	 *  cannot trust. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Engagement")
	int32 GetReservedWeight() const;

	/** How many DISTINCT actors currently hold a grant. This is the number a designer means by
	 *  "how many are on him", and it is what the assignment capacity below is expressed in. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Engagement")
	int32 GetAttackerCount() const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Engagement")
	int32 GetTokenBudget() const { return TokenBudget; }

	// ------------------------------------------------------------------ capacity (assignment)

	/**
	 * How many attackers may be ASSIGNED to this actor at all - a wider gate than the token budget.
	 *
	 * Tokens ration who is swinging; this rations who is even walking over. Without it every goblin
	 * in the warband converges on the nearest militiaman, waits its turn for a token, and the fight
	 * is a queue. Halo 3's Objectives system solves the same problem with per-task capacity and lets
	 * the overflow "filter down" to the next task; this is that idea at its cheapest.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Engagement")
	bool HasEngagementRoom(const AActor* Requester) const;

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Engagement")
	void RegisterEngaged(AActor* Attacker);

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Engagement")
	void UnregisterEngaged(AActor* Attacker);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Engagement")
	int32 GetEngagedCount() const;

	// ------------------------------------------------------------------ position (ring slots)

	/**
	 * Claim an angular slot on the ring, returning its index, or INDEX_NONE if the ring is full.
	 *
	 * Prefers the slot nearest the claimant's CURRENT bearing, which preserves the property that
	 * made the unreserved version work: you approach from the side you are already on, so nobody
	 * crosses the pack to reach their place. The difference from before is that two agents on a
	 * similar bearing can no longer land on the same slot and shove.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Engagement")
	int32 ClaimRingSlot(AActor* Claimant);

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Engagement")
	void ReleaseRingSlot(AActor* Claimant);

	/** World-space position of a claimed slot, or the owner's location if the index is invalid. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Engagement")
	FVector GetRingSlotLocation(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Engagement")
	int32 GetClaimedSlotCount() const;

	// ------------------------------------------------------------------ release-everything

	/** One call for "this attacker has let go of this victim" - token, engagement and slot. They
	 *  share every release condition, so releasing them separately is how one of them gets missed. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Engagement")
	void ReleaseAll(AActor* Requester);

	/** Applies the archetype's numbers. Called by AGSEnemyCharacter/AGSHordeGoblin after their race
	 *  data resolves, so a knight can be worth more attention than a levy without a Blueprint edit. */
	void ConfigureFromArchetype(int32 InTokenBudget, int32 InMaxEngaged);

protected:
	virtual void BeginPlay() override;

	/** Total attack WEIGHT that may be in flight against this actor. See the header. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Engagement", meta = (ClampMin = "1"))
	int32 TokenBudget = 2;

	/** How many attackers may be assigned at once. Deliberately larger than the token budget: the
	 *  extra ones are the menace ring, circling and feinting rather than queueing motionless. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Engagement", meta = (ClampMin = "1"))
	int32 MaxEngagedAttackers = 3;

	/**
	 * Evenly spaced angular positions. Spacing S = 2*R*sin(pi/N).
	 *
	 * WAS 8, and the old comment here named the capsule-TOUCH distance as the design target - which
	 * was the bug, written down: 8 at radius 180 gives S = 138uu against a 68.6+70.2 = 138.8uu pair
	 * of guard capsules, so two guards on adjacent slots are interpenetrating before either has
	 * moved, and RVO rather than the ring decides where anyone actually stands.
	 *
	 * 6 at radius 180 gives S = 2*180*sin(30deg) = 180uu: 41uu of daylight past the worst guard pair,
	 * and past the 168.5uu that RVO's 1.2 radius expansion asks for. The radius stays 180 on purpose
	 * - clearance is bought from the slot COUNT because radius is the axis that spends AttackRange
	 * headroom (RingRadius + arrival tolerance must stay under AttackRange 250), and S responds
	 * harder to N than to R. 6 slots against MaxEngagedAttackers 3 leaves three spare for churn.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Engagement", meta = (ClampMin = "1", ClampMax = "32"))
	int32 RingSlotCount = 6;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Engagement", meta = (ClampMin = "0.0"))
	float RingRadius = 180.f;

	/** A claimant that is neither on its slot nor closing on it for this long loses it, so an agent
	 *  stuck on geometry cannot hold a place in the ring for the rest of the fight. Raised 4 -> 6 now
	 *  that the clock only runs on an agent making NO progress: a long path around a building can
	 *  legitimately stall a real claimant for several seconds. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Engagement", meta = (ClampMin = "0.0"))
	float SlotClaimTimeoutSeconds = 6.f;

	/** Close enough to the slot to count as standing on it. Must stay under half the slot spacing
	 *  (90uu at 6 slots x radius 180) or "arrived" could mean "on my neighbour's slot". */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Engagement", meta = (ClampMin = "0.0"))
	float SlotArrivedRadius = 70.f;

	/** How much closer a claimant must get to count as still closing. Roughly one service tick of
	 *  walking. Linear distance, not squared - at 400uu a squared-distance epsilon would pass on
	 *  under a uu of real progress. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Engagement", meta = (ClampMin = "0.0"))
	float SlotProgressEpsilon = 25.f;

	/** A grant older than this has leaked and is swept. Every release path is belt-and-braces
	 *  already (decorator cease-relevant, death, hit-react, target change) - this is the brace of
	 *  last resort, because a leaked token silently freezes a fight and reads as "the AI broke". */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Engagement", meta = (ClampMin = "0.0"))
	float TokenWatchdogSeconds = 3.f;

private:
	UPROPERTY()
	TArray<FGSAttackGrant> Grants;

	UPROPERTY()
	TArray<FGSRingSlot> RingSlots;

	/** Assigned attackers, which is a superset of token holders. */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Engaged;

	/** Drops grants whose holder died or which outlived TokenWatchdogSeconds, and slot claims that
	 *  timed out. Called at the top of every query so the component is self-healing rather than
	 *  relying on every caller being well behaved. */
	void PruneStale();

	float NowSeconds() const;

	/** On-screen attackers outrank off-screen ones, so the fight the player is actually watching is
	 *  the one that stays live. DOOM does exactly this - "an on-screen enemy stealing a token from
	 *  an off-screen enemy". */
	static int32 RelevancePriority(const AActor* Actor);
};
