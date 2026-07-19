// PARKED for the slice (GDD decision: score only). Stays in the scaffold untouched as the
// post-slice economy's landing pad - costs nothing by existing (tech doc §10). Reconstructed
// 2026-07-19.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GSWeaponProgressionTree.generated.h"

USTRUCT(BlueprintType)
struct FGSWeaponUnlockNode
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag WeaponTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 UnlockCost = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FGameplayTag> PrerequisiteWeapons;
};

UCLASS(BlueprintType)
class GOBLINSIEGE_API UGSWeaponProgressionTree : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Progression")
	TArray<FGSWeaponUnlockNode> Nodes;
};
