#include "Core/GSGameState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

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

	TickRaidClock(DeltaSeconds);
}

// ====================================================================== alarm meter

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

	// The razed flag and the Razed phase are the same event seen two ways; keep them in step so
	// nothing has to listen to both.
	if (bRazed)
	{
		RequestAlarmPhase(EGSAlarmPhase::Razed, EGSAlarmSource::ObjectiveProgress);
	}
}

// ====================================================================== alarm phase

void AGSGameState::RequestAlarmPhase(EGSAlarmPhase NewPhase, EGSAlarmSource Source)
{
	if (!HasAuthority() || NewPhase == AlarmPhase)
	{
		return;
	}

	// Monotonic from Raid up. "Alarm reducers: none. Goblins don't de-escalate, they leave."
	// Quiet <-> Suspicious is the only reversible edge and DecayToQuiet() owns going back.
	const bool bIsDowngrade = static_cast<uint8>(NewPhase) < static_cast<uint8>(AlarmPhase);
	const bool bLockedIn = AlarmPhase >= EGSAlarmPhase::Raid;
	if (bIsDowngrade && bLockedIn)
	{
		return;
	}

	SetAlarmPhaseInternal(NewPhase);
}

void AGSGameState::SetAlarmPhaseInternal(EGSAlarmPhase NewPhase)
{
	const EGSAlarmPhase OldPhase = AlarmPhase;
	AlarmPhase = NewPhase;

	UWorld* World = GetWorld();

	// Leaving Suspicious for any reason cancels the pending decay.
	if (World && OldPhase == EGSAlarmPhase::Suspicious)
	{
		World->GetTimerManager().ClearTimer(SuspicionDecayHandle);
	}

	// Entering Suspicious arms it.
	if (World && NewPhase == EGSAlarmPhase::Suspicious && SuspicionDecaySeconds > 0.f)
	{
		World->GetTimerManager().SetTimer(SuspicionDecayHandle, this,
			&AGSGameState::DecayToQuiet, SuspicionDecaySeconds, false);
	}

	// Once the town is properly alarmed the fuse has done its job.
	if (World && NewPhase >= EGSAlarmPhase::Raid)
	{
		World->GetTimerManager().ClearTimer(UnseenFireFuseHandle);
	}

	OnRep_AlarmPhase(OldPhase);
}

void AGSGameState::DecayToQuiet()
{
	if (!HasAuthority() || AlarmPhase != EGSAlarmPhase::Suspicious)
	{
		return;
	}

	SetAlarmPhaseInternal(EGSAlarmPhase::Quiet);
}

void AGSGameState::ReportConfirmedSighting()
{
	if (!HasAuthority())
	{
		return;
	}

	// A fresh sighting re-arms the decay even if we're already Suspicious.
	if (AlarmPhase == EGSAlarmPhase::Suspicious)
	{
		SetAlarmPhaseInternal(EGSAlarmPhase::Suspicious);
		return;
	}

	RequestAlarmPhase(EGSAlarmPhase::Suspicious, EGSAlarmSource::CombatNoise);
}

void AGSGameState::ReportFireStarted()
{
	if (!HasAuthority())
	{
		return;
	}

	bAnyFireStarted = true;

	// Already loud? Nothing to arm.
	if (AlarmPhase >= EGSAlarmPhase::Raid)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || UnseenFireFuseSeconds <= 0.f)
	{
		return;
	}

	// Idempotent: the first fire arms the fuse, later fires don't restart it. Decision 19 -
	// "even an unwitnessed blaze announces itself shortly."
	if (!World->GetTimerManager().IsTimerActive(UnseenFireFuseHandle))
	{
		World->GetTimerManager().SetTimer(UnseenFireFuseHandle, this,
			&AGSGameState::HandleUnseenFireFuse, UnseenFireFuseSeconds, false);
	}
}

void AGSGameState::ReportFireSeenByHuman()
{
	if (!HasAuthority())
	{
		return;
	}

	bAnyFireStarted = true;
	RequestAlarmPhase(EGSAlarmPhase::Raid, EGSAlarmSource::FireDamage);
}

