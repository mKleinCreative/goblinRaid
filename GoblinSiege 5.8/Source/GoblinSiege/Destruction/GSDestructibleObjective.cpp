#include "Destruction/GSDestructibleObjective.h"
#include "Destruction/GSFlammableComponent.h"
#include "Destruction/GSBurnFXComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"

AGSDestructibleObjective::AGSDestructibleObjective()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	GeometryCollectionComponent = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GeometryCollection"));
	RootComponent = GeometryCollectionComponent;
	// Fracture stays dormant until burn-complete - the granary stands solid while it burns.
	GeometryCollectionComponent->SetSimulatePhysics(false);

	FlammableComponent = CreateDefaultSubobject<UGSFlammableComponent>(TEXT("FlammableComponent"));

	// 2026-07-31: burning the granary has to be worth having done ten minutes later. Constructed
	// here rather than added per-Blueprint so every granary in every level - including ones nobody
	// has kitbashed yet - chars while it burns and keeps smoking after the flames have gone out.
	// Hand-wiring this in a Blueprint is exactly the step someone forgets on the one granary the
	// playtest happens to stand in front of.
	BurnFXComponent = CreateDefaultSubobject<UGSBurnFXComponent>(TEXT("BurnFX"));

	// GSObjective_BurnGranaries finds its targets by this tag - "counts what it finds".
	Tags.Add(FName(TEXT("Objective.Granary")));
}

void AGSDestructibleObjective::BeginPlay()
{
	Super::BeginPlay();

	if (FlammableComponent)
	{
		FlammableComponent->OnBurnedDown.AddDynamic(this, &AGSDestructibleObjective::HandleBurnedDown);
	}
}

void AGSDestructibleObjective::HandleBurnedDown()
{
	if (bDestroyed)
	{
		return;
	}
	bDestroyed = true;

	// The collapse moment (design doc §7): burn-complete releases the Chaos simulation. A field
	// system / BP impulse bound to OnObjectiveDestroyed can add the outward blast for drama.
	if (GeometryCollectionComponent)
	{
		GeometryCollectionComponent->SetSimulatePhysics(true);
	}

	OnObjectiveDestroyed.Broadcast();
}
