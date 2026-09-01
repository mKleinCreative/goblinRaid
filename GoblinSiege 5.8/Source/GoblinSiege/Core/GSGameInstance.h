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

	/** WorldContext convenience for UI (e.g. the main menu's gold/XP display), which has no
	 *  strongly-typed GSGameInstance pointer to hand a Cast node - saves the widget graph a
	 *  GetGameInstance + Cast pair. Returns null if the world has no GSGameInstance (should not
	 *  happen once GameInstanceClass is set in DefaultEngine.ini, but UI should not crash if it is). */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Save", meta = (WorldContext = "WorldContextObject"))
	static UGSSaveGame* GetCurrentSave(const UObject* WorldContextObject);

protected:
	UPROPERTY()
	TObjectPtr<UGSSaveGame> CurrentSave;

	static const FString SaveSlotName;
};
