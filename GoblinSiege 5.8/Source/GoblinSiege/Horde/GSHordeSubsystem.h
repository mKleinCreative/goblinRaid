// The horde pool, the summon, and the stimulus bus - GDD §2.5 ("the horn and the horde") and the
// repo export §5. Written 2026-08-07, ticket #069.
//
// WHY A WORLD SUBSYSTEM: the same four reasons UGSRaidDirector's header sets out, and one more that
// is specific to this feature. A horde goblin has to be summonable on a map nobody wired by hand,
// including a generated one; the GameMode is a Blueprint asset a designer also owns; the GameMode
// does not exist on clients, and the HUD wants to show the pool counter; and UGSBurnMaskSubsystem
// already made this call for this lifecycle reason. The extra one: the pool is per-WORLD state that
// must reset when a raid restarts, and a PIE restart gives us that reset for free.
//
// ---- THE THREE POOL EXITS (decision 40, and GDD §2.7) --------------------------------------
// Getting this wrong in either direction breaks the whole economy, and both wrong versions look
// reasonable in isolation:
//
//   DEBIT ON SPAWN. Blowing the horn spends goblins from the reserve. That is the ONLY debit.
//   Re-horning a goblin that already walked home is free because it was never credited back on
//   the way out - it simply never left the reserve to begin with.
//
//   DEATH is permanent and costs nothing extra. The goblin was already debited when it spawned;
//   dying just means it never comes back. Debiting again on death double-charges, and a pool that
//   lost nobody would still drain.
//
//   DELIVERY CREDITS BACK. A courier that reaches the stones rejoins the summonable pool
//   ("only death is permanent - a successful delivery costs nothing but time", §2.7). Per GOBLIN,
//   never per cargo: three prisoners on one rope chain is one goblin returning, not three.
//
//   THE WARREN SPENDS ONE WITHOUT KILLING IT (§2.6) - "the digger stays behind to hold it open".
//   That is a fourth state, not a death: it must not fire the death path, must not credit back,
//   and must not leave the goblin counted as active.
//
// ---- WHY THE GOBLINS HAVE NO SENSES ---------------------------------------------------------
// GDD §3.4 is explicit that the horde is "a crowd of perception-less agents fed stimuli by a
// central subsystem, which is what makes ten concurrent goblins cheap", and §3.1 calls it "the
// game's quietest performance trick". Before this class, AGSHordeAIController inherited
// AGSAIControllerBase's UAIPerceptionComponent and a 1200uu sight sense - ten independent sight
// queries per frame feeding an empty perception handler (since deleted, #115). The threat registry
// below is the replacement: things that matter announce themselves once, and every goblin reads the
// same resolved answer.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Horde/GSHordeOrderTypes.h"
#include "GSHordeSubsystem.generated.h"

class AGSHordeGoblin;
class AGSCharacterBase;
class AGSHordeOrderMarker;
class AController;

