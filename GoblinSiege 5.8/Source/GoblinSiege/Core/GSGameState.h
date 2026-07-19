// Squad-shared raid state: the alarm meter, reinforcement tier, and the razed flag all live here,
// replicated, with AddAlarm() as the single server-side mutator (design doc §4; tech doc §4).
// Reconstructed 2026-07-19 to match the surviving GSGameState.cpp and its callers exactly.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Alarm/GSAlarmTypes.h"
#include "GSGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnAlarmChanged, float, Alarm01);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnHordeWaveTriggered, EGSReinforcementTier, NewTier);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnDistrictRazedChanged, bool, bRazed);

UCLASS()
class GOBLINSIEGE_API AGSGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AGSGameState();

	virtual void Tick(float DeltaSeconds) override;

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

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Alarm")
	FGSOnAlarmChanged OnAlarmChanged;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Alarm")
	FGSOnHordeWaveTriggered OnHordeWaveTriggered;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Alarm")
	FGSOnDistrictRazedChanged OnDistrictRazedChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** Escalates the tier, resets Alarm to a partial value, and broadcasts. GameMode/AI-director
	 *  listens to actually spawn the wave - GameState owns the number, not the spawn logic. */
	void TriggerHordeWave();

	UFUNCTION()
	void OnRep_Alarm();

	UFUNCTION()
	void OnRep_ReinforcementTier();

	UFUNCTION()
	void OnRep_DistrictRazed();

	UPROPERTY(ReplicatedUsing = OnRep_Alarm)
	float Alarm = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Alarm|Tuning")
	float MaxAlarm = 100.f;

	/** Optional slow background pressure; 0 disables (stealth-lite phases handle pacing instead). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Alarm|Tuning")
	float PassiveAlarmPerSecond = 0.f;

	/** After a horde wave fires, Alarm resets to MaxAlarm * this - partial, so pressure resumes. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Alarm|Tuning")
	float PostHordeResetFraction = 0.4f;

	UPROPERTY(ReplicatedUsing = OnRep_ReinforcementTier)
	EGSReinforcementTier ReinforcementTier = EGSReinforcementTier::Tier0_Baseline;

	UPROPERTY(ReplicatedUsing = OnRep_DistrictRazed)
	bool bDistrictRazed = false;
};
