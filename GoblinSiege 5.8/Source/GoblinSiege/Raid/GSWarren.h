// The Warren (GDD 6 and 9, ledger ruling 18): the hole in the ground the horde climbs out of, and
// the place loot goes home through. N_ChaosRune2 is its face.
//
// TWO JOBS, NOT ONE, and keeping them in one actor is the whole point of ruling 18:
//
//   ARRIVAL. Summoned goblins emerge HERE rather than fading in at a Marker.HordeArrival. That
//   satisfies the settled no-pop-in mandate - "watching them arrive is the joke" - and it is why
//   the 2026-08-07 ruling says the treeline sprint must never be restored.
//
//   BANKING. Anything with a LootValue that reaches this circle is banked permanently, the instant
//   it arrives. Deliberately forgiving: a raid that ends in a wipe still keeps what it carried home.
//
// ---- WHAT THIS ACTOR MUST NEVER DO -----------------------------------------------------------
// BANK A DEED. Not one, not ever, not behind a flag. GDD 9: "a second place to bank deeds would
// erase the reason to ever risk the run home. That tension is the loop." UGSScoreSubsystem::AddDeeds
// must not appear anywhere in GSWarren.cpp, and a future agent reading "the Warren banks" as "the
// Warren banks everything" is exactly the mistake this paragraph exists to stop.
//
// ---- PLACED, NOT PLANTED (Michael, 2026-08-20) ------------------------------------------------
// The design carried a planting channel and a "digger" goblin who stayed behind to hold the hole
// open. Both are cut: the Warren is "just a magic spot where they can summon goblins and drop off
// loot", placed by a designer and open from BeginPlay. There is therefore no channel, no cost and
// no dig here.
//
// UGSHordeSubsystem::NotifyGoblinSpentOnWarren() survives that cut UNCALLED and must not be
// deleted - it is the fourth pool exit (spent, but not dead: no death path, no credit back), and
// the accounting is correct the day a digger ever lands.
//
// ---- WHY A UCLASS AND NOT A TAG ---------------------------------------------------------------
// GSRaidMarker.h:20-26 argues hard against new UCLASSes, and "AGSHordeSpawnMarker will NEVER be
// built" still stands. That rule is about marker TYPES - "the marker says where, and nothing else".
// This is behaviour: an overlap volume, a banking rule, two derived transforms and a Blueprint hook.
// AGSRunicSite made the same call for the same reason, and this class is deliberately its twin so
// that whoever fixes a spawn bug in one goes looking in the other.
//
// The actor owns STATE and OVERLAP only. The rune is assigned on BP_GS_Warren, composing
// Content/MagicRuneVFX (N_ChaosRune2). No VFX is authored here; the pack ships no Blueprints, so
// the Blueprint is the composition step - exactly as BP_GS_RunicSite composes the Portal 4 set.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSWarren.generated.h"

class USphereComponent;
class UGSLootBankComponent;
class UStaticMeshComponent;
class UNiagaraComponent;
class APawn;

/** Something was swallowed. Points is what it was worth; Source is the actor that went in.
 *
 *  Source is STILL ALIVE when this fires and is destroyed immediately afterwards, so a listener
 *  may read its location, class or mesh to place a burst - but must not keep the pointer.
 *  See UGSLootBankComponent::CommitBank for why the order is that way round - the rule moved
 *  there when the runic site became the second consumer. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSOnLootBanked, AActor*, Source, int32, Points);

UCLASS()
class GOBLINSIEGE_API AGSWarren : public AActor
{
	GENERATED_BODY()

public:
	AGSWarren();

	/**
	 * Nearest Warren to a point, or null if the level has none.
	 *
	 * Static and iterating, like AGSRaidMarker::GatherByType and UGSHordeSubsystem's own runic-site
	 * search: there are at most a handful of these in a level and no registry is worth the
	 * lifetime management it would cost. Call it once and cache - never from a per-frame getter.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Warren", meta = (WorldContext = "WorldContextObject"))
	static AGSWarren* FindNearest(const UObject* WorldContextObject, FVector From);

	/** As FindNearest, but only Warrens that answer the horn. */
	static AGSWarren* FindNearestArrivalMouth(const UObject* WorldContextObject, FVector From);

	/** As FindNearest, but only Warrens that take respawns. */
	static AGSWarren* FindNearestRespawnPoint(const UObject* WorldContextObject, FVector From);

	/**
	 * Where a summoned goblin climbs out.
	 *
	 * Faces the Warren's own yaw, so UGSHordeSubsystem::SummonWave's lateral fan spreads a blast
	 * ACROSS the mouth rather than into it.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Warren")
	FTransform GetArrivalTransform() const;

	/**
	 * Where the player respawns (GDD 9 - the Warren becomes the respawn point).
	 *
	 * Unlike the runic site there is no offset to clear: banking consumes cargo rather than ending
	 * the raid, so standing in your own Warren is safe and spawning on it is the whole point.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Warren")
	FTransform GetSpawnTransform() const;

	/**
	 * Bank whatever this pawn is carrying. SERVER ONLY. Returns the points banked - 0 if it was
	 * carrying nothing, or carrying something with no LootValue.
	 *
	 * Public because the runic site wants the identical seam when mid-raid banking lands there.
	 * GDD 9 names that defect explicitly: "build it once and give it both consumers."
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Warren")
	int32 BankCarriedLoot(APawn* Pawn);

	/** Total banked at THIS Warren. The authoritative running total is UGSScoreSubsystem's; this
	 *  exists so "the Warren is not banking" and "the score is not counting" can be told apart
	 *  without attaching a debugger - they look identical in play and have different causes. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Warren")
	int32 GetPointsBankedHere() const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Warren")
	bool IsArrivalMouth() const { return bIsArrivalMouth; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Warren")
	bool IsRespawnPoint() const { return bIsRespawnPoint; }

	/** Fires on the server only - banking is not predicted (this module's multiplayer posture).
	 *  Hook the swallow burst, the gulp, an Overlord bark. The C++ has no opinion about any of it. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Warren")
	FGSOnLootBanked OnLootBanked;

	/** One line for GS.Warren.Status. */
	FString DescribeStatus() const;

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

