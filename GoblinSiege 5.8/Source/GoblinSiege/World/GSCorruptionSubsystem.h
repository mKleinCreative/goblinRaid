// World corruption - the land turns to Mordor as you raid. Ledger rulings 40-45 (2026-08-21) and 62
// (2026-08-24). Written 2026-08-25, ticket #309. Stage 1 of 6: the scalar and the output stage only,
// driven by console. No drivers are wired here - that is stage 2 onward.
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
#include "GSCorruptionSubsystem.generated.h"

// ONE declaration, shared. Both GSCorruptionSubsystem.cpp and GSCorruptionDirector.cpp had
// their own DEFINE_LOG_CATEGORY_STATIC(LogGSCorruption) - which is fine in isolation and a
// redefinition the moment the unity build puts the two into one translation unit. That is the
// C2011 that stopped the whole module compiling. Declared here, defined once in the subsystem.
DECLARE_LOG_CATEGORY_EXTERN(LogGSCorruption, Log, All);

class AGSCorruptionDirector;

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

private:
	void TickCorruption();
	float ComputeTarget01() const;
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
