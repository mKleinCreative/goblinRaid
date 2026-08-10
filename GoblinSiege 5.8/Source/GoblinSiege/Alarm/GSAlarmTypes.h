// Alarm/heat types shared by GameState, spawners, missions, and AI tasks (design doc §4, §8.1).
// Reconstructed 2026-07-19; entry names are load-bearing - GSGameState.cpp, GSSpawnerActor.cpp,
// and BTTask_Firefight.cpp reference them by name.
// 2026-07-28 (Block C): added EGSAlarmPhase - the four-phase model from design doc §8.1 that
// drives guard BT branches, barracks spawning, the patrol director, HUD, and ambient audio.
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
	ObjectiveProgress,

	/** The player blew the war-horn (GDD §2.5). APPENDED, never inserted: GSGameState.cpp,
	 *  GSSpawnerActor.cpp and BTTask_Firefight.cpp reference these entries by name, and a
	 *  BlueprintType enum's numeric values are serialised into any asset that stores one -
	 *  reordering would silently reassign every saved value.
	 *
	 *  Distinct from CombatNoise on purpose: the horn is not a scuffle overheard, it is the
	 *  deliberate end of the quiet half, and it is the one alarm source the player chooses. */
	HornBlast
};

/**
 * The town's state of mind (design doc §8.1). Distinct from the alarm *meter*: the meter is a
 * float that escalates reinforcement tiers, the phase is a coarse mode that whole systems branch
 * on. Ordered and monotonic past Raid - "Alarm reducers: none. Goblins don't de-escalate, they
 * leave." Only Quiet <-> Suspicious is reversible.
 */
UENUM(BlueprintType)
enum class EGSAlarmPhase : uint8
{
	/** Daily routine. Alarm ticks only from witnessed events. */
	Quiet = 0,
	/** A half-confirmed sighting, a noise, a corpse, a patrol gone missing. Decays back to Quiet
	 *  if nothing is confirmed. */
	Suspicious,
	/** Bell rung, open combat, or fire seen by a human. Barracks spawn, castle reinforcements
	 *  start marching. Never decays. */
	Raid,
	/** The settlement is substantially burned: internal spawners go quiet, relief columns
	 *  intensify. The "get out now" phase. */
	Razed
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

/**
 * Where the raid clock is (design doc §9 "The clock"). The portal's own open/closed state lives
 * on the runic site; this is purely the 30-minute timer's phase so HUD, whispers and the
 * left-behind rule all read one enum.
 */
UENUM(BlueprintType)
enum class EGSRaidClockPhase : uint8
{
	/** Clock hasn't started (pre-raid / menu). */
	NotStarted = 0,
	/** Normal countdown. */
	Running,
	/** Under FinalWarningSeconds remaining - the Overlord's whispers turn impatient. */
	FinalWarning,
	/** 0:00 reached. The portal is collapsing; CollapseGraceSeconds to get through. */
	Collapsing,
	/** Grace window expired. Anyone still outside is left behind. */
	Expired
};
