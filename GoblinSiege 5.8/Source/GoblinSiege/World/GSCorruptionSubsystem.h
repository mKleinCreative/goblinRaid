// World corruption - the land turns to Mordor as you raid. Ledger rulings 40-45 (2026-08-21) and 62
// (2026-08-24). Written 2026-08-25 (#309: the scalar and the output stage), extended #312 (the four
// passive drivers) and #320 (per-TYPE objective weighting).
//
// ALL FIVE DRIVERS ARE WIRED: objectives, kills, structures destroyed, raid clock, horde presence,
// plus the razed floor. Corruption is reachable at 1.00 from gameplay alone as of #327.
//
// The kill term is WEIGHTED, not counted (ruling 62): a civilian death corrupts the land more than
// an armed defender's, because that is the first thing in this game that makes an ugly choice a real
// one. The multiplier is deliberately NOT in the ruling - it is a feel number living on
// GS.Corruption.CivilianWeight until it settles.
//
// WHY A WORLD SUBSYSTEM, and not AGSGameState: the instinct to put this beside Alarm is right in
// shape and wrong in substance (see THE SAWTOOTH below). Beyond that, this value churns at 10 Hz and
// is client-local cosmetic - Q-36 says replicate cheap ROOT state only - and it will end up bound to
// roughly eight delegates across five subsystems, where Deinitialize is a clean unbind point and
// AGameStateBase::EndPlay is not. UGSBurnMaskSubsystem, UGSHordeSubsystem, UGSRaidDirector and
// UGSScoreSubsystem all made the same call for the same lifecycle reason, and the PCG hamlets mean
// anything that needs hand-placing in a level does not exist on the maps this has to work on.
//
// ---- THE SAWTOOTH (ruling 42) ---------------------------------------------------------------
// DO NOT make corruption a read of AGSGameState::GetAlarm01(). It is the obvious simplification and
// it is wrong in a way that hides: AGSGameState::TriggerHordeWave resets Alarm to
// MaxAlarm * PostHordeResetFraction (0.4) on every wave - GSGameState.cpp:82, .h:239. Corruption
// derived from it would UN-MORDOR THE WORLD every time reinforcements arrive, and the bug does not
// appear until the second wave, by which time nobody is looking at the alarm meter.
//
// For the same reason DO NOT drive it from UGSScoreSubsystem::GetDeeds(). GDD 12.1 row 12: any burn
// objective pays deeds, so ~30 houses swamp the tally, and burning houses would darken the sky
// faster than detonating the mill.
//
// ---- MONOTONIC IS A CORRECTNESS CONSTRAINT, NOT A TASTE CALL (ruling 41) ---------------------
// Corruption ratchets: Target is floored at the high-water mark and never falls below it. That
// follows GDD 8 doctrine ("no alarm reducers - goblins do not de-escalate, they leave"), but the
// binding reason is UGSBurnMaskSubsystem: its R channel is monotonic BY CONSTRUCTION, so a global
// scalar that falls freely would render a clean blue sky over permanently black ground. The two
// must not be able to disagree.
//
// ---- WHY THE FOLLOWER IS CONSTANT-RATE, NOT EXPONENTIAL -------------------------------------
// FInterpConstantTo, never FInterpTo. An exponential follower's speed is proportional to its error,
// so a 0.15 objective step and a 0.02 kill step would settle in the SAME time - the exact opposite
// of "the world lurches when the mill goes up and creeps as you kill". At 0.06/s an objective step
// reads as a ~2.5s lurch and a kill as a ~0.3s creep you feel but cannot point at.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "GSCorruptionSubsystem.generated.h"

// ONE declaration, shared. Both GSCorruptionSubsystem.cpp and GSCorruptionDirector.cpp had
// their own DEFINE_LOG_CATEGORY_STATIC(LogGSCorruption) - which is fine in isolation and a
// redefinition the moment the unity build puts the two into one translation unit. That is the
// C2011 that stopped the whole module compiling. Declared here, defined once in the subsystem.
DECLARE_LOG_CATEGORY_EXTERN(LogGSCorruption, Log, All);

class AGSCorruptionDirector;
class AGSCharacterBase;
class UGSCorruptionDataAsset;

