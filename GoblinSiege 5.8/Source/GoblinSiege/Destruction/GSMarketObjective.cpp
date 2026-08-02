#include "Destruction/GSMarketObjective.h"
#include "Destruction/GSFlammableComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"

AGSMarketObjective::AGSMarketObjective()
{
	PrimaryActorTick.bCanEverTick = false;
	ObjectiveType = EGSBurnObjectiveType::Market;
	// A market is "gone" well before literally every awning is ash - chasing the last stall
	// across a burning square is not the fantasy.
	CompletionThreshold01 = 0.75f;
}

void AGSMarketObjective::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	AdoptCluster();

	// Bind after adoption so hand-listed and auto-adopted stalls go through the same path.
	int32 BoundCount = 0;
	for (AActor* Stall : Stalls)
	{
		if (!Stall)
		{
			continue;
		}
		if (UGSFlammableComponent* Flammable = Stall->FindComponentByClass<UGSFlammableComponent>())
		{
			Flammable->OnBurnedDown.AddDynamic(this, &AGSMarketObjective::HandleStallBurnedDown);
			++BoundCount;
		}
	}

	InitialStallCount = Stalls.Num();

	// FAIL LOUDLY. An empty market is not an empty market - it is an objective that can never be
	// completed, and it looks identical to a working one in the level. The usual cause is stall
	// actors whose names don't match AutoAdoptNameFilters, or stalls with collision disabled so
	// the overlap query never sees them.
	if (InitialStallCount == 0)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[GoblinSiege] Market objective '%s' adopted ZERO stalls - this objective can "
				 "never be completed. Check AutoAdoptNameFilters (currently %d entries), "
				 "AutoAdoptRadius (%.0f), and that stall actors have collision enabled."),
			*GetName(), AutoAdoptNameFilters.Num(), AutoAdoptRadius);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[GoblinSiege] Market objective '%s' adopted %d stalls (%d flammable)."),
			*GetName(), InitialStallCount, BoundCount);
	}
}

bool AGSMarketObjective::ContainsWorldLocation(const FVector& WorldLocation) const
{
	return FVector::DistSquared(GetActorLocation(), WorldLocation) <= FMath::Square(AutoAdoptRadius);
}

void AGSMarketObjective::AdoptCluster()
{
	if (!bAutoAdoptCluster)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || AutoAdoptRadius <= 0.f)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSMarketAdopt), false, this);

	const bool bAnyHit = World->OverlapMultiByObjectType(
		Overlaps,
		GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
		FCollisionShape::MakeSphere(AutoAdoptRadius),
		Params);

	if (!bAnyHit)
	{
		return;
	}

	for (const FOverlapResult& Result : Overlaps)
	{
		AActor* Other = Result.GetActor();
		if (!Other || Other == this || Stalls.Contains(Other))
		{
			continue;
		}

		if (!Other->FindComponentByClass<UGSFlammableComponent>())
		{
			continue;
		}

		// Name gate: a market square is full of barrels and fences that should burn as scenery
		// without counting toward the objective.
		if (AutoAdoptNameFilters.Num() > 0)
		{
			const FString Name = Other->GetName();
			bool bMatches = false;
			for (const FString& Filter : AutoAdoptNameFilters)
			{
				if (!Filter.IsEmpty() && Name.Contains(Filter))
				{
					bMatches = true;
					break;
				}
			}
			if (!bMatches)
			{
				continue;
			}
		}

		Stalls.Add(Other);
	}
}

void AGSMarketObjective::IgniteAtLocation(const FVector& WorldLocation)
{
	if (!HasAuthority() || IsComplete() || Stalls.Num() == 0)
	{
		return;
	}

	// Light the nearest stall and let the spread system carry it across the cluster.
	AActor* Nearest = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	for (AActor* Stall : Stalls)
	{
		if (!Stall)
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Stall->GetActorLocation(), WorldLocation);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Nearest = Stall;
		}
	}

	if (Nearest)
	{
		if (UGSFlammableComponent* Flammable = Nearest->FindComponentByClass<UGSFlammableComponent>())
		{
			Flammable->Ignite();
		}
	}
}

