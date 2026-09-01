#include "Core/GSGameInstance.h"
#include "Progression/GSSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

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

UGSSaveGame* UGSGameInstance::GetCurrentSave(const UObject* WorldContextObject)
{
	if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr)
	{
		if (const UGSGameInstance* GameInstance = World->GetGameInstance<UGSGameInstance>())
		{
			return GameInstance->GetSaveGame();
		}
	}
	return nullptr;
}

void UGSGameInstance::SaveGame()
{
	if (CurrentSave)
	{
		UGameplayStatics::SaveGameToSlot(CurrentSave, SaveSlotName, 0);
	}
}
