// The whole main menu, code-side (#383/#385 - closing the raid loop needed somewhere for
// MainMenuButton/QuitButton to actually GO). Deliberately NOT AGSGameMode: that class is raid
// orchestration (lives, respawn, the runic site) with nothing in it a menu level has any use for,
// and giving the menu a GameMode that happens to inherit all of that is how a menu level ends up
// quietly depending on raid-only state existing.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GSMainMenuGameMode.generated.h"

class UUserWidget;

UCLASS()
class GOBLINSIEGE_API AGSMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGSMainMenuGameMode();

protected:
	virtual void BeginPlay() override;

	/** WBP_MainMenu, or whatever a designer swaps in - set on the class default, not hardcoded,
	 *  so this GameMode is reusable if a second menu-shaped level ever exists. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|MainMenu")
	TSoftClassPtr<UUserWidget> MenuWidgetClass;

private:
	UPROPERTY()
	TObjectPtr<UUserWidget> MenuWidget;
};
