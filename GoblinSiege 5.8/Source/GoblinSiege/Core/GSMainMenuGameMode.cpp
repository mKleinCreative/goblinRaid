#include "Core/GSMainMenuGameMode.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSMainMenu, Log, All);

AGSMainMenuGameMode::AGSMainMenuGameMode()
{
	// A menu has no pawn to possess and no HUD class to spawn - both are the raid GameMode's job.
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
}

void AGSMainMenuGameMode::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}

	UClass* WidgetClass = MenuWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		UE_LOG(LogGSMainMenu, Error,
			TEXT("[GoblinSiege] AGSMainMenuGameMode has no MenuWidgetClass set - the menu level has ")
			TEXT("nothing to show. Set it on the GameMode's class defaults (WBP_MainMenu)."));
		return;
	}

	MenuWidget = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (!MenuWidget)
	{
		return;
	}

	MenuWidget->AddToViewport();

	// Same reasoning as UGSPlayerHUDWidget::HandleRaidEnded: nothing in this project has ever
	// needed a mouse cursor before a menu existed to click, so nothing upstream shows one.
	//
	// NOT SetWidgetToFocus(MenuWidget->TakeWidget()) - a plain UUserWidget's root SObjectWidget is
	// not focusable by default (logged "Attempting to focus Non-Focusable widget" and silently
	// failed to focus anything). Mouse clicks on the buttons underneath do not need keyboard focus
	// to register; leaving this unset just means nothing has focus for gamepad/keyboard navigation
	// yet, which this menu does not use.
	PC->SetShowMouseCursor(true);
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
}
