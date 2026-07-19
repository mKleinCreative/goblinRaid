#include "Core/GSGameInstance.h"
#include "Progression/GSSaveGame.h"
#include "Kismet/GameplayStatics.h"

const FString UGSGameInstance::SaveSlotName = TEXT("GoblinSiegeSave");

void UGSGameInstance::Init()
{
	Super::Init();

	if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
	{
		CurrentSave = Cast<UGSSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
	}

	if (!CurrentSave)
	{
		CurrentSave = Cast<UGSSaveGame>(UGameplayStatics::CreateSaveGameObject(UGSSaveGame::StaticClass()));
	}
}

void UGSGameInstance::SaveGame()
{
	if (CurrentSave)
	{
		UGameplayStatics::SaveGameToSlot(CurrentSave, SaveSlotName, 0);
	}
}
