#include "Characters/GSStaminaComponent.h"
#include "Net/UnrealNetwork.h"

UGSStaminaComponent::UGSStaminaComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UGSStaminaComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UGSStaminaComponent, Stamina);
	DOREPLIFETIME(UGSStaminaComponent, bExhausted);
}

void UGSStaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Server owns the pool. Clients read the replicated value; predicting it locally would look
	// smoother and then snap on every correction, which is worse than a small delay on a bar.
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	if (DrainRate > 0.f)
	{
		SecondsSinceSpend = 0.f;
		SetStamina(Stamina - DrainRate * DeltaTime);
		return;
	}

	SecondsSinceSpend += DeltaTime;

	// Suppressed = the pool is frozen, not merely not-draining. Hanging on a wall costs nothing and
	// gives nothing back. The delay timer keeps running so letting go does not restart it.
	if (bRegenSuppressed)
	{
		return;
	}

	if (RegenRate > 0.f && SecondsSinceSpend >= RegenDelaySeconds && Stamina < MaxStamina)
	{
		SetStamina(Stamina + RegenRate * DeltaTime);
	}
}

bool UGSStaminaComponent::TryConsume(float Cost)
{
	if (Cost <= 0.f)
	{
		return true;
	}

	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	// Exhausted refuses everything. Otherwise the latch would only govern sustained drains and a
	// player at zero could still vault, which reads as the meter not mattering.
	if (bExhausted || Stamina < Cost)
	{
		return false;
	}

	SecondsSinceSpend = 0.f;
	SetStamina(Stamina - Cost);
	return true;
}

void UGSStaminaComponent::SetDrainRate(float PerSecond)
{
	DrainRate = FMath::Max(0.f, PerSecond);
}

void UGSStaminaComponent::SetRegenSuppressed(bool bSuppressed)
{
	bRegenSuppressed = bSuppressed;
}

void UGSStaminaComponent::ResetToFull()
{
	DrainRate = 0.f;
	SecondsSinceSpend = 0.f;
	// A respawn must clear this. Dying mid-climb would otherwise leave the pool frozen for the rest
	// of the raid, with nothing on screen to explain why stamina stopped coming back.
	bRegenSuppressed = false;

	const bool bWasExhausted = bExhausted;
	bExhausted = false;
	SetStamina(MaxStamina);

	if (bWasExhausted)
	{
		OnRecovered.Broadcast();
	}
}

void UGSStaminaComponent::SetStamina(float NewValue)
{
	const float Clamped = FMath::Clamp(NewValue, 0.f, MaxStamina);
	if (FMath::IsNearlyEqual(Clamped, Stamina))
	{
		return;
	}
	Stamina = Clamped;

	// Latch transitions AFTER the value is written, so a listener reading GetStamina() from inside
	// OnExhausted sees 0 rather than the value from before this frame.
	if (!bExhausted && Stamina <= 0.f)
	{
		bExhausted = true;
		DrainRate = 0.f;   // nothing left to drain; stops a caller holding the pool at 0 forever
		OnStaminaChanged.Broadcast(Stamina, MaxStamina);
		OnExhausted.Broadcast();
		return;
	}

	if (bExhausted && Stamina >= MaxStamina * RecoverFraction)
	{
		bExhausted = false;
		OnStaminaChanged.Broadcast(Stamina, MaxStamina);
		OnRecovered.Broadcast();
		return;
	}

	OnStaminaChanged.Broadcast(Stamina, MaxStamina);
}

void UGSStaminaComponent::OnRep_Stamina()
{
	// Clients get the number, not the transitions: OnExhausted drives gameplay (letting go of a wall,
	// drowning) and that is a server decision. Firing it here too would double-handle it on a listen
	// server and give autonomous clients an authority they do not have.
	OnStaminaChanged.Broadcast(Stamina, MaxStamina);
}
