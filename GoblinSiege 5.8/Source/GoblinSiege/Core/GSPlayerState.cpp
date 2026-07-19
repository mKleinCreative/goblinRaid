#include "Core/GSPlayerState.h"
#include "Net/UnrealNetwork.h"

AGSPlayerState::AGSPlayerState()
{
	Lives = MaxLives;
}

int32 AGSPlayerState::LoseLife()
{
	if (!HasAuthority())
	{
		return Lives;
	}

	Lives = FMath::Max(0, Lives - 1);
	OnRep_Lives(); // server broadcasts too - listeners shouldn't care which side they're on
	return Lives;
}

void AGSPlayerState::ResetLivesForNewRaid()
{
	if (!HasAuthority())
	{
		return;
	}

	Lives = MaxLives;
	OnRep_Lives();
}

void AGSPlayerState::OnRep_Lives()
{
	OnLivesChanged.Broadcast(Lives);
}

void AGSPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGSPlayerState, Lives);
}
