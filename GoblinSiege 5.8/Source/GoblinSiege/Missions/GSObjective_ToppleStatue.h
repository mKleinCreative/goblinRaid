// The slice's statue objective: bring down the king's statue (GDD 2.8). Tag-search at
// BeginObjective - counts what it finds, so level designers add/remove statues without touching
// code. Reconstructed 2026-07-19; renamed from GSObjective_BurnGranaries 2026-08-18 when queue
// #156 removed the granary from the GDD and the statue took its place in the required trio.
//
// 2.8 on why this is not a burn objective: "The statue is the one target that doesn't burn: it
// has to be brought down, stone on stone." See the WARNING on AGSDestructibleObjective - the
// class this counts still gates its fracture behind burn-complete, which contradicts that rule
// and is tracked separately.
#pragma once

#include "CoreMinimal.h"
#include "Missions/GSMissionObjective.h"
#include "GSObjective_ToppleStatue.generated.h"

UCLASS()
class GOBLINSIEGE_API AGSObjective_ToppleStatue : public AGSMissionObjective
{
	GENERATED_BODY()

public:
	virtual void BeginObjective() override;

protected:
	UFUNCTION()
	void HandleStatueDestroyed();

	/** Matches the tag AGSDestructibleObjective adds in its constructor. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Missions")
	FName StatueActorTag = TEXT("Objective.Statue");

	int32 TotalStatues = 0;
	int32 ToppledStatues = 0;
};
