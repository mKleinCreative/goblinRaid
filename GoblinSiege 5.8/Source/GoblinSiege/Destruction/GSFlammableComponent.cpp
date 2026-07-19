#include "Destruction/GSFlammableComponent.h"
#include "TimerManager.h"

UGSFlammableComponent::UGSFlammableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGSFlammableComponent::Ignite()
{
	// FireResistance >= 1 means "doesn't burn" (Dwarf Bunker / stone Barracks path).
	if (bIsBurning || FireResistance >= 1.f)
	{
		return;
	}

	bIsBurning = true;
	OnIgnited.Broadcast();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(BurnTimerHandle, this,
			&UGSFlammableComponent::BurnTick, BurnTickInterval, true);
	}
	// Fire FX attachment is a Blueprint concern bound to OnIgnited - keeps this component
	// presentation-free.
}

void UGSFlammableComponent::Extinguish()
{
	if (!bIsBurning)
	{
		return;
	}

	bIsBurning = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BurnTimerHandle);
	}
	// Progress deliberately NOT reset: a re-ignited half-burned granary finishes faster -
	// rewards re-torching what the defenders saved.
	OnExtinguished.Broadcast();
}

void UGSFlammableComponent::BurnTick()
{
	BurnedSeconds += BurnTickInterval * (1.f - FireResistance);

	if (BurnedSeconds >= BurnDurationSeconds)
	{
		bIsBurning = false;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BurnTimerHandle);
		}
		OnBurnedDown.Broadcast();
	}
}

void UGSFlammableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BurnTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}
