// Implements design doc §2.4's one readable rule: hold the sighting ~1.5 seconds to confirm,
// break line of sight and the timer is zero again - not a partial signal, no signal at all.

#include "Stealth/GSSightPerceptionComponent.h"

#include "Alarm/GSAlarmTypes.h"
#include "Core/GSGameState.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Stealth/GSStealthSubsystem.h"

UGSSightPerceptionComponent::UGSSightPerceptionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// 20Hz is plenty for a 1.5s hold and keeps the line traces off the frame budget.
	PrimaryComponentTick.TickInterval = 0.05f;

	SetIsReplicatedByDefault(false);
}

void UGSSightPerceptionComponent::BeginPlay()
{
	Super::BeginPlay();

	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		// Detection is a server-side truth. Clients keep the component for its accessors only.
		SetComponentTickEnabled(false);
	}
}

void UGSSightPerceptionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Observer = GetOwner();
	if (!IsValid(Observer) || !Observer->HasAuthority())
	{
		return;
	}

	TArray<AActor*> Targets;
	if (UGSStealthSubsystem* Stealth = UGSStealthSubsystem::Get(this))
	{
		Stealth->GatherStealthTargets(Targets);
	}

	TSet<AActor*> Processed;
	Processed.Reserve(Targets.Num());

	bool bSawSomeone = false;

	for (AActor* Target : Targets)
	{
		if (!IsValid(Target) || Target == Observer)
		{
			continue;
		}

		Processed.Add(Target);

		float Exposure01 = 0.f;
		const bool bVisible = EvaluateVisibility(Target, Exposure01);

		FGSSightingRecord& Record = Sightings.FindOrAdd(Target);
		Record.bVisibleNow = bVisible;
		Record.LastExposure01 = Exposure01;

		if (bVisible)
		{
			bSawSomeone = true;
			Record.TimeSinceLastSeen = 0.f;
			Record.LastKnownLocation = Target->GetActorLocation();

			if (!Record.bConfirmed)
			{
				Record.ConfirmTimer += DeltaTime;
				if (Record.ConfirmTimer >= Tuning.ConfirmHoldSeconds)
				{
					ConfirmSighting(Target, Record);
				}
			}
		}
		else
		{
			// Duck behind the haycart before the confirm lands and you were never there.
			Record.ConfirmTimer = 0.f;
			Record.TimeSinceLastSeen += DeltaTime;

			if (Record.TimeSinceLastSeen >= Tuning.ForgetSeconds)
			{
				Sightings.Remove(Target);
			}
		}
	}

	// Age out anything the registry stopped reporting (dead, streamed out, unregistered).
	for (TMap<TWeakObjectPtr<AActor>, FGSSightingRecord>::TIterator It(Sightings); It; ++It)
	{
		AActor* Key = It.Key().Get();
		if (!IsValid(Key))
		{
			It.RemoveCurrent();
			continue;
		}

		if (Processed.Contains(Key))
		{
			continue;
		}

		FGSSightingRecord& Record = It.Value();
		Record.bVisibleNow = false;
		Record.ConfirmTimer = 0.f;
		Record.TimeSinceLastSeen += DeltaTime;
		if (Record.TimeSinceLastSeen >= Tuning.ForgetSeconds)
		{
			It.RemoveCurrent();
		}
	}

	UpdateDoze(DeltaTime, bSawSomeone);
	DrawDebugVision(DeltaTime);
}

