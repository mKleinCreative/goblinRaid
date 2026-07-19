// Alarm/heat types shared by GameState, spawners, missions, and AI tasks (design doc §4).
// Reconstructed 2026-07-19; entry names are load-bearing - GSGameState.cpp, GSSpawnerActor.cpp,
// and BTTask_Firefight.cpp reference them by name.
#pragma once

#include "CoreMinimal.h"
#include "GSAlarmTypes.generated.h"

/** Where an alarm delta came from - lets tuning/telemetry distinguish "the players are loud"
 *  from "the city is on fire", and lets firefighting pay the meter back down. */
UENUM(BlueprintType)
enum class EGSAlarmSource : uint8
{
	PassiveTick,
	CombatNoise,
	FireDamage,
	BarracksDestroyed,
	FireExtinguishedByDefenders,
	ObjectiveProgress
};

/** Escalation tiers crossed as the alarm meter maxes out and resets (design doc §4 horde waves).
 *  Spawners spawn faster at higher tiers; the AI director picks nastier wave compositions. */
UENUM(BlueprintType)
enum class EGSReinforcementTier : uint8
{
	Tier0_Baseline = 0,
	Tier1_Reinforced,
	Tier2_Elite,
	Tier3_LastStand
};
