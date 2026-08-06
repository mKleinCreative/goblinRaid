// Raid-loop types shared by the director, the runic site, the game mode and the HUD.
// Written 2026-08-05 with UGSRaidDirector.
//
// Its own header rather than living on whichever class declared it first, mirroring
// Alarm/GSAlarmTypes.h: an enum that four systems branch on is a shared vocabulary, and putting it
// inside GSRaidDirector.h would force the HUD to include the whole subsystem to name a result.
#pragma once

#include "CoreMinimal.h"
#include "GSRaidTypes.generated.h"

/**
 * How a raid ended (design doc §2.2 - "win, lose, and the three pressures").
 *
 * ONE enum for every terminal state, deliberately. The score screen, the save-game write and the
 * post-raid progression all branch on the same fact, and the alternative - a bRaidOver flag beside
 * a bExtracted flag beside a bTimedOut flag - is how they end up disagreeing about whether a raid
 * that timed out with the portal already open counts as a win.
 *
 * NotEnded is the initial value rather than a separate "has it ended" bool for the same reason:
 * "has the raid ended" and "how did it end" cannot contradict each other if they are one field.
 */
UENUM(BlueprintType)
enum class EGSRaidResult : uint8
{
	/** The raid is still running. The only non-terminal value. */
	NotEnded = 0,

	/** Burned one of each objective type and stepped through the portal. The only win (GDD §2.2). */
	Extracted,

	/** The 30-minute clock expired and the 90s collapse grace ran out too (GDD §9). */
	LeftBehind,

	/** All lives spent (GDD §3 - lives, not permadeath). */
	OutOfLives
};

/**
 * One row of the HUD's objective list (GDD §2.1: the HUD NAMES the three objectives, and locating
 * them stays the player's job - names only, no arrows, no waypoints, decision 11).
 *
 * A flattened snapshot rather than a pointer to the carrier, so a Blueprint building a prettier
 * list cannot accidentally reach through it and mutate objective state from the HUD.
 */
USTRUCT(BlueprintType)
struct FGSObjectiveRow
{
	GENERATED_BODY()

	/** AGSBurnObjectiveBase::GetObjectiveDisplayName(). */
	UPROPERTY(BlueprintReadOnly, Category = "GoblinSiege|Objective")
	FText DisplayName;

	/** Required / Optional / Complete - drives the row's styling, not its presence. */
	UPROPERTY(BlueprintReadOnly, Category = "GoblinSiege|Objective")
	uint8 ListState = 0;

	/** 0..1. Meaningful mid-burn; a field at 0.4 is visibly on its way. */
	UPROPERTY(BlueprintReadOnly, Category = "GoblinSiege|Objective")
	float Completion01 = 0.f;
};
