// Mission objective base (design doc §9). Concrete objectives override BeginObjective(); the
// GameMode (or level Blueprint) begins them at raid start, and completion opens the runic-site
// portal (Part II §22 supersession - the listener changed, the delegate wiring didn't).
// Reconstructed 2026-07-19 to match the surviving GSObjective_KillLandlord.h.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSMissionObjective.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnObjectiveCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnObjectiveProgressChanged, float, Progress01);

UCLASS(Abstract)
class GOBLINSIEGE_API AGSMissionObjective : public AActor
{
	GENERATED_BODY()

public:
	/** Called at raid start. Concrete objectives find their targets (tag search) and bind here. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Missions")
	virtual void BeginObjective();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Missions")
	bool IsCompleted() const { return bCompleted; }

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Missions")
	FGSOnObjectiveCompleted OnObjectiveCompleted;

	/** HUD binds this (tech doc §11 - widgets bind delegates, never poll). */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Missions")
	FGSOnObjectiveProgressChanged OnProgressChanged;

protected:
	void ReportProgress(float Progress01);
	void CompleteObjective();

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Missions")
	FText ObjectiveName;

	bool bCompleted = false;
};
