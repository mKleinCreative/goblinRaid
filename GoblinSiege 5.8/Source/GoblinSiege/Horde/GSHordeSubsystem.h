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
// queries per frame feeding a HandlePerceptionUpdated that does nothing at all. The threat registry
// below is the replacement: things that matter announce themselves once, and every goblin reads the
// same resolved answer.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSHordeSubsystem.generated.h"

class AGSHordeGoblin;
class AGSCharacterBase;
class AController;

/** Reserve remaining, active count, cap. Everything a HUD counter or a bark trigger needs. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGSOnHordePoolChanged, int32, ReserveRemaining, int32, ActiveCount, int32, ActiveCap);

/** The treeline is silent - fired once, when the reserve first reaches zero. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnHordePoolDry);

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

	// ------------------------------------------------------------------ the stimulus bus

	/**
	 * Announce a hostile worth swarming. Called from the Frenzy hooks on AGSCharacterBase
	 * (OnDamaged / OnDealtDamage) and from the point command - never from a per-goblin sense.
	 *
	 * Idempotent: re-registering an existing threat refreshes its timestamp rather than stacking.
	 */
	void RegisterThreat(AActor* Threat, AActor* Provoker);

	void UnregisterThreat(AActor* Threat);

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

private:
	struct FGSHordeThreat
	{
		TWeakObjectPtr<AActor> Threat;
		TWeakObjectPtr<AActor> Provoker;
		float LastRefreshedTime = 0.f;
	};

	/** Keyed by summoning controller from day one. In the single-player slice there is exactly one
	 *  key and this is indistinguishable from a flat list - which is the point. The repo GDD says
	 *  "cap 10 active PER PLAYER" and the canonical doc says a flat 10; they agree until co-op, and
	 *  retrofitting a key after the point command, the Warren and the couriers all read
	 *  GetActiveCount() is the expensive version of this decision. */
	TMap<TWeakObjectPtr<AController>, TArray<TWeakObjectPtr<AGSHordeGoblin>>> ActiveGoblins;

	TArray<FGSHordeThreat> Threats;

	int32 ReserveRemaining = 0;

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
