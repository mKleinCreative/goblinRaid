// Owns save/load. The progression economy is parked for the slice (score only, GDD decision),
// but the save plumbing keeps personal bests across sessions. Reconstructed 2026-07-19.
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GSGameInstance.generated.h"

class UGSSaveGame;

UCLASS()
class GOBLINSIEGE_API UGSGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Save")
	void SaveGame();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Save")
	UGSSaveGame* GetSaveGame() const { return CurrentSave; }

protected:
	UPROPERTY()
	TObjectPtr<UGSSaveGame> CurrentSave;

	static const FString SaveSlotName;
};