protected:
	virtual void BeginPlay() override;

	/**
	 * Bank a loose actor lying in the mouth - a sack a courier put down, a pig dropped on the way in.
	 *
	 * This is why the courier half needs no change to BTTask_DeliverCargo: that task already calls
	 * PutDown() at the delivery location, and pointing UGSHordeSubsystem::ResolveDeliveryLocation
	 * here makes the drop land inside this sphere. One rule, three consumers - the player, the
	 * courier, and anything thrown in.
	 *
	 * A thin forwarder now - see LootBank. The rule is unchanged; only its address is.
	 */
	int32 BankLooseActor(AActor* Actor);

	/** Forwards the component's banked-something notification onto this actor's own delegate, so
	 *  Blueprints already bound to AGSWarren::OnLootBanked keep firing after the migration. */
	UFUNCTION()
	void HandleLootBanked(AActor* Source, int32 Points);

	// ------------------------------------------------------------------ components

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Warren")
	TObjectPtr<USceneComponent> WarrenRoot;

	/**
	 * The banking circle.
	 *
	 * Deliberately TIGHTER than the runic site's 1200uu. That radius is generous because extraction
	 * is the "you made it" moment and a portal you can sprint past is a bug report. This is the
	 * opposite case: a Warren you cannot walk near without losing the pig you were carrying past it
	 * is the bug report here.
	 */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Warren")
	TObjectPtr<USphereComponent> BankingSphere;

	/**
	 * The banking rule itself, shared with the runic site.
	 *
	 * GDD 9 asked for exactly this - "build it once and give it both consumers" - and for a while
	 * there were two copies: this actor grew one first, and #255 lifted it into a component for the
	 * portal without disturbing verified behaviour. This is the second half of that job. The public
	 * surface of AGSWarren is deliberately unchanged; only the implementation moved.
	 */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Warren")
	TObjectPtr<UGSLootBankComponent> LootBank;

	/** Left empty on the Blueprint, like BP_GS_RunicSite's PortalMesh: the rune is the visual. Kept
	 *  so a mouth mesh can be dropped in later without a rebuild. */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Warren")
	TObjectPtr<UStaticMeshComponent> MouthMesh;

	/** N_ChaosRune2 on the Blueprint. bAutoActivate stays ON - unlike the portal, which must not
	 *  blaze before it opens, a Warren that is not burning is a Warren the player cannot find. */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Warren")
	TObjectPtr<UNiagaraComponent> WarrenFX;

	// ------------------------------------------------------------------ tuning

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Warren|Tuning", meta = (ClampMin = "100.0"))
	float BankingRadius = 500.f;

	/** Serve as the horde's arrival mouth. Off makes a decorative rune - a banking-only Warren,
	 *  which the design does not ask for but a level might. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Warren|Tuning")
	bool bIsArrivalMouth = true;

	/** Take over player respawn from the runic site. GDD 9 says yes; a Warren placed somewhere
	 *  hostile might want to say no. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Warren|Tuning")
	bool bIsRespawnPoint = true;

	/** Accept loot. There is deliberately no matching bBanksDeeds and there must never be one. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Warren|Tuning")
	bool bBanksLoot = true;

private:

};