bool UGSSightPerceptionComponent::EvaluateVisibility(AActor* Target, float& OutExposure01) const
{
	OutExposure01 = 0.f;

	const AActor* Observer = GetOwner();
	UWorld* World = GetWorld();
	if (!IsValid(Target) || !IsValid(Observer) || !World)
	{
		return false;
	}

	EGSStealthStance Stance = EGSStealthStance::Standing;
	if (UGSStealthSubsystem* Stealth = UGSStealthSubsystem::Get(this))
	{
		Stance = Stealth->GetStanceFor(Target);
	}

	if (Stance == EGSStealthStance::Hidden)
	{
		return false;
	}

	const FVector EyeLocation = GetEyeLocation();
	const FVector TargetCenter = Target->GetActorLocation();
	const float Distance = FVector::Dist(EyeLocation, TargetCenter);

	if (Distance <= 1.f)
	{
		OutExposure01 = 1.f;
		return true;
	}

	// Cheap reject before any traces: nothing can ever be seen past the base range.
	if (Distance > Tuning.BaseSightRange)
	{
		return false;
	}

	const FVector ToTarget = (TargetCenter - EyeLocation).GetSafeNormal();
	const FVector ViewDirection = GetVisionDirection();
	const float CosLimit = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(GetEffectiveHalfAngleDegrees(), 1.f, 179.f)));
	if (FVector::DotProduct(ViewDirection, ToTarget) < CosLimit)
	{
		return false;
	}

	// Cover geometry: sample the silhouette rather than one hero ray, so a hedgerow that clips
	// the legs shortens the guard's range instead of doing nothing at all.
	const float HalfHeight = FMath::Max(20.f, Target->GetSimpleCollisionHalfHeight());
	const float Radius = FMath::Max(10.f, Target->GetSimpleCollisionRadius() * 0.8f);
	const FVector RightOffset = FVector::CrossProduct(FVector::UpVector, ToTarget).GetSafeNormal() * Radius;

	TArray<FVector, TInlineAllocator<5>> Samples;
	Samples.Add(TargetCenter + FVector(0.f, 0.f, HalfHeight * 0.85f));
	Samples.Add(TargetCenter);
	Samples.Add(TargetCenter - FVector(0.f, 0.f, HalfHeight * 0.6f));
	Samples.Add(TargetCenter + RightOffset);
	Samples.Add(TargetCenter - RightOffset);

	FCollisionQueryParams Params(TEXT("GSSightConfirm"), /*bTraceComplex=*/false, Observer);
	Params.AddIgnoredActor(Target);

	int32 VisibleSamples = 0;
	for (const FVector& Sample : Samples)
	{
		if (!World->LineTraceTestByChannel(EyeLocation, Sample, ECC_Visibility, Params))
		{
			++VisibleSamples;
		}
	}

	OutExposure01 = Samples.Num() > 0 ? static_cast<float>(VisibleSamples) / static_cast<float>(Samples.Num()) : 0.f;
	if (OutExposure01 <= Tuning.MinimumExposureToSee)
	{
		OutExposure01 = 0.f;
		return false;
	}

	const float EffectiveRange = Tuning.GetEffectiveRange(Stance, OutExposure01, GetDozeRangeMultiplier());
	return Distance <= EffectiveRange;
}

void UGSSightPerceptionComponent::ConfirmSighting(AActor* Target, FGSSightingRecord& Record)
{
	Record.bConfirmed = true;
	Record.ConfirmTimer = Tuning.ConfirmHoldSeconds;
	DozeSeconds = 0.f;

	OnSightingConfirmed.Broadcast(GetOwner(), Target);

	if (UGSStealthSubsystem* Stealth = UGSStealthSubsystem::Get(this))
	{
		// Exactly one soft signal per confirm - the subsystem owns the de-duplication.
		Stealth->ReportSightingConfirmed(GetOwner(), Target);
	}
}

void UGSSightPerceptionComponent::UpdateDoze(float DeltaTime, bool bSawSomeone)
{
	if (!DozeTuning.bCanDoze || bSawSomeone)
	{
		DozeSeconds = 0.f;
		return;
	}

	const UWorld* World = GetWorld();
	const AGSGameState* GameState = World ? World->GetGameState<AGSGameState>() : nullptr;
	if (GameState && GameState->GetAlarmPhase() >= EGSAlarmPhase::Raid)
	{
		// Nobody dozes once the bell has rung.
		DozeSeconds = 0.f;
		return;
	}

	DozeSeconds += DeltaTime;
}

