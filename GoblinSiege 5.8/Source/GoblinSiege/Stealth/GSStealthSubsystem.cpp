// Implements the confirm funnel described in design doc §2.4 / §2.6: one confirm, one soft
// signal, town to Suspicious - and the First Spark Unseen ledger.

#include "Stealth/GSStealthSubsystem.h"

#include "Core/GSGameState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UGSStealthSubsystem::Deinitialize()
{
	RegisteredTargets.Reset();
	StanceOverrides.Reset();
	LastConfirmTimes.Reset();
	ConfirmedSightingCount = 0;
	bFirstSparkHappened = false;
	bFirstSparkUnseen = false;

	Super::Deinitialize();
}

UGSStealthSubsystem* UGSStealthSubsystem::Get(const UObject* WorldContextObject)
{
	if (!GEngine || !WorldContextObject)
	{
		return nullptr;
	}

	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
	{
		return World->GetSubsystem<UGSStealthSubsystem>();
	}

	return nullptr;
}

void UGSStealthSubsystem::RegisterStealthTarget(AActor* Target)
{
	if (IsValid(Target))
	{
		RegisteredTargets.Add(Target);
	}
}

void UGSStealthSubsystem::UnregisterStealthTarget(AActor* Target)
{
	if (Target)
	{
		RegisteredTargets.Remove(Target);
		StanceOverrides.Remove(Target);
	}
}

void UGSStealthSubsystem::GatherStealthTargets(TArray<AActor*>& OutTargets) const
{
	OutTargets.Reset();

	for (const TWeakObjectPtr<AActor>& WeakTarget : RegisteredTargets)
	{
		if (AActor* Target = WeakTarget.Get())
		{
			if (IsValid(Target))
			{
				OutTargets.AddUnique(Target);
			}
		}
	}

	// Player pawns are always fair game - no one has to remember to register the goblins.
	if (const UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (const APlayerController* PC = It->Get())
			{
				if (APawn* Pawn = PC->GetPawn())
				{
					if (IsValid(Pawn))
					{
						OutTargets.AddUnique(Pawn);
					}
				}
			}
		}
	}
}

EGSStealthStance UGSStealthSubsystem::GetStanceFor(const AActor* Target) const
{
	if (!IsValid(Target))
	{
		return EGSStealthStance::Hidden;
	}

	if (const EGSStealthStance* Override = StanceOverrides.Find(Target))
	{
		return *Override;
	}

	if (const ACharacter* Character = Cast<ACharacter>(Target))
	{
		if (Character->bIsCrouched)
		{
			return EGSStealthStance::Crouched;
		}

		if (const UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			if (Movement->IsCrouching())
			{
				return EGSStealthStance::Crouched;
			}
		}
	}

	return EGSStealthStance::Standing;
}

void UGSStealthSubsystem::SetStanceOverride(AActor* Target, EGSStealthStance Stance)
{
	if (IsValid(Target))
	{
		StanceOverrides.Add(Target, Stance);
	}
}

void UGSStealthSubsystem::ClearStanceOverride(AActor* Target)
{
	if (Target)
	{
		StanceOverrides.Remove(Target);
	}
}

bool UGSStealthSubsystem::ReportSightingConfirmed(AActor* Observer, AActor* Target)
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(Observer) || !IsValid(Target))
	{
		return false;
	}

	// Signals are authored on the server only; clients never move the town's state of mind.
	if (World->GetNetMode() == NM_Client)
	{
		return false;
	}

	const double Now = World->GetTimeSeconds();

	FGSConfirmKey Key;
	Key.Observer = Observer;
	Key.Target = Target;

	if (const double* LastTime = LastConfirmTimes.Find(Key))
	{
		if (Now - *LastTime < static_cast<double>(FMath::Max(0.f, ConfirmDedupeSeconds)))
		{
			return false;
		}
	}

	LastConfirmTimes.Add(Key, Now);
	++ConfirmedSightingCount;

	// Exactly one uncorroborated soft signal: Quiet -> Suspicious, and it takes a second signal
	// to escalate past that (design doc §2.4, §2.6).
	if (AGSGameState* GameState = World->GetGameState<AGSGameState>())
	{
		GameState->ReportConfirmedSighting();
	}

	OnStealthConfirmSignal.Broadcast(Observer, Target, ConfirmedSightingCount);
	return true;
}

void UGSStealthSubsystem::NotifyFirstObjectiveIgnited()
{
	if (bFirstSparkHappened)
	{
		return;
	}

	bFirstSparkHappened = true;
	bFirstSparkUnseen = (ConfirmedSightingCount == 0);

	OnFirstSparkResolved.Broadcast(bFirstSparkUnseen);
}
