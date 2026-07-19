#include "Destruction/GSDestructibleObjective.h"
#include "Destruction/GSFlammableComponent.h"
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
