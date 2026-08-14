#include "GSClimbLibrary.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

namespace
{
	// The probe sweeps from head height down to just above the climber, mirroring the Blueprint
	// geometry it replaces so the numbers stay comparable with the measurements in ticket #079.
	constexpr float ProbeTopOffset    = 460.f;
	constexpr float ProbeBottomOffset =  30.f;
	constexpr float ProbeRadius       =  25.f;

	constexpr float InsetMin  =  80.f;
	constexpr float InsetMax  = 340.f;
	constexpr float InsetStep =  20.f;

	// How far above a candidate deck we look for a roof. Anything found means we are under the
	// building, not on top of it.
	constexpr float SkyProbeHeight = 1500.f;
	constexpr float SkyProbeBase   =   15.f;

	constexpr float FallbackWalkableZ = 0.4695f;   // matches WalkableFloorAngle 62 on the goblin
}

static TAutoConsoleVariable<int32> CVarLogLedge(
	TEXT("GS.Climb.LogLedge"),
	0,
	TEXT("Log every ledge search: what each inset found and why it was rejected. 0 off, 1 on."),
	ECVF_Default);

FGSClimbLedgeResult UGSClimbLibrary::FindClimbLedge(ACharacter* Climber, FVector WallNormal)
{
	FGSClimbLedgeResult Result;

	if (!Climber)
	{
		return Result;
	}

	UWorld* World = Climber->GetWorld();
	if (!World)
	{
		return Result;
	}

	// A zero or near-zero normal means the caller has no wall; searching would probe straight down
	// through the climber and find the ground.
	FVector Outward = WallNormal;
	if (!Outward.Normalize())
	{
		return Result;
	}
	const FVector Inward = -Outward;

	const FVector From = Climber->GetActorLocation();

	float WalkableZ = FallbackWalkableZ;
	if (const UCharacterMovementComponent* Move = Climber->GetCharacterMovement())
	{
		WalkableZ = Move->GetWalkableFloorZ();
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSFindClimbLedge), /*bTraceComplex=*/false, Climber);
	Params.AddIgnoredActor(Climber);

	const FCollisionShape Probe = FCollisionShape::MakeSphere(ProbeRadius);

	for (float Inset = InsetMin; Inset <= InsetMax; Inset += InsetStep)
	{
		const FVector Column = From + Inward * Inset;
		const FVector Top    = FVector(Column.X, Column.Y, From.Z + ProbeTopOffset);
		const FVector Bottom = FVector(Column.X, Column.Y, From.Z + ProbeBottomOffset);

		FHitResult DeckHit;
		if (!World->SweepSingleByChannel(DeckHit, Top, Bottom, FQuat::Identity, ECC_Visibility, Probe, Params))
		{
			++Result.RejectedEmpty;
			continue;
		}

		if (DeckHit.ImpactNormal.Z < WalkableZ)
		{
			++Result.RejectedSteep;
			continue;
		}

		// Open sky above it, or it is a floor inside the building.
		const FVector SkyFrom = DeckHit.ImpactPoint + FVector(0.f, 0.f, SkyProbeHeight);
		const FVector SkyTo   = DeckHit.ImpactPoint + FVector(0.f, 0.f, SkyProbeBase);

		FHitResult SkyHit;
		if (World->LineTraceSingleByChannel(SkyHit, SkyFrom, SkyTo, ECC_Visibility, Params))
		{
			++Result.RejectedInterior;
			continue;
		}

		Result.bFound  = true;
		Result.Deck    = DeckHit.ImpactPoint;
		Result.Inset   = Inset;
		Result.Rise    = DeckHit.ImpactPoint.Z - From.Z;
		Result.NormalZ = DeckHit.ImpactNormal.Z;
		break;
	}

	if (CVarLogLedge.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log,
			TEXT("GSDBG|LEDGE|found=%s|inset=%.0f|rise=%.0f|nz=%.2f|z=%.1f|steep=%d|interior=%d|empty=%d|walkZ=%.3f"),
			Result.bFound ? TEXT("true") : TEXT("false"),
			Result.Inset, Result.Rise, Result.NormalZ, From.Z,
			Result.RejectedSteep, Result.RejectedInterior, Result.RejectedEmpty, WalkableZ);
	}

	return Result;
}

bool UGSClimbLibrary::IsClimbBlockedUpward(ACharacter* Climber, float RiseDistance)
{
	if (!Climber)
	{
		return false;
	}

	UWorld* World = Climber->GetWorld();
	const UCapsuleComponent* Capsule = Climber->GetCapsuleComponent();
	if (!World || !Capsule)
	{
		return false;
	}

	const FVector From = Climber->GetActorLocation();
	const FVector To   = From + FVector(0.f, 0.f, FMath::Max(1.f, RiseDistance));

	// Use the capsule's CURRENT dimensions - the climb shrinks it to radius 24 to clear trim bands,
	// and testing against the unshrunk size would report blocked while the real capsule fits.
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSClimbBlockedUpward), /*bTraceComplex=*/false, Climber);
	Params.AddIgnoredActor(Climber);

	FHitResult Hit;
	return World->SweepSingleByChannel(Hit, From, To, FQuat::Identity, ECC_Visibility, Shape, Params);
}
