// Save plumbing (owned by UGSGameInstance). The progression economy is parked - score is the
// currency-shaped thing for the slice - but personal bests persist (tech doc §10, Part II §23).
// Reconstructed 2026-07-19.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "GSSaveGame.generated.h"

UCLASS(BlueprintType)
class GOBLINSIEGE_API UGSSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Personal bests keyed by map/mission name (Part II §23 score screen). */
	UPROPERTY()
	TMap<FName, int32> BestScores;

	/** Post-slice economy landing pad - unused during the slice. */
	UPROPERTY()
	TArray<FGameplayTag> UnlockedWeaponTags;

	/** Running total gold across every raid ever played, ADDED to each raid's
	 *  UGSScoreSubsystem::GetLoot() at EndRaid - distinct from BestScores, which is a per-map high
	 *  score (Max()), not a wallet (+=). What it's spent on is a later problem (2026-08-30). */
	UPROPERTY(BlueprintReadOnly, Category = "GoblinSiege|Save")
	int32 Gold = 0;

	/** Running total experience across every raid ever played, ADDED to each raid's
	 *  UGSScoreSubsystem::GetDeeds() at EndRaid. */
	UPROPERTY(BlueprintReadOnly, Category = "GoblinSiege|Save")
	int32 Experience = 0;
};