void AGSMarketObjective::HandleStallBurnedDown()
{
	if (!HasAuthority())
	{
		return;
	}

	// The delegate carries no payload, so recount rather than guess which stall fired. The
	// cluster is tens of actors, not thousands - a recount here is free and can't drift.
	int32 Burnt = 0;
	for (AActor* Stall : Stalls)
	{
		if (!Stall)
		{
			continue;
		}
		if (UGSFlammableComponent* Flammable = Stall->FindComponentByClass<UGSFlammableComponent>())
		{
			if (Flammable->HasBurnedDown())
			{
				++Burnt;

				// Fire the per-stall event once only. The loot layer binds this to destroy that
				// stall's unlooted contents, and doing that twice would be worse than not at all.
				bool bAlreadyReported = false;
				ReportedStalls.Add(Stall, &bAlreadyReported);
				if (!bAlreadyReported)
				{
					OnStallBurnedDown.Broadcast(Stall);
				}
			}
		}
	}

	// ReportedStalls is a high-water mark: once a stall has burned down it stays burned down, even
	// if the actor later destroys itself and drops out of the live recount. Without this floor a
	// self-destroying stall would make completion go BACKWARDS and the market might never finish.
	BurntStallCount = FMath::Max(Burnt, ReportedStalls.Num());

	if (InitialStallCount > 0)
	{
		SetCompletion01(static_cast<float>(BurntStallCount) / static_cast<float>(InitialStallCount));
	}
}

int32 AGSMarketObjective::DouseAtLocation(const FVector& WorldLocation, float Radius)
{
	if (!HasAuthority() || Radius <= 0.f)
	{
		return 0;
	}

	const float RadiusSq = Radius * Radius;
	int32 Doused = 0;

	for (AActor* Stall : Stalls)
	{
		if (!Stall)
		{
			continue;
		}

		if (FVector::DistSquared(Stall->GetActorLocation(), WorldLocation) > RadiusSq)
		{
			continue;
		}

		if (UGSFlammableComponent* Flammable = Stall->FindComponentByClass<UGSFlammableComponent>())
		{
			// Only a stall still in the fight can be saved - ash is ash.
			if (Flammable->IsBurning() && !Flammable->HasBurnedDown())
			{
				Flammable->Extinguish();
				++Doused;
			}
		}
	}

	return Doused;
}

int32 AGSMarketObjective::GetBurningStallCount() const
{
	int32 Burning = 0;
	for (AActor* Stall : Stalls)
	{
		if (!Stall)
		{
			continue;
		}
		if (UGSFlammableComponent* Flammable = Stall->FindComponentByClass<UGSFlammableComponent>())
		{
			if (Flammable->IsBurning())
			{
				++Burning;
			}
		}
	}
	return Burning;
}

void AGSMarketObjective::DrawDebugState() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (AActor* Stall : Stalls)
	{
		if (!Stall)
		{
			continue;
		}

		FColor Colour = FColor(30, 120, 30);
		if (UGSFlammableComponent* Flammable = Stall->FindComponentByClass<UGSFlammableComponent>())
		{
			if (Flammable->HasBurnedDown()) { Colour = FColor(40, 40, 40); }
			else if (Flammable->IsBurning()) { Colour = FColor::Orange; }
		}

		DrawDebugBox(World, Stall->GetActorLocation(), FVector(80.f), Colour, false, 0.3f, 0, 6.f);
	}

	const FString Readout = FString::Printf(TEXT("%s  %d/%d stalls burnt (need %.0f%%)"),
		*GetName(), BurntStallCount, InitialStallCount, CompletionThreshold01 * 100.f);

	DrawDebugString(World, GetActorLocation() + FVector(0, 0, 400.f), Readout,
		nullptr, IsComplete() ? FColor::Green : FColor::White, 0.3f, true);

	// The adoption radius, so a market that swallowed the wrong props is obvious at a glance.
	DrawDebugCircle(World, GetActorLocation(), AutoAdoptRadius, 48, FColor(90, 90, 140),
		false, 0.3f, 0, 4.f, FVector(1, 0, 0), FVector(0, 1, 0), false);
}
