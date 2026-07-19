#include "Core/GSGameState.h"
#include "Net/UnrealNetwork.h"

AGSGameState::AGSGameState()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f; // alarm doesn't need per-frame precision
}

void AGSGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	if (PassiveAlarmPerSecond > 0.f)
	{
		AddAlarm(PassiveAlarmPerSecond * DeltaSeconds, EGSAlarmSource::PassiveTick);
	}
}

void AGSGameState::AddAlarm(float Amount, EGSAlarmSource Source)
{
	if (!HasAuthority() || Amount == 0.f)
	{
		return;
	}

	// FireExtinguishedByDefenders should be called with a negative Amount by the firefighting
	// BT task - see design doc §4 "Defenders fight the fires."
	Alarm = FMath::Clamp(Alarm + Amount, 0.f, MaxAlarm);
	OnRep_Alarm();

	if (Alarm >= MaxAlarm)
	{
		TriggerHordeWave();
	}
}

void AGSGameState::TriggerHordeWave()
{
	ReinforcementTier = static_cast<EGSReinforcementTier>(
		FMath::Min<uint8>(static_cast<uint8>(ReinforcementTier) + 1,
			static_cast<uint8>(EGSReinforcementTier::Tier3_LastStand)));
	OnRep_ReinforcementTier();

	Alarm = MaxAlarm * PostHordeResetFraction;
	OnRep_Alarm();

	OnHordeWaveTriggered.Broadcast(ReinforcementTier);
	// GSGameMode (or an AI Director subsystem) listens to this to actually spawn the wave -
	// GameState just owns the number, not the spawn logic.
}

void AGSGameState::SetDistrictRazed(bool bRazed)
{
	if (!HasAuthority() || bDistrictRazed == bRazed)
	{
		return;
	}

	bDistrictRazed = bRazed;
	OnRep_DistrictRazed();
}

void AGSGameState::OnRep_Alarm()
{
	OnAlarmChanged.Broadcast(GetAlarm01());
}

void AGSGameState::OnRep_ReinforcementTier()
{
	// Intentionally left for HUD/AI-Director Blueprint hooks; C++ side already broadcasts
	// OnHordeWaveTriggered from the server path in TriggerHordeWave().
}

void AGSGameState::OnRep_DistrictRazed()
{
	OnDistrictRazedChanged.Broadcast(bDistrictRazed);
}

void AGSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGSGameState, Alarm);
	DOREPLIFETIME(AGSGameState, ReinforcementTier);
	DOREPLIFETIME(AGSGameState, bDistrictRazed);
}
