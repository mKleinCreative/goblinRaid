// Squad-shared raid state: the alarm meter, alarm phase, reinforcement tier, raid clock and the
// razed flag all live here, replicated, with AddAlarm() as the single server-side mutator
// (design doc §4, §8.1, §9; tech doc §4, §19, §22).
// Reconstructed 2026-07-19 to match the surviving GSGameState.cpp and its callers exactly.
// 2026-07-28 (Block C): + EGSAlarmPhase state machine, fire-seen promotion with the ~10s unseen
// fuse, and the 30-minute raid clock with its 90s collapse window.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Alarm/GSAlarmTypes.h"
#include "GSGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnAlarmChanged, float, Alarm01);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnHordeWaveTriggered, EGSReinforcementTier, NewTier);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnDistrictRazedChanged, bool, bRazed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSOnAlarmPhaseChanged, EGSAlarmPhase, NewPhase, EGSAlarmPhase, OldPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnRaidClockPhaseChanged, EGSRaidClockPhase, NewPhase);

UCLASS()
class GOBLINSIEGE_API AGSGameState : public AGameStateBase
{
	GENERATED_BODY()

protected:
	/**
	 * ACF's team manager (#229). Added as a COMPONENT rather than by reparenting this class onto
	 * AACFGameState: the team subsystem only ever does
	 * `GameState->FindComponentByClass<UACFTeamManagerComponent>()`
	 * (ACFTeamManagerSubsystem.cpp:31), so it does not care what our GameState derives from - and
	 * AACFGameState derives from AGameState rather than AGameStateBase, which would have dragged in
	 * match-state machinery this project does not use.
	 *
	 * Without this, every run logged `Missing Team Config - UACFTeamManagerComponent` and every
	 * team lookup fell through to the default attitude.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|ACF")
	TObjectPtr<class UACFTeamManagerComponent> TeamManagerComponent;

public:
	AGSGameState();

	virtual void Tick(float DeltaSeconds) override;

	// ---------------------------------------------------------------- alarm meter

	/** Server-only, and the ONLY alarm mutator. Negative amounts allowed (firefighting pays the
	 *  meter back down - see BTTask_Firefight). Crossing MaxAlarm triggers a horde wave and
	 *  escalates the reinforcement tier. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Alarm")
	void AddAlarm(float Amount, EGSAlarmSource Source);

	/** Server-only. "A razed city stops producing defenders" - silences spawners (design doc §4). */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Alarm")
	void SetDistrictRazed(bool bRazed);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Alarm")
	float GetAlarm01() const { return MaxAlarm > 0.f ? Alarm / MaxAlarm : 0.f; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Alarm")
	EGSReinforcementTier GetReinforcementTier() const { return ReinforcementTier; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Alarm")
	bool IsDistrictRazed() const { return bDistrictRazed; }

	// ---------------------------------------------------------------- alarm phase

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Alarm")
	EGSAlarmPhase GetAlarmPhase() const { return AlarmPhase; }

	/**
	 * Server-only. Requests a phase. Monotonic from Raid upward - a request to drop below Raid
	 * once Raid is reached is ignored, because goblins don't de-escalate a town (design doc §8.1).
	 * Quiet <-> Suspicious is the only reversible edge, and DecayToQuiet() owns that direction.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Alarm")
	void RequestAlarmPhase(EGSAlarmPhase NewPhase, EGSAlarmSource Source);

	/**
	 * Server-only. Call when a human *confirms* a goblin sighting (the 1.5s confirm-hold landed).
	 * Promotes Quiet -> Suspicious. Does not touch Raid.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Alarm")
	void ReportConfirmedSighting();

	/**
	 * Server-only. Call when any objective/structure ignites. Starts the unseen-fire fuse if it
	 * isn't already running: even a blaze nobody witnessed promotes the town to Raid after
	 * UnseenFireFuseSeconds (design doc §8.1, decision 19). Idempotent - the first fire arms it.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Alarm")
	void ReportFireStarted();

	/**
	 * Server-only. Call when a human actually sees a fire. Promotes to Raid immediately and
	 * cancels the fuse - the fuse is only a backstop for fires nobody witnessed.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Alarm")
	void ReportFireSeenByHuman();

	/** True once any fire has been reported this raid - gates the "First Spark" scoring bonus
	 *  and the ambient-audio swap. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Alarm")
	bool HasAnyFireStarted() const { return bAnyFireStarted; }

	// ---------------------------------------------------------------- raid clock

	/** Server-only. Starts the 30-minute countdown. Called by GameMode once the raid begins. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Clock")
	void StartRaidClock();

	/**
	 * Freeze the clock. Called by UGSRaidDirector::EndRaid, because ending a raid did not stop time.
	 *
	 * Observed 2026-08-06 (#049): after `RAID ENDED: OutOfLives` the clock kept counting - 1769s then
	 * 1762s across two status reads - so a lost raid carried on ticking toward a collapse phase it had
	 * no business entering. `LeftBehind` only looked correct because there the clock expiring is what
	 * ended the raid.
	 *
	 * Halts rather than forcing a phase: the phase stays TRUE (a raid lost with 29 minutes left really
	 * was Running when it ended), and the HUD shows the time it stopped at. Setting Expired here would
	 * make every loss claim the portal collapsed, which is a different ending.
	 */
	void StopRaidClock();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Clock")
	EGSRaidClockPhase GetRaidClockPhase() const { return RaidClockPhase; }

	/**
	 * DEBUG ONLY. Fast-forward the clock to the edge of whatever phase it is in.
	 *
	 * Exists because the LeftBehind lose path is otherwise untestable: it needs a 30-minute raid
	 * plus a 90-second collapse window to elapse honestly, so nobody ever ran it, and a lose
	 * condition nobody has run is a lose condition nobody knows works. This drives the REAL
	 * transition - it only moves the counter, and TickRaidClock still decides every phase change.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Clock|Debug")
	void DebugExpireClock();

	/** Seconds until 0:00. Zero once the clock has expired into the collapse window. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Clock")
	float GetRaidSecondsRemaining() const { return FMath::Max(0.f, RaidSecondsRemaining); }

	/** Seconds left in the 90s collapse grace window; 0 unless Collapsing. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Clock")
	float GetCollapseSecondsRemaining() const { return FMath::Max(0.f, CollapseSecondsRemaining); }

	// ---------------------------------------------------------------- events

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Alarm")
	FGSOnAlarmChanged OnAlarmChanged;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Alarm")
	FGSOnHordeWaveTriggered OnHordeWaveTriggered;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Alarm")
	FGSOnDistrictRazedChanged OnDistrictRazedChanged;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Alarm")
	FGSOnAlarmPhaseChanged OnAlarmPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Clock")
	FGSOnRaidClockPhaseChanged OnRaidClockPhaseChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** Escalates the tier, resets Alarm to a partial value, and broadcasts. GameMode/AI-director
	 *  listens to actually spawn the wave - GameState owns the number, not the spawn logic. */
	void TriggerHordeWave();

	/** Server-only. Suspicious -> Quiet after SuspicionDecaySeconds with nothing confirmed. */
	void DecayToQuiet();

	/** Server-only. The unseen-fire fuse firing: nobody saw it, but smoke announces itself. */
	void HandleUnseenFireFuse();

	void SetAlarmPhaseInternal(EGSAlarmPhase NewPhase);
	void SetRaidClockPhaseInternal(EGSRaidClockPhase NewPhase);
	void TickRaidClock(float DeltaSeconds);

	UFUNCTION()
	void OnRep_Alarm();

	UFUNCTION()
	void OnRep_ReinforcementTier();

	UFUNCTION()
	void OnRep_DistrictRazed();

	UFUNCTION()
	void OnRep_AlarmPhase(EGSAlarmPhase OldPhase);

	UFUNCTION()
	void OnRep_RaidClockPhase();

	// ---------------------------------------------------------------- state

	UPROPERTY(ReplicatedUsing = OnRep_Alarm)
	float Alarm = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_AlarmPhase)
	EGSAlarmPhase AlarmPhase = EGSAlarmPhase::Quiet;

	UPROPERTY(ReplicatedUsing = OnRep_ReinforcementTier)
	EGSReinforcementTier ReinforcementTier = EGSReinforcementTier::Tier0_Baseline;

	UPROPERTY(ReplicatedUsing = OnRep_DistrictRazed)
	bool bDistrictRazed = false;

	UPROPERTY(ReplicatedUsing = OnRep_RaidClockPhase)
	EGSRaidClockPhase RaidClockPhase = EGSRaidClockPhase::NotStarted;

	/** Set by StopRaidClock; TickRaidClock early-outs on it. Replicated so a client's HUD stops too -
	 *  without it the clock would freeze on the server and keep counting on every client. */
	UPROPERTY(Replicated)
	bool bRaidClockHalted = false;

	UPROPERTY(Replicated)
	float RaidSecondsRemaining = 0.f;

	UPROPERTY(Replicated)
	float CollapseSecondsRemaining = 0.f;

	/** Server-side only; drives the First Spark bonus and the fuse's armed state. */
	bool bAnyFireStarted = false;

	// ---------------------------------------------------------------- tuning

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Alarm|Tuning")
	float MaxAlarm = 100.f;

	/** Optional slow background pressure; 0 disables (stealth-lite phases handle pacing instead). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Alarm|Tuning")
	float PassiveAlarmPerSecond = 0.f;

	/** After a horde wave fires, Alarm resets to MaxAlarm * this - partial, so pressure resumes. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Alarm|Tuning")
	float PostHordeResetFraction = 0.4f;

	/** Suspicious falls back to Quiet after this long with no confirmation (design doc §8.1). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Alarm|Tuning")
	float SuspicionDecaySeconds = 20.f;

	/** Decision 19: an unwitnessed fire still promotes the town to Raid after this long. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Alarm|Tuning")
	float UnseenFireFuseSeconds = 10.f;

	/** Design doc §9: hard 30-minute cap. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Clock|Tuning")
	float RaidDurationSeconds = 1800.f;

	/** The Overlord's whispers turn impatient inside this window. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Clock|Tuning")
	float FinalWarningSeconds = 300.f;

	/** 0:00 -> portal collapse grace window before anyone outside is left behind. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Clock|Tuning")
	float CollapseGraceSeconds = 90.f;

	FTimerHandle SuspicionDecayHandle;
	FTimerHandle UnseenFireFuseHandle;
};