void UGSSightPerceptionComponent::DrawDebugVision(float DeltaTime) const
{
#if ENABLE_DRAW_DEBUG
	if (!bDrawDebugVisionCone)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bool bAnyConfirmed = false;
	bool bAnyHolding = false;
	for (const TPair<TWeakObjectPtr<AActor>, FGSSightingRecord>& Pair : Sightings)
	{
		bAnyConfirmed |= Pair.Value.bConfirmed;
		bAnyHolding |= (!Pair.Value.bConfirmed && Pair.Value.ConfirmTimer > 0.f);
	}

	FColor ConeColor = FColor::Green;
	if (bAnyConfirmed)
	{
		ConeColor = FColor::Red;
	}
	else if (bAnyHolding)
	{
		ConeColor = FColor::Orange;
	}
	else if (GetDozeAlpha() > 0.f)
	{
		ConeColor = FColor::Blue;
	}

	const float Length = FMath::Max(50.f, Tuning.BaseSightRange * GetDozeRangeMultiplier());
	const float AngleRadians = FMath::DegreesToRadians(FMath::Clamp(GetEffectiveHalfAngleDegrees(), 1.f, 89.f));
	const float Lifetime = FMath::Max(DeltaTime, 0.05f) * 1.05f;

	DrawDebugCone(World, GetEyeLocation(), GetVisionDirection(), Length, AngleRadians, AngleRadians, 16, ConeColor, false, Lifetime, 0, 1.f);

	for (const TPair<TWeakObjectPtr<AActor>, FGSSightingRecord>& Pair : Sightings)
	{
		const AActor* Target = Pair.Key.Get();
		if (!IsValid(Target) || Pair.Value.ConfirmTimer <= 0.f)
		{
			continue;
		}

		const float Progress = FMath::Clamp(Pair.Value.ConfirmTimer / FMath::Max(0.05f, Tuning.ConfirmHoldSeconds), 0.f, 1.f);
		DrawDebugLine(World, GetEyeLocation(), Target->GetActorLocation(), Pair.Value.bConfirmed ? FColor::Red : FColor::Yellow, false, Lifetime, 0, 1.f + 2.f * Progress);
	}
#endif
}

FGSSightingRecord UGSSightPerceptionComponent::GetSightingRecord(AActor* Target) const
{
	if (Target)
	{
		if (const FGSSightingRecord* Found = Sightings.Find(Target))
		{
			return *Found;
		}
	}

	return FGSSightingRecord();
}

float UGSSightPerceptionComponent::GetConfirmProgress01(AActor* Target) const
{
	if (Target)
	{
		if (const FGSSightingRecord* Found = Sightings.Find(Target))
		{
			return FMath::Clamp(Found->ConfirmTimer / FMath::Max(0.05f, Tuning.ConfirmHoldSeconds), 0.f, 1.f);
		}
	}

	return 0.f;
}

bool UGSSightPerceptionComponent::HasConfirmedSighting(AActor* Target) const
{
	if (Target)
	{
		if (const FGSSightingRecord* Found = Sightings.Find(Target))
		{
			return Found->bConfirmed;
		}
	}

	return false;
}

bool UGSSightPerceptionComponent::HasAnyConfirmedSighting() const
{
	for (const TPair<TWeakObjectPtr<AActor>, FGSSightingRecord>& Pair : Sightings)
	{
		if (Pair.Value.bConfirmed)
		{
			return true;
		}
	}

	return false;
}

FVector UGSSightPerceptionComponent::GetEyeLocation() const
{
	const AActor* Observer = GetOwner();
	if (!IsValid(Observer))
	{
		return FVector::ZeroVector;
	}

	FVector EyeLocation = FVector::ZeroVector;
	FRotator EyeRotation = FRotator::ZeroRotator;
	Observer->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	return EyeLocation;
}

FVector UGSSightPerceptionComponent::GetVisionDirection() const
{
	const AActor* Observer = GetOwner();
	if (!IsValid(Observer))
	{
		return FVector::ForwardVector;
	}

	FVector EyeLocation = FVector::ZeroVector;
	FRotator EyeRotation = FRotator::ZeroRotator;
	Observer->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	// The droop: a dozing watchman's cone sags toward his boots.
	EyeRotation.Pitch = FMath::Clamp(EyeRotation.Pitch - GetCurrentDroopDegrees(), -89.f, 89.f);
	return EyeRotation.Vector();
}

float UGSSightPerceptionComponent::GetEffectiveHalfAngleDegrees() const
{
	return Tuning.PeripheralHalfAngleDegrees * FMath::Lerp(1.f, FMath::Clamp(DozeTuning.DozeConeMultiplier, 0.05f, 1.f), GetDozeAlpha());
}

float UGSSightPerceptionComponent::GetDozeRangeMultiplier() const
{
	return FMath::Lerp(1.f, FMath::Clamp(DozeTuning.DozeRangeMultiplier, 0.f, 1.f), GetDozeAlpha());
}

void UGSSightPerceptionComponent::Rouse()
{
	DozeSeconds = 0.f;
}

void UGSSightPerceptionComponent::ForgetAll()
{
	Sightings.Reset();
}