void AGSGameState::HandleUnseenFireFuse()
{
	if (!HasAuthority())
	{
		return;
	}

	// Smoke over the treetops is its own witness.
	RequestAlarmPhase(EGSAlarmPhase::Raid, EGSAlarmSource::FireDamage);
}

// ====================================================================== raid clock

void AGSGameState::DebugExpireClock()
{
	if (!HasAuthority())
	{
		return;
	}

	// Leave a sliver rather than zeroing: TickRaidClock owns every phase transition, and handing it
	// a value it can decrement is what keeps this a test of the real code rather than of this
	// function. Collapsing is driven the same way, so one command walks Running -> Collapsing ->
	// Expired across three calls.
	if (RaidClockPhase == EGSRaidClockPhase::Collapsing)
	{
		CollapseSecondsRemaining = 0.05f;
	}
	else
	{
		RaidSecondsRemaining = 0.05f;
	}

	UE_LOG(LogTemp, Warning, TEXT("[GoblinSiege] GS.Raid.ExpireClock: phase=%d raid=%.2fs collapse=%.2fs"),
		static_cast<int32>(RaidClockPhase), RaidSecondsRemaining, CollapseSecondsRemaining);
}

void AGSGameState::StartRaidClock()
{
	if (!HasAuthority() || RaidClockPhase != EGSRaidClockPhase::NotStarted)
	{
		return;
	}

	RaidSecondsRemaining = RaidDurationSeconds;
	CollapseSecondsRemaining = 0.f;
	SetRaidClockPhaseInternal(EGSRaidClockPhase::Running);
}

void AGSGameState::TickRaidClock(float DeltaSeconds)
{
	switch (RaidClockPhase)
	{
	case EGSRaidClockPhase::Running:
	case EGSRaidClockPhase::FinalWarning:
	{
		RaidSecondsRemaining -= DeltaSeconds;

		if (RaidSecondsRemaining <= 0.f)
		{
			RaidSecondsRemaining = 0.f;
			CollapseSecondsRemaining = CollapseGraceSeconds;
			SetRaidClockPhaseInternal(EGSRaidClockPhase::Collapsing);
		}
		else if (RaidClockPhase == EGSRaidClockPhase::Running
			&& RaidSecondsRemaining <= FinalWarningSeconds)
		{
			SetRaidClockPhaseInternal(EGSRaidClockPhase::FinalWarning);
		}
		break;
	}
	case EGSRaidClockPhase::Collapsing:
	{
		CollapseSecondsRemaining -= DeltaSeconds;
		if (CollapseSecondsRemaining <= 0.f)
		{
			CollapseSecondsRemaining = 0.f;
			// GameMode listens for Expired to apply the left-behind rule (design doc §9).
			SetRaidClockPhaseInternal(EGSRaidClockPhase::Expired);
		}
		break;
	}
	default:
		break;
	}
}

void AGSGameState::SetRaidClockPhaseInternal(EGSRaidClockPhase NewPhase)
{
	if (RaidClockPhase == NewPhase)
	{
		return;
	}

	RaidClockPhase = NewPhase;
	OnRep_RaidClockPhase();
}

// ====================================================================== replication

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

void AGSGameState::OnRep_AlarmPhase(EGSAlarmPhase OldPhase)
{
	OnAlarmPhaseChanged.Broadcast(AlarmPhase, OldPhase);
}

void AGSGameState::OnRep_RaidClockPhase()
{
	OnRaidClockPhaseChanged.Broadcast(RaidClockPhase);
}

void AGSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGSGameState, Alarm);
	DOREPLIFETIME(AGSGameState, AlarmPhase);
	DOREPLIFETIME(AGSGameState, ReinforcementTier);
	DOREPLIFETIME(AGSGameState, bDistrictRazed);
	DOREPLIFETIME(AGSGameState, RaidClockPhase);
	DOREPLIFETIME(AGSGameState, RaidSecondsRemaining);
	DOREPLIFETIME(AGSGameState, CollapseSecondsRemaining);
}
