// Raid-loop types shared by the director, the runic site, the game mode and the HUD.
// Written 2026-08-05 with UGSRaidDirector.
//
// Its own header rather than living on whichever class declared it first, mirroring
// Alarm/GSAlarmTypes.h: an enum that four systems branch on is a shared vocabulary, and putting it
// inside GSRaidDirector.h would force the HUD to include the whole subsystem to name a result.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
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
/** Which of the three state markers a row draws. Mirrors the [ ] / [~] / [x] the text list uses,
 *  because they say the same thing and must not be able to disagree. */
UENUM(BlueprintType)
enum class EGSObjectiveRowIcon : uint8
{
	Untouched,   // [ ] nothing has happened to it
	InProgress,  // [~] burning, or partly done
	Done         // [x] complete
};

/**
 * One row EXACTLY as it should be displayed - already collapsed, already counted, already
 * pluralised.
 *
 * FGSObjectiveRow below is the raw per-carrier row. This is what survives the grouping pass: a
 * type with more carriers than CollapseTypeAbove becomes ONE of these reading "Houses 12 / 27",
 * while small types keep a row each.
 *
 * It exists so the text list and the icon list are the same list. The collapse rule, the required
 * denominator and the clamp are subtle and were each written to fix a specific misreading; a
 * second consumer re-deriving them would eventually disagree with the first, and the player would
 * be told two different things about the same objective.
 */
USTRUCT(BlueprintType)
struct FGSObjectiveDisplayRow
{
	GENERATED_BODY()

	/** "Houses", "The Wheat Field". Never empty - an unnamed objective still gets a placeholder. */
	UPROPERTY(BlueprintReadOnly, Category = "GoblinSiege|Objective")
	FText Label;

	/** The trailing qualifier: "12 / 27", "40%", "(bonus)", or empty. */
	UPROPERTY(BlueprintReadOnly, Category = "GoblinSiege|Objective")
	FText Detail;

	UPROPERTY(BlueprintReadOnly, Category = "GoblinSiege|Objective")
	EGSObjectiveRowIcon Icon = EGSObjectiveRowIcon::Untouched;

	/** Picks the type picture - house, field, market, mill, statue. */
	UPROPERTY(BlueprintReadOnly, Category = "GoblinSiege|Objective")
	FGameplayTag TypeTag;
};

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

	/**
	 * Which burn type this carrier is (Objective.Burn.Field / .Mill / .Market / .House).
	 *
	 * Added 2026-08-05 so the HUD can COLLAPSE. Eleven houses landed in the level and the objective
	 * list printed eleven identical "A House" rows on top of field, mill and market - a list that
	 * long stops being read at all, which defeats the one job it has. The list needs to group, and
	 * grouping needs the type on the row; the display name alone cannot tell two houses apart from
	 * a house and a mill.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "GoblinSiege|Objective")
	FGameplayTag TypeTag;
};