/** The display value, every time it changes. For HUD tints, bark triggers, anything continuous. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnCorruptionChanged, float, Corruption01);

/**
 * Crossed a named band. Deliberately shaped (New, Old) to match AGSGameState's
 * FGSOnAlarmPhaseChanged - a second world meter with a DIFFERENT public shape is the real cost of
 * getting this wrong, so the accessor and the delegate mirror the alarm meter exactly.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSOnCorruptionStageChanged, int32, NewStage, int32, OldStage);

UCLASS(Config = Game)
class GOBLINSIEGE_API UGSCorruptionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Game worlds only - and note this is NOT UGSBurnMaskSubsystem's predicate. BurnMask refuses to
	 * exist on a dedicated server because it is purely cosmetic; corruption is cosmetic on the
	 * OUTPUT side and authoritative gameplay state on the DRIVER side, so copying that predicate
	 * would silently kill the drivers on a server. The output half early-outs inside the director.
	 */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Not Initialize: level actors are not reliably present during world initialisation. */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Null-safe. Mirrors UGSRaidDirector::Get and UGSHordeSubsystem::Get. */
	static UGSCorruptionSubsystem* Get(const UObject* WorldContextObject);

	/** What the world is actually rendering: the follower, or the override if one is latched. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Corruption")
	float GetCorruption01() const;

	/** Where the drivers are pulling. Equal to GetCorruption01() once it has settled. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Corruption")
	float GetCorruptionTarget01() const;

	/** 0 Quiet, 1 Scarred, 2 Burning, 3 Mordor. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Corruption")
	int32 GetCorruptionStage() const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Corruption")
	FString GetStageName() const;

	/**
	 * Debug/cheat, and the co-op seam. Latches until ReleaseCorruptionOverride(); the drivers keep
	 * accumulating underneath so releasing does not snap backwards to a stale value.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Corruption")
	void SetCorruptionOverride(float Value01);

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Corruption")
	void ReleaseCorruptionOverride();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Corruption")
	bool IsOverridden() const;

	/** Advance to the next quarter band. Lets any stage be inspected without playing to it. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Corruption")
	void StepCorruptionStage();

	/**
	 * A prop was smashed, toppled or burned to the ground. Called DIRECTLY by the three
	 * Destruction/ components rather than bound to their delegates - the same choice, for the same
	 * reason, that GSTopplableComponent.cpp:211-215 already documents for scoring: those call sites
	 * are first-wins guarded, so reporting inside them cannot double-count, whereas a subscriber can
	 * silently be bound twice. It also survives an actor spawned after BeginPlay, which a
	 * bind-on-sweep would not.
	 */
	void ReportStructureDestroyed(const AActor* Structure);

	/** Re-run the director's actor discovery, after streaming or a PCG spawn. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Corruption")
	void RefreshOutputs();

	/**
	 * The instrument. Prints the scalar AND what the engine actually has, because two of this
	 * feature's worst failure modes - a Static sun, a director that never resolved - are completely
	 * silent at runtime. A value reading zero must be distinguishable from a value that is not wired.
	 */
	FString DescribeState() const;

	AGSCorruptionDirector* GetDirector() const;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Corruption")
	FGSOnCorruptionChanged OnCorruptionChanged;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Corruption")
	FGSOnCorruptionStageChanged OnCorruptionStageChanged;

	// ---- tuning (Config = Game; a subsystem has no CDO, so Config is its EditDefaultsOnly) ----

	/**
	 * Term weights. They MUST sum to 1.0 - a subsystem has no CDO, so a hand-edited ini is the only
	 * way these change, and a set that sums to 0.8 silently caps the world at 80% corrupted forever.
	 * OnWorldBeginPlay checks the sum and complains loudly rather than renormalising, because a
	 * silent correction would hide the typo that caused it.
	 *
	 * NOTE the kill weight is live in this build but its term always reads 0.00 - the kill hook is
	 * stage 3. Max reachable corruption until then is 0.80, BY DESIGN. GS.Corruption.Dump says so on
	 * the kill line so nobody reads it as a bug.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Corruption|Weights")
	float WeightObjectives = 0.45f;

	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Corruption|Weights")
	float WeightKills = 0.20f;

	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Corruption|Weights")
	float WeightStructures = 0.15f;

	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Corruption|Weights")
	float WeightClock = 0.10f;

	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Corruption|Weights")
	float WeightHorde = 0.10f;

	/**
	 * Soft knee for the structure count: Score = N / (N + Knee). Saturating on purpose - 20 smashed
	 * props is half the term, and no amount of barrel-smashing ever maxes it.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Corruption|Weights", meta = (ClampMin = "1"))
	float StructureSoftKnee = 20.f;

	/**
	 * Soft knee for WEIGHTED kills - civilians count more than soldiers, so this is not a body count.
	 *
	 * DELIBERATELY UNSIZED. It was drafted at 12 against ruling 19's finite 15-defender pool, and the
	 * 2026-08-23 roster ruling then made the castle guards Militia with "just a decent amount of
	 * them" - so 12 saturates too early. I could not count the real roster from outside the editor
	 * (a .umap string scan gives reference counts, not instances), and guessing a second number from
	 * a number I know to be unreliable is worse than leaving it visible and tunable. Watch a full
	 * raid, read the kill line in GS.Corruption.Dump, then set this from what actually happened.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Corruption|Weights", meta = (ClampMin = "1"))
	float KillSoftKnee = 12.f;

	/**
	 * The archetype row that means "civilian" in DA_Race_Human, whose rows are Militia, Archer,
	 * Knight and Civilian. Config rather than a literal so renaming the row is a data change.
	 *
	 * An FName row key, NOT a class-name match: the project's own doctrine is "archetypes are data,
	 * not classes - one pawn class plus a GSRaceDataAsset row picked by ArchetypeRowName"
	 * (GSEnemyCharacter.h:1-2), so this is reading the canonical key, not sniffing a type.
	 *
	 * The cleaner home is FGSArchetypeDefinition::RoleTag, which exists and has no consumers - but
	 * populating it means editing DA_Race_Human, which needs an editor session. When that happens,
	 * this should become a tag test and this property should go.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Corruption|Weights")
	FName CivilianArchetypeRowName = TEXT("Civilian");

	/** Razed (or district-razed) floors the target here regardless of the terms. */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Corruption|Weights", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RazedFloor01 = 0.85f;

	/**
	 * How often the objectives term re-reads the roster. 2 Hz, not the subsystem's 10 Hz: a hamlet
	 * carries 71 objectives (L_Tutorial_Island) and iterating them ten times a second buys nothing -
	 * a burn completion moves over seconds, and the follower smooths the result anyway. Every other
	 * term is a couple of getters and stays on the fast tick.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Corruption", meta = (ClampMin = "0.0"))
	float ObjectiveRecomputeIntervalSeconds = 0.5f;

	/** 10 Hz. The output stage is a lerp, not a simulation - it does not want a per-frame tick. */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Corruption", meta = (ClampMin = "0.01"))
	float TickIntervalSeconds = 0.1f;

	/**
	 * Spawn an ExponentialHeightFog if the map has none. L_CombatArena - the default startup map and
	 * the only map anything since 2026-08-07 has been watched in - has no fog actor at all.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Corruption")
	bool bSpawnMissingAtmosphereActors = true;

	/** Off for a map with baked lighting, which cannot show a sun change anyway. See the director. */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Corruption")
	bool bDriveDirectionalLight = true;

	/**
	 * Soft path, never a hard TSubclassOf default: a BP subclass is how a level artist overrides the
	 * look per map. Empty falls back to the C++ class, which is correct and visible on its own.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Corruption")
	FSoftClassPath CorruptionDirectorClassPath;

	/**
	 * The tuning asset (stage 4). Soft path with a C++ default so the feature works on a fresh clone
	 * with no ini entry; an absent asset is a WARNING and the C++ defaults stand, never a silent
	 * zeroing - a missing data asset must not look like a broken feature.
	 *
	 * Structural values (this path, the tick interval, the director class) stay in Config because a
	 * subsystem has no CDO. Everything a designer judges by eye or by feel lives in the asset.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "GoblinSiege|Corruption")
	FSoftObjectPath CorruptionDataPath = FSoftObjectPath(TEXT("/Game/Data/World/DA_Corruption_Default.DA_Corruption_Default"));

private:
	/**
	 * Last computed value of each term, kept ONLY so GS.Corruption.Dump can show its working. A term
	 * reading 0.00 because nothing has happened and a term reading 0.00 because it is not wired look
	 * identical on screen, and confusing the two is how a broken driver survives a playtest.
	 */
	struct FGSCorruptionTerms
	{
		float Objectives = 0.f;
		float Kills = 0.f;
		float Structures = 0.f;
		float Clock = 0.f;
		float Horde = 0.f;
		int32 ObjectiveCount = 0;
		bool bRazedFloorApplied = false;
		bool bDirectorSeen = false;
	};
	FGSCorruptionTerms Terms;

	/**
	 * One row PER TYPE, not per carrier - which is both the fix and the instrument for it.
	 *
	 * The first version weighted per instance and normalised by total weight. L_Tutorial_Island
	 * showed what that actually does: 67 houses x 0.15 = 10.05 of a 13.40 total, so houses were
	 * **75% of the objectives term** despite carrying the lowest weight per instance. Burning a
	 * street of houses moved the sky three times as much as detonating the mill, the market and the
	 * field together - which is GDD 12.1 row 12's complaint ("~30 houses swamp the tally", and it is
	 * really 67) reappearing one level up, and exactly what ruling 42 exists to stop.
	 *
	 * Averaging within a type and weighting the four TYPE averages makes the term agree with the
	 * raid's own win condition (Q-32/Q-37, win by type-flags): houses become 0.15/2.50 = 6% of it.
	 *
	 * Lives on the subsystem rather than inside Terms so the snapshot copy does not carry an array.
	 */
	struct FGSObjectiveTypeRow
	{
		FGameplayTag TypeTag;
		int32 Count = 0;
		float Weight = 0.f;
		float AverageCompletion = 0.f;
	};
	TArray<FGSObjectiveTypeRow> ObjectiveBreakdown;

	/** Fills Terms and CorruptionTarget01. Non-const because the instrument needs the working. */
	void RecomputeTarget();

	/**
	 * The objectives term, recomputed on its own slower cadence. Separated because it is the only
	 * term whose cost scales with the level: 71 carriers iterated ten times a second on a hamlet,
	 * for a value that moves as slowly as a fire spreads. Every other term is a couple of getters.
	 */
	void RecomputeObjectiveTerm();
	float CachedObjectives01 = 0.f;
	int32 CachedObjectiveCount = 0;
	/** Starts high so the very first tick recomputes rather than reading 0.00 for half a second. */
	float ObjectiveRecomputeAccumulator = UE_BIG_NUMBER;

	/** Takes a float because kills are WEIGHTED (a civilian is worth more than one). */
	static float SoftKnee(float Count, float Knee);
	/** Non-static since #337: the weights come from the tuning asset when one is loaded. */
	float ObjectiveTypeWeight(const FGameplayTag& TypeTag) const;

	/** Copies the asset's values over the C++/Config defaults. Logs exactly what it overrode. */
	void ApplyTuningAsset();

	UPROPERTY(Transient)
	TObjectPtr<UGSCorruptionDataAsset> TuningData;

	/**
	 * Bound to AGSGameMode::OnCharacterKilled. UFUNCTION because a dynamic multicast delegate can
	 * only bind to reflected functions.
	 *
	 * Server-only by construction, with no HasAuthority() check anywhere: the bind itself goes
	 * through GetAuthGameMode, and AGSGameMode does not exist on a client. Nobody should "fix" that.
	 */
	UFUNCTION()
	void HandleCharacterKilled(AGSCharacterBase* Victim, FGameplayTag VictimRaceTag, FVector Location);

	int32 HumansKilled = 0;

	/** Subset of HumansKilled. Ruling 62: a civilian death corrupts the land more than a soldier's. */
	int32 CiviliansKilled = 0;

	int32 StructuresDestroyed = 0;

	/**
	 * AGSGameState::RaidDurationSeconds is private with no getter and GSGameState.h is not this
	 * ticket's file to claim, so the duration is self-calibrated: the largest remaining-time ever
	 * observed IS the duration, because the clock only counts down. No second copy of 1800 to drift.
	 */
	float RaidDurationObserved = 0.f;

	void TickCorruption();
	static int32 BandFor(float InCorruption01);
	void EnsureDirector();

	/** The smoothed follower - the only genuinely new state this subsystem owns. */
	float Corruption01 = 0.f;
	float CorruptionTarget01 = 0.f;

	/** The ratchet floor (ruling 41). Never decreases while the world lives. */
	float HighWaterMark01 = 0.f;

	float ForcedOverride01 = 0.f;
	bool bOverridden = false;

	int32 CurrentStage = 0;
	float LastBroadcast01 = -1.f;

	TWeakObjectPtr<AGSCorruptionDirector> Director;
	FTimerHandle CorruptionTickTimer;
};
