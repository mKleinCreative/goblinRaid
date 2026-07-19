// The slice's objective: burn 3 granaries (design doc §9). Tag-search at BeginObjective - counts
// what it finds, so level designers add/remove granaries without touching code. Reconstructed
// 2026-07-19.
#pragma once

#include "CoreMinimal.h"
#include "Missions/GSMissionObjective.h"
#include "GSObjective_BurnGranaries.generated.h"

UCLASS()
class GOBLINSIEGE_API AGSObjective_BurnGranaries : public AGSMissionObjective
{
	GENERATED_BODY()

public:
	virtual void BeginObjective() override;

protected:
	UFUNCTION()
	void HandleGranaryDestroyed();

	/** Matches the tag AGSDestructibleObjective adds in its constructor. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Missions")
	FName GranaryActorTag = TEXT("Objective.Granary");

	int32 TotalGranaries = 0;
	int32 BurnedGranaries = 0;
};