/** Reserve remaining, active count, cap. Everything a HUD counter or a bark trigger needs. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGSOnHordePoolChanged, int32, ReserveRemaining, int32, ActiveCount, int32, ActiveCap);

/** The treeline is silent - fired once, when the reserve first reaches zero. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnHordePoolDry);

/** A standing order was issued, replaced or cleared. Exists so a bark or a marker sound can hang off
 *  the point command without a rebuild - the provision AGENT_STATE asks new systems to make. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGSOnHordeOrderChanged, EGSHordeOrder, Verb, AActor*, Subject, FVector, Location);

UCLASS(Config = Game)
class GOBLINSIEGE_API UGSHordeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------ lifecycle

	/** Game and PIE only, for UGSRaidDirector's reason: UE spins up editor, preview and thumbnail
	 *  worlds constantly and none of them should own a raid pool. */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Null-safe accessor. Mirrors UGSRaidDirector::Get - the two subsystems that omit this
	 *  (Score, Stealth) make every caller write the same three lines. */
	static UGSHordeSubsystem* Get(const UObject* WorldContextObject);

	// ------------------------------------------------------------------ the pool

	/**
	 * Blow the horn: spawn up to SummonsPerBlast goblins, debiting the reserve by however many
	 * actually spawned.
	 *
	 * Returns the number spawned, which is NOT SummonsPerBlast whenever the reserve is short or the
	 * active cap is close. A return of 0 with a dry pool is the designed end state ("when the pool
	 * is dry, the treeline is silent"), not an error.
	 *
	 * Summoner may be null in the single-player slice; it is threaded through so the cap can be
	 * per-player later without touching any caller. See ActiveGoblins.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Horde")
	int32 SummonWave(AController* Summoner);

	/**
	 * ONE goblin climbs out. Returns 1, or 0 if the reserve is dry or the active cap is full.
	 *
	 * THIS IS NOW THE ONLY DEBIT IN THE CLASS (decision 40 unchanged in substance, moved in
	 * location). SummonWave used to hold it and is now a loop over this - which matters, because
	 * the horn no longer summons in blasts. Michael, 2026-08-20: "if click, one goblin appears, if
	 * you hold down MMB, they pop out of the warren one at a time until you get the full squad."
	 * A held horn calls THIS on a timer, so a debit that still lived in SummonWave would either be
	 * skipped entirely or charged in fours.
	 *
	 * Death still does not debit again, and a delivery still credits back against this same counter.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Horde")
	int32 SummonOne(AController* Summoner);

	/** Room left under the active cap, bounded by what the reserve can actually pay for. What a
	 *  held horn asks before deciding it is done. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde")
	int32 GetSummonableNow() const;

	/** Permanent. The goblin was debited at spawn, so this credits nothing back - it only stops
	 *  counting against the active cap. */
	void NotifyGoblinDied(AGSHordeGoblin* Goblin);

	/** A courier reached the stones or the Warren. Credits ONE goblin back to the reserve
	 *  regardless of how much cargo it carried, then despawns it. */
	void NotifyCourierDelivered(AGSHordeGoblin* Goblin);

	/** The digger stays behind to hold the hole open (§2.6). Spent, but not dead: no death path,
	 *  no credit back. */
	void NotifyGoblinSpentOnWarren(AGSHordeGoblin* Goblin);

	/** Back to a full reserve and no actives. Bound to the raid director's start. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Horde")
	void ResetPoolForNewRaid();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde")
	int32 GetReserveRemaining() const { return ReserveRemaining; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde")
	int32 GetActiveCount() const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde")
	int32 GetActiveCap() const { return ActiveCap; }

	/** The 20 of "a finite raid pool of 20" - derived, never typed as a literal, so the cap and the
	 *  pool cannot drift apart when either is tuned. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde")
	int32 GetRaidPoolSize() const { return ActiveCap * ReserveMultiplier; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde")
	bool IsPoolDry() const { return ReserveRemaining <= 0; }

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Horde")
	FGSOnHordePoolChanged OnHordePoolChanged;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Horde")
	FGSOnHordePoolDry OnPoolDry;

	// ------------------------------------------------------------------ the order board (#141)
	//
	// GDD §2.5's point command, finally given a writer. EGSHordeState::Commanded has existed since
	// #069 with nothing anywhere setting it; AGSHordeAIController::RefreshStimulus now does, off these.
	//
	// ONE ORDER PER SUMMONER, not per goblin. Splitting a warband needs a selection UI that does not
	// exist, and the GDD is explicit that there is "no squad-command layer" - this is the one context
	// command, with the verb named on a wheel instead of guessed from the crosshair.

	/**
	 * SERVER ONLY. Replaces this summoner's standing order, retiring the previous marker and spawning
	 * a new one.
	 *
	 * Follow (and None) CLEAR rather than set: the recall's whole job is to put the horde back on the
	 * default Follow-and-Frenzy behaviour, and a standing "Follow" order that had to be honoured would
	 * be a third thing meaning the same as the absence of an order.
	 */
	void IssueOrder(AController* Summoner, EGSHordeOrder Verb, AActor* Subject, const FVector& Location);

	/** Retires the marker and drops the summoner's goblins back to Follow/Frenzy. */
	void ClearOrder(AController* Summoner);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde")
	EGSHordeOrder GetOrderVerbFor(AGSHordeGoblin* Goblin) const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde")
	AActor* GetOrderSubjectFor(AGSHordeGoblin* Goblin) const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde")
	FVector GetOrderLocationFor(AGSHordeGoblin* Goblin) const;

	/** Where a courier takes its cargo. Read from the order, which resolved it ONCE at issue time -
	 *  see FGSHordeOrder::DeliveryLocation for why this must never be a live search. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde")
	FVector GetDeliveryLocationFor(AGSHordeGoblin* Goblin) const;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Horde")
	FGSOnHordeOrderChanged OnHordeOrderChanged;

	/** One line per standing order, for GS.Horde.Status. "No orders" has to be distinguishable from
	 *  "an order nobody is obeying" without attaching a debugger - those look identical in play and
	 *  they have completely different causes. */
	FString DescribeOrders() const;

	// ------------------------------------------------------------------ the stimulus bus

	/**
	 * Announce a hostile worth swarming. Called from the Frenzy hooks on AGSCharacterBase
	 * (OnDamaged / OnDealtDamage) and from the point command - never from a per-goblin sense.
	 *
	 * Idempotent: re-registering an existing threat refreshes its timestamp rather than stacking.
	 */
	void RegisterThreat(AActor* Threat);

	// The three below take a NON-const AGSHordeGoblin* deliberately. UnrealHeaderTool rejects a
	// `const T*` parameter on a UFUNCTION outright ("not supported by blueprint"), so const-correct
	// signatures here would not compile - and finding that out costs a six-minute editor-closed
	// build on this machine. The functions themselves are const and treat the pawn as read-only.

	/** The one resolved answer every goblin reads instead of running its own perception query.
	 *  Null means "nothing to frenzy on - go back to Follow". */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde")
	AActor* GetAssignedTargetFor(AGSHordeGoblin* Goblin) const;

	/** Where a goblin with nothing to do should be: near its summoner, in a loose scamper. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde")
	AActor* GetFollowTargetFor(AGSHordeGoblin* Goblin) const;

	/** Stable 0..N-1 index of this goblin among its summoner's actives, so Follow can hand out
	 *  deterministic ring slots instead of re-rolling a random angle on every repath. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde")
	int32 GetFollowSlotFor(AGSHordeGoblin* Goblin) const;

	/**
	 * WHERE THIS GOBLIN SHOULD STAND WHILE FOLLOWING - a post behind the summoner, not the summoner.
	 *
	 * `FollowSlot` has been computed and published to the blackboard since the horde was written, and
	 * NOTHING has ever consumed it: `Follow Summoner` is a stock BTTask_MoveTo pointed at the
	 * `FollowTarget` OBJECT, so every goblin in the band paths to the same point - the player. That
	 * is the crowding. The controller's own comment says "the ring maths belongs in the BT's move
	 * task"; no such task was ever written.
	 *
	 * Rows of three, filling backwards from the summoner's heel: slot 0-2 across the first rank,
	 * 3-5 the second, and so on. Deterministic on purpose - an earlier version rolled FMath::FRand()
	 * on every repath, so a goblin standing still kept changing its mind about where to stand, which
	 * reads as a pathing bug rather than as scatter.
	 *
	 * Measured from the summoner's ACTOR forward rather than its control rotation, so the formation
	 * does not swing around the player every time they look sideways.
	 */
	FVector GetFollowPostFor(AGSHordeGoblin* Goblin) const;

	// Plain members, not UPROPERTYs: a UWorldSubsystem has no details panel to edit them in, so
	// EditDefaultsOnly would advertise a knob that does not exist. Retune here.

	/** First rank's distance behind the summoner. */
	float FollowPostDepth = 220.f;

	/** Added per rank behind the first. */
	float FollowPostRankSpacing = 140.f;

	/** Left-right spacing within a rank. */
	float FollowPostFileSpacing = 115.f;

	/** Goblins per rank. */
	int32 FollowPostPerRank = 3;

