// Save plumbing (owned by UGSGameInstance). The progression economy is parked - score is the
// currency-shaped thing for the slice - but personal bests persist (tech doc §10, Part II §23).
// Reconstructed 2026-07-19.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "GSSaveGame.generated.h"

UCLASS()
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
};
