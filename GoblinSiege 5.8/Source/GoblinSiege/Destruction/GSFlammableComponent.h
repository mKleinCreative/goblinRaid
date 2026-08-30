// Attach to anything that can burn. FireResistance implements material hardness on the burn axis
// (GDD §11.0): 0 = tinder, 1 = effectively immune (stone barracks). Reconstructed 2026-07-19 to
// match surviving callers (GSSpawnerActor binds OnBurnedDown; GSFireVolume/BTTask_Firefight call
// Ignite/Extinguish/IsBurning).
// 2026-07-28 (Block C): + fire SPREAD. This is the "fire and destruction as a language" pillar -
// a burning thing lights its neighbours once it is properly going, so a torch in a hedgerow can
// take a fence, a haycart and the granary behind them (decision 26).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSFlammableComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnIgnited);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnExtinguished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnBurnedDown);

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSFlammableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSFlammableComponent();

	/**
	 * InSpreadGeneration is how many chain-spread hops removed this ignition is from an original,
	 * player/objective-caused fire (0). Every caller outside this component's own TrySpread() wants
	 * the default - a torch, a mill detonation, a debug command are all "the origin," not a hop -
	 * so existing zero-arg call sites are unaffected. Only TrySpread() ever passes non-zero, and
	 * only on the actor IT is igniting. See SpreadChanceDecayPerHop/MaxSpreadGenerations.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Fire")
	void Ignite(int32 InSpreadGeneration = 0);

	/** Firefighting defenders (BTTask_Firefight) and rain-of-the-future call this. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Fire")
	void Extinguish();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Fire")
	bool IsBurning() const { return bIsBurning; }

	/**
	 * Override how far this thing can throw fire. 0 or less is ignored.
	 *
	 * Exists so AGSBuildingObjective can give its adopted pieces a village-sized reach without
	 * changing this component's own 450uu default, which is correct for the loose flammables it was
	 * tuned against. A setter rather than making SpreadRadius public: this is authored data, and the
	 * one legitimate moment to change it is when a building adopts the piece.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Fire|Spread")
	void SetSpreadRadius(float NewRadius);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Fire")
	bool HasBurnedDown() const { return bBurnedDown; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Fire")
	float GetBurnProgress01() const
	{
		return BurnDurationSeconds > 0.f ? FMath::Clamp(BurnedSeconds / BurnDurationSeconds, 0.f, 1.f) : 0.f;
	}

	/** 0 = tinder, 1 = immune. Exposed so the spread pass can skip immune neighbours cheaply. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Fire")
	float GetFireResistance() const { return FireResistance; }

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Fire")
	FGSOnIgnited OnIgnited;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Fire")
	FGSOnExtinguished OnExtinguished;

	/** Fired once when the burn completes - objectives collapse, spawners silence off this. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Fire")
	FGSOnBurnedDown OnBurnedDown;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * CO-OP ROOT FIX (2026-07-31, Q-36).
	 *
	 * This component called SetIsReplicatedByDefault(true) in its constructor from the day it was
	 * written and then replicated NOTHING - there was no GetLifetimeReplicatedProps at all, so the
	 * registration was pure intent. Every piece of state lived server-side, which means every
	 * listener that hangs off OnIgnited / OnExtinguished / OnBurnedDown fired on the server only.
	 *
	 * The visible consequence is UGSBurnFXComponent: it binds all three of those delegates in
	 * BeginPlay and does nothing else, so char - the entire "fire leaves a mark" pillar, GDD §11.0
	 * - existed on the host alone. A co-op client walked through a village the host could see was
	 * blackened and saw it pristine. Same for the smolder plumes, which spawn off OnBurnedDown.
	 *
	 * The fix is the two BOOLS and nothing else. What arrives on a client is the transitions, which
	 * is all the existing listeners consume; they re-broadcast unchanged so nothing downstream
	 * needed touching.
	 */
	UFUNCTION()
	void OnRep_BurningState();

	UFUNCTION()
	void OnRep_BurnedDown();

	void BurnTick();

	/** Sphere-overlaps for neighbouring flammables and lights the ones that qualify. */
	void TrySpread();

	/** 0 = tinder, 1 = immune. Scales burn progress rate, per-material (GDD §11.0). */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Fire", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FireResistance = 0.f;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Fire")
	float BurnDurationSeconds = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Tuning")
	float BurnTickInterval = 0.5f;

	// ------------------------------------------------------------------ spread

	/** Off for hero objectives that should only burn when the player lights them; on for the
	 *  everyday flammables (fences, haycarts, hedgerows, stalls) that make fire feel alive. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Fire|Spread")
	bool bCanSpread = true;

	/** Can this be lit BY a neighbour? Objectives set this false so a stray hedge fire can't
	 *  complete a burn objective the player never aimed at (objective points are earned, §10). */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Fire|Spread")
	bool bCanBeLitBySpread = true;

	/** World units. Generous by default - the world is authored at ~2.25x real scale. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Fire|Spread")
	float SpreadRadius = 450.f;

	/** Fire only jumps once this thing is properly going, so a glancing torch doesn't chain the
	 *  whole village instantly. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Fire|Spread", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpreadAtProgress01 = 0.35f;

	/** How often a burning thing attempts to light neighbours. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Fire|Spread")
	float SpreadAttemptInterval = 1.5f;

	/** Per-attempt chance so spread reads as organic rather than a uniform expanding disc. This is
	 *  the GENERATION-0 (origin) chance; each successive chain hop is multiplied by
	 *  SpreadChanceDecayPerHop, see below. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Fire|Spread", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpreadChance = 0.5f;

	/**
	 * Diminishing returns per chain-spread hop (Michael's own proposal, 2026-08-29 - "a diminishing
	 * returns chance to have fire jump from object to object"), added 2026-08-30 after unbounded
	 * spread walked a wheat-field fire across open grass toward the village.
	 *
	 * Without this, SpreadChance=0.5 retried every SpreadAttemptInterval for the whole
	 * BurnDurationSeconds gives ~8 attempts per burning object - a neighbour within SpreadRadius
	 * has (1 - (1-0.5)^8) ≈ 99.6% odds of catching regardless of how far that neighbour already is
	 * from the original ignition. Effective chance at generation N is
	 * SpreadChance * SpreadChanceDecayPerHop^N, so a fire's reach past its own origin tapers off
	 * instead of marching indefinitely through anything flammable spaced under SpreadRadius apart.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Fire|Spread", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpreadChanceDecayPerHop = 0.55f;

	/**
	 * Hard stop, independent of chance. A component at or past this many hops from the original
	 * ignition never attempts to spread further, however lucky the rolls - decay alone asymptotes
	 * toward zero chance but never actually reaches it, and repeated attempts over a long enough
	 * burn (or a long enough chain of short-lived tinder) could still crawl arbitrarily far without
	 * a floor. This is that floor.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Fire|Spread", meta = (ClampMin = "0"))
	int32 MaxSpreadGenerations = 4;

	/** Neighbours at or above this resistance never catch from spread, regardless of roll. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Fire|Spread", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxNeighbourResistanceToCatch = 0.8f;

	/**
	 * Replicated 2026-07-31 (Q-36). Carries BOTH the ignition and the extinguish transition: the
	 * OnRep reads the new value and picks which delegate to fire, so one bool covers two events.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_BurningState)
	bool bIsBurning = false;

	/** Replicated 2026-07-31 (Q-36). One-way latch, so its OnRep only ever has one thing to say. */
	UPROPERTY(ReplicatedUsing = OnRep_BurnedDown)
	bool bBurnedDown = false;

	/**
	 * DELIBERATELY NOT REPLICATED (2026-07-31, Q-36) - along with bEnableSmoke and FireIntensity01
	 * on AGSFireVolume. Explicitly out of scope, deferred until co-op is actually scheduled.
	 *
	 * Q-36 is the ROOT fix and stops there on purpose: the two bools are transitions, they change
	 * a handful of times in an object's whole life, and they are what every existing listener
	 * consumes. BurnedSeconds changes twice a second on every burning prop in the hamlet, which is
	 * a continuous per-object float stream for a value nothing on a client currently reads - the
	 * only consumer is GetBurnProgress01, and UGSBurnFXComponent's BurnFXTick, which is its one
	 * caller, is driven by a timer started in HandleIgnited on whichever machine broadcast
	 * OnIgnited. Once these bools replicate, that timer starts on clients too and drives char off
	 * the CLIENT's local BurnedSeconds, which stays 0 there because BurnTick is not client-driven.
	 *
	 * So the honest state after Q-36: clients get the ignition, the extinguish and the burn-down
	 * beats, and therefore the final locked char and the persistent smolder - the parts that make
	 * a razed village read as razed. What they do NOT yet get is the intermediate char RAMP while
	 * something is mid-burn. Closing that wants either BurnedSeconds replicated with a condition or
	 * a client-side predicted timer, and that is a co-op scheduling decision, not a bug to fix
	 * quietly here.
	 */
	float BurnedSeconds = 0.f;

	float SecondsSinceSpreadAttempt = 0.f;
	FTimerHandle BurnTimerHandle;

	/** How many chain-spread hops this ignition is from an origin fire. Set once, in Ignite(), from
	 *  whatever generation the caller passes (0 for every caller except TrySpread() itself). Not
	 *  replicated - purely a server-side spread-pass concern, like SecondsSinceSpreadAttempt. */
	int32 SpreadGeneration = 0;
};
