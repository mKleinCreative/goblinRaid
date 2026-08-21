#include "Characters/GSStaminaComponent.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "ACFStatisticsSet.h"

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
		SetStamina(GetStamina() - DrainRate * DeltaTime);
		return;
	}

	SecondsSinceSpend += DeltaTime;

	// Suppressed = the pool is frozen, not merely not-draining. Hanging on a wall costs nothing and
	// gives nothing back. The delay timer keeps running so letting go does not restart it.
	if (bRegenSuppressed)
	{
		return;
	}

	if (RegenRate > 0.f && SecondsSinceSpend >= RegenDelaySeconds && GetStamina() < GetMaxStamina())
	{
		SetStamina(GetStamina() + RegenRate * DeltaTime);
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
	if (bExhausted || GetStamina() < Cost)
	{
		return false;
	}

	SecondsSinceSpend = 0.f;
	SetStamina(GetStamina() - Cost);
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
	SetStamina(GetMaxStamina());

	if (bWasExhausted)
	{
		OnRecovered.Broadcast();
	}
}

UAbilitySystemComponent* UGSStaminaComponent::GetOwnerASC() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

float UGSStaminaComponent::GetStamina() const
{
	if (const UAbilitySystemComponent* ASC = GetOwnerASC())
	{
		bool bFound = false;
		const float Value = ASC->GetGameplayAttributeValue(
			GetDefault<UACFStatisticsSet>()->StaminaAttribute(), bFound);
		if (bFound)
		{
			return Value;
		}
	}
	return Stamina;
}

float UGSStaminaComponent::GetMaxStamina() const
{
	if (const UAbilitySystemComponent* ASC = GetOwnerASC())
	{
		bool bFound = false;
		const float Value = ASC->GetGameplayAttributeValue(
			GetDefault<UACFStatisticsSet>()->MaxStaminaAttribute(), bFound);
		// A zero max would make every ratio and clamp meaningless, so treat it as "not configured"
		// and fall back rather than dividing the feel of the game by zero.
		if (bFound && Value > 0.f)
		{
			return Value;
		}
	}
	return MaxStamina;
}

float UGSStaminaComponent::GetStaminaNormalised() const
{
	const float Max = GetMaxStamina();
	return Max > 0.f ? GetStamina() / Max : 0.f;
}

void UGSStaminaComponent::SetStamina(float NewValue)
{
	const float Max = GetMaxStamina();
	const float Clamped = FMath::Clamp(NewValue, 0.f, Max);
	if (FMath::IsNearlyEqual(Clamped, GetStamina()))
	{
		return;
	}

	// ARS IS THE STORE (#231). The local float is written too so that an owner without an ability
	// system still works and so the replicated member stays meaningful, but ARS is what everything
	// reads back through GetStamina().
	Stamina = Clamped;
	if (UAbilitySystemComponent* ASC = GetOwnerASC())
	{
		const FGameplayAttribute StaminaAttr = GetDefault<UACFStatisticsSet>()->StaminaAttribute();
		bool bFound = false;
		ASC->GetGameplayAttributeValue(StaminaAttr, bFound);
		if (bFound)
		{
			ASC->SetNumericAttributeBase(StaminaAttr, Clamped);
		}
	}

	// Latch transitions AFTER the value is written, so a listener reading GetStamina() from inside
	// OnExhausted sees 0 rather than the value from before this frame.
	if (!bExhausted && GetStamina() <= 0.f)
	{
		bExhausted = true;
		DrainRate = 0.f;   // nothing left to drain; stops a caller holding the pool at 0 forever
		OnStaminaChanged.Broadcast(GetStamina(), GetMaxStamina());
		OnExhausted.Broadcast();
		return;
	}

	if (bExhausted && GetStamina() >= GetMaxStamina() * RecoverFraction)
	{
		bExhausted = false;
		OnStaminaChanged.Broadcast(GetStamina(), GetMaxStamina());
		OnRecovered.Broadcast();
		return;
	}

	OnStaminaChanged.Broadcast(GetStamina(), GetMaxStamina());
}

void UGSStaminaComponent::OnRep_Stamina()
{
	// Clients get the number, not the transitions: OnExhausted drives gameplay (letting go of a wall,
	// drowning) and that is a server decision. Firing it here too would double-handle it on a listen
	// server and give autonomous clients an authority they do not have.
	OnStaminaChanged.Broadcast(GetStamina(), GetMaxStamina());
}