protected:
	// ---- tuning (Config = Game; a subsystem has no CDO, so Config is its EditDefaultsOnly) ----

	/** "up to 10 active" (§2.5). */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Horde", meta = (ClampMin = "1"))
	int32 ActiveCap = 10;

	/** "reserve 2x cap" (repo GDD §5). 10 x 2 = the raid pool of 20. */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Horde", meta = (ClampMin = "1"))
	int32 ReserveMultiplier = 2;

	/** "3-4 per blast" (§2.5). The upper bound; a blast is clamped by cap and reserve. */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Horde", meta = (ClampMin = "1"))
	int32 SummonsPerBlast = 4;

	/** Which pawn a summon spawns. Set in DefaultGame.ini to BP_HordeGoblin - spawning the C++
	 *  class directly gives an invisible goblin with no mesh and no anim BP. */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Horde")
	FSoftClassPath HordeGoblinClassPath;

	/** The beacon. Set in DefaultGame.ini to BP_HordeOrderMarker, exactly like the goblin above.
	 *  Unset is survivable and deliberately so: orders are still issued and still obeyed, the player
	 *  just cannot see where he sent them. ResolveOrderMarkerClass says so once. */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Horde")
	FSoftClassPath OrderMarkerClassPath;

	/** How long a registered threat stays interesting with nothing renewing it. Frenzy expiring
	 *  back to Follow is what stops the horde committing to a guard who ran away. */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Horde", meta = (ClampMin = "0.0"))
	float ThreatMemorySeconds = 8.f;

	// ---- opening a fight (2026-08-08) -----------------------------------------------------
	// Until now RegisterThreat was fed ONLY by the three Frenzy damage hooks on AGSHordeGoblin -
	// "anything that attacks you" and "anything you attack". Both are retaliation. A militiaman
	// standing three metres away who had not yet hit anybody was never a threat, so a summoned
	// warband would walk past the garrison and follow the player, and the raid's whole centrepiece
	// fight could only start if the player personally began it.
	//
	// The scan is deliberately ONE sweep for the whole crowd rather than a proximity check on each
	// AGSHordeGoblin. That distinction is the entire point of this class: GDD §3.4 calls the horde
	// "a crowd of perception-less agents fed stimuli by a central subsystem, which is what makes ten
	// concurrent goblins cheap", and a per-pawn check is per-agent sensing wearing a different hat.

	/** Seconds between proximity sweeps. Early-outs to nothing while no goblins are active. */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Horde", meta = (ClampMin = "0.05"))
	float ThreatScanIntervalSeconds = 0.5f;

	/** How close a hostile must come to any goblin (or its summoner) to be worth swarming.
	 *
	 *  1200 matches AGSAIControllerBase's SightRadius exactly, so neither faction gets a structural
	 *  first strike: a defender notices a goblin at the same distance the goblins notice him. */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Horde", meta = (ClampMin = "0.0"))
	float AutoThreatRadius = 1200.f;

	/**
	 * How far from the ORDER LOCATION a goblin will look for something to hit (#264).
	 *
	 * Michael: *"have them move to the location, then sample in front of them if there's anything to
	 * attack, then attack what's around them rather than a specific item."* An Attack order is now a
	 * PLACE, not a person: the named victim still decides where the warband goes, because the order
	 * location is where he was standing, but on arrival each goblin engages whatever is nearest to
	 * IT rather than queueing for a place on one body.
	 *
	 * Bounded rather than unlimited so an ordered warband cannot drift into a different fight it can
	 * see from where it was sent.
	 */
	float OrderEngageRadius = 1500.f;

	/**
	 * How far from a POINTED-AT LOCATION IssueOrder will search for a carryable when a Loot order's
	 * aim trace found bare ground (2026-08-30, design ticket #379, Michael: "goblins under my
	 * control will look around the area I pointed to, for anything they can loot").
	 *
	 * Deliberately separate from OrderEngageRadius rather than reusing it: that dial is a PER-GOBLIN,
	 * on-arrival search radius consumed continuously by GetAssignedTargetFor every time an Attack
	 * order's target is resolved. This one is a ONE-SHOT search run exactly once, at the moment the
	 * order is issued, inside IssueOrder itself - closer in spirit to UGSHordeCommandComponent's
	 * OrderTraceRadius (which is a precision-aim radius, 60uu, deliberately far too tight for this)
	 * than to the ambient combat dial. Sized "a room, not a handhold" per the design ticket's own
	 * framing - large enough to cover a market stall's worth of ground, not so large that pointing
	 * roughly at one loot table also picks up a barrel across the courtyard.
	 *
	 * SCOPE, per the design ticket's open question 2, answered: only actors already carrying a
	 * carryable UGSInteractableComponent (UGSHordeCommandComponent::ResolveOrderSubject's own
	 * "carryable" branch, mirrored here rather than widened) count as a candidate. Plain decoration
	 * meshes (the SM_Ham / SM_CheeseWheel / etc. props found scattered across L_Tutorial_Island by
	 * the food-consumable research earlier this session) are NOT auto-discovered - they carry no
	 * UGSInteractableComponent at all today, so including them would need that separate ticket's
	 * work to land first, not a widened search here.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Horde", meta = (ClampMin = "0.0"))
	float LootSearchRadius = 800.f;

private:
	struct FGSHordeThreat
	{
		TWeakObjectPtr<AActor> Threat;
		float LastRefreshedTime = 0.f;
	};

	/** Keyed by summoning controller from day one. In the single-player slice there is exactly one
	 *  key and this is indistinguishable from a flat list - which is the point. The repo GDD says
	 *  "cap 10 active PER PLAYER" and the canonical doc says a flat 10; they agree until co-op, and
	 *  retrofitting a key after the point command, the Warren and the couriers all read
	 *  GetActiveCount() is the expensive version of this decision. */
	TMap<TWeakObjectPtr<AController>, TArray<TWeakObjectPtr<AGSHordeGoblin>>> ActiveGoblins;

	TArray<FGSHordeThreat> Threats;

	/** Keyed the same way ActiveGoblins is, and for the same co-op reason. At most one entry per
	 *  player. */
	TMap<TWeakObjectPtr<AController>, FGSHordeOrder> ActiveOrders;

	/**
	 * Area-forage claims under a Loot order (#379 Shape B, 2026-08-30 morning): which lootable each
	 * goblin has independently committed to, so a warband spreads across a cluster of items instead of
	 * piling onto whichever single Subject the player's aim resolved. Michael: "loot everything within
	 * a radius... it makes it more interactive to see your group of goblins giggling as they smash and
	 * loot" - explicitly the point, not a race condition to prevent.
	 *
	 * MUTABLE because GetOrderSubjectFor is const (BlueprintPure, called every ~0.15-0.2s refresh from
	 * every goblin's controller) and this is a memoization cache, not externally-visible state: once a
	 * goblin claims an item it keeps returning the SAME item every refresh (sticky) rather than
	 * re-rolling mid-walk, and the entry is dropped the moment the goblin's order stops being Loot or
	 * the claimed actor goes invalid (looted, destroyed, or the order cleared). Bounded by horde size
	 * (the pool caps active goblins), so this never grows unbounded.
	 */
	mutable TMap<TWeakObjectPtr<AGSHordeGoblin>, TWeakObjectPtr<AActor>> LootClaims;

	/** The standing order covering this goblin, or null. Const because the three Get*For accessors
	 *  are; IssueOrder writes through the map directly. */
	const FGSHordeOrder* FindOrderFor(const AGSHordeGoblin* Goblin) const;

	/** Which controller summoned this goblin. Hoisted out of GetFollowTargetFor, which was doing the
	 *  same double loop inline and is now one line shorter for it. */
	AController* FindSummonerFor(const AGSHordeGoblin* Goblin) const;

	/**
	 * IssueOrder's Loot fallback (#379): the aim trace found bare ground, so search within
	 * LootSearchRadius of Location for the nearest carryable instead of refusing outright. Mirrors
	 * UGSHordeCommandComponent::ResolveOrderSubject's own carryable branch rather than widening what
	 * counts as loot - see LootSearchRadius's comment for why. Returns nullptr, unchanged, if nothing
	 * carryable is in range; IssueOrder's existing refusal handles that case exactly as it always has.
	 *
	 * Exclude lets a caller ask for the nearest UNCLAIMED item - see GetOrderSubjectFor's area-forage
	 * use (#379 Shape B). Empty by default for IssueOrder's own single-anchor resolution, which runs
	 * before any goblin has claimed anything.
	 */
	AActor* FindNearestLootable(const FVector& Location, const TSet<TWeakObjectPtr<AActor>>& Exclude = {}) const;

	/**
	 * The exact two-part test FindNearestLootable's own overlap loop applies, pulled out so
	 * GetOrderSubjectFor can apply it to a CACHED or FALLBACK subject too, not just a freshly found
	 * one. Without this, a goblin's sticky claim (or the order's own resolved Subject) kept being
	 * trusted forever once looted - Existing->Get() only ever checked "did the actor get destroyed",
	 * never "is it still something worth standing over" - so a goblin whose barrel had already been
	 * smashed and looted by someone else just kept walking back to recheck it (2026-08-30).
	 */
	bool IsStillLootable(const AActor* Candidate) const;

	/** Retires orders whose subject has died or vanished. Runs on the EXISTING ScanForThreats timer -
	 *  the horde does not get a second one, and 0.5s is well inside the time it takes anyone to
	 *  notice a beacon outliving its target. */
	void PruneStaleOrders();

	/** Nearest AGSRunicSite, else nearest Marker.HordeArrival, else the summoner. Iterates, so it is
	 *  called ONCE per order from IssueOrder and cached - never from a getter. */
	FVector ResolveDeliveryLocation(AController* Summoner, const FVector& From) const;

	UClass* ResolveOrderMarkerClass() const;

	int32 ReserveRemaining = 0;

	/** Increments on every successful spawn, forever. Only used to rotate the sideways offset at
	 *  the mouth so goblins climbing out one after another do not stack inside one capsule. Never
	 *  reset - it is an ordinal, not a count, and GetActiveCount() is the count. */
	int32 SpawnOrdinal = 0;

	/** Fired once per dry spell, not once per failed horn blast. */
	bool bDryAnnounced = false;

	/** Removes a goblin from the active map. Shared tail of every pool exit; returns the summoner
	 *  it was found under so callers can broadcast a correct active count. */
	AController* RemoveFromActive(AGSHordeGoblin* Goblin);

	void BroadcastPoolChanged();

	void PruneStaleThreats();

	/** One sweep for the whole crowd: registers every live hostile within AutoThreatRadius of any
	 *  active goblin or its summoner. Driven by ThreatScanTimer, started in OnWorldBeginPlay. */
	void ScanForThreats();

	FTimerHandle ThreatScanTimer;

	/** Resolves HordeGoblinClassPath once, loudly. A null here is the difference between "the horn
	 *  does nothing" and a diagnosable problem. */
	UClass* ResolveGoblinClass() const;

	/** Picks an arrival marker and a standable spot at it. Returns false and logs if the level has
	 *  no Marker.HordeArrival markers at all, which is the single most likely reason a correctly
	 *  built horn appears to do nothing. */
	bool FindArrivalTransform(AController* Summoner, FTransform& OutTransform) const;
};
