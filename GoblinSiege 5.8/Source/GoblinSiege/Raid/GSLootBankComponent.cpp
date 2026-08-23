#include "Raid/GSLootBankComponent.h"

#include "Raid/GSScoreSubsystem.h"
#include "Interaction/GSCarryComponent.h"
#include "Interaction/GSInteractableComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSLootBank, Log, All);

UGSLootBankComponent::UGSLootBankComponent()
{
	// Purely reactive - the owner calls in when something arrives. Nothing to poll.
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UGSLootBankComponent::GetLootValueOf(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return 0;
	}
	const UGSInteractableComponent* Interactable = Actor->FindComponentByClass<UGSInteractableComponent>();
	return Interactable ? Interactable->GetLootValue() : 0;
}

int32 UGSLootBankComponent::BankFromOverlap(AActor* Arriving)
{
	if (!IsValid(Arriving))
	{
		return 0;
	}

	// Pawn first. See the header: a courier is both a pawn and a loose actor, and the loose path
	// would happily swallow the goblin.
	if (APawn* Pawn = Cast<APawn>(Arriving))
	{
		return BankCarriedLoot(Pawn);
	}
	return BankLooseActor(Arriving);
}

int32 UGSLootBankComponent::BankCarriedLoot(APawn* Pawn)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !IsValid(Pawn))
	{
		return 0;
	}

	UGSCarryComponent* Carry = Pawn->FindComponentByClass<UGSCarryComponent>();
	if (!Carry || !Carry->IsCarrying())
	{
		return 0;
	}

	AActor* Cargo = Carry->GetCarriedActor();
	const int32 Value = GetLootValueOf(Cargo);
	if (Value <= 0)
	{
		// Carrying something worthless - a body, a prop. Logged rather than silently ignored,
		// because "the portal did not take my pig" and "the pig is worth nothing" are different
		// bugs that look identical from the player's side of the screen.
		UE_LOG(LogGSLootBank, Verbose,
			TEXT("[GoblinSiege] '%s' reached the %s carrying '%s', which has no LootValue."),
			*Pawn->GetName(), *BankLabel, *GetNameSafe(Cargo));
		return 0;
	}

	// PutDown, never a bare Destroy: going straight to Destroy leaves the carrier's attach, its
	// move-speed effect and its replicated CarriedActor all pointing at a dead actor. PutDown is the
	// one path that unwinds all of it. CommitBank does the destroying afterwards.
	Carry->PutDown();

	return CommitBank(Cargo, Value);
}

int32 UGSLootBankComponent::BankLooseActor(AActor* Actor)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !IsValid(Actor) || Actor == Owner)
	{
		return 0;
	}

	// A pawn is never loose cargo.
	if (Actor->IsA<APawn>())
	{
		return 0;
	}

	const int32 Value = GetLootValueOf(Actor);
	if (Value <= 0)
	{
		return 0;
	}

	// Something a goblin is holding is not lying in the circle, whatever the overlap says. Without
	// this, a courier walking its cargo in would have the cargo banked out from under it by this
	// path a frame before its own PutDown, leaving the carrier's state dangling.
	if (Actor->GetAttachParentActor() != nullptr)
	{
		return 0;
	}

	return CommitBank(Actor, Value);
}

int32 UGSLootBankComponent::CommitBank(AActor* Cargo, int32 Value)
{
	const FString CargoName = GetNameSafe(Cargo);
	AActor* Owner = GetOwner();

	// LOOT, NEVER DEEDS - see the header. There is no flag for this and there must never be one.
	if (UWorld* World = GetWorld())
	{
		if (UGSScoreSubsystem* Score = World->GetSubsystem<UGSScoreSubsystem>())
		{
			Score->AddLoot(Value);
		}
		else
		{
			UE_LOG(LogGSLootBank, Error,
				TEXT("[GoblinSiege] %s banked '%s' worth %d but found no UGSScoreSubsystem - "
					 "the points are gone."), *GetNameSafe(Owner), *CargoName, Value);
		}
	}

	PointsBankedHere += Value;
	++ItemsBankedHere;

	UE_LOG(LogGSLootBank, Log,
		TEXT("[GoblinSiege] The %s on '%s' swallowed '%s' for %d loot. %d banked here across %d item(s)."),
		*BankLabel, *GetNameSafe(Owner), *CargoName, Value, PointsBankedHere, ItemsBankedHere);

	// BROADCAST BEFORE DESTROYING. Every listener this hook exists for - a burst at the mouth, a
	// gulp, an Overlord bark naming what went in - wants the cargo's location or class.
	OnLootBanked.Broadcast(Cargo, Value);

	if (IsValid(Cargo))
	{
		Cargo->Destroy();
	}

	return Value;
}
