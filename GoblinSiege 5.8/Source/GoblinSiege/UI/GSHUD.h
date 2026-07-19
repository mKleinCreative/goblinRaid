// One UMG widget, created here; every element binds to existing delegates (OnAlarmChanged,
// OnLivesChanged, OnOrbCountChanged, OnProgressChanged) rather than polling (tech doc §11).
// Reconstructed 2026-07-19.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GSHUD.generated.h"

class UUserWidget;

UCLASS()
class GOBLINSIEGE_API AGSHUD : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	/** WBP_GoblinSiegeHUD - assigned on the BP_GSHUD subclass. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> HUDWidget;
};
