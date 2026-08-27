#include "Destruction/GSDestructibleObjective.h"
#include "Destruction/GSFlammableComponent.h"
#include "Destruction/GSBurnFXComponent.h"
#include "Destruction/GSCrumbleComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"

AGSDestructibleObjective::AGSDestructibleObjective()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	GeometryCollectionComponent = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GeometryCollection"));
	RootComponent = GeometryCollectionComponent;
	// Fracture stays dormant until burn-complete - the objective stands solid while it burns.
	GeometryCollectionComponent->SetSimulatePhysics(false);

	CrumbleComponent = CreateDefaultSubobject<UGSCrumbleComponent>(TEXT("Crumble"));

	// Kept even though a statue does not burn: this class is the general "finished when it comes
	// apart" objective, and a burnable one still wants to char while it goes. What was removed is
	// the burn DECIDING that it is destroyed, not the burn happening at all.
	FlammableComponent = CreateDefaultSubobject<UGSFlammableComponent>(TEXT("FlammableComponent"));

	// 2026-07-31: destroying this has to be worth having done ten minutes later. Constructed here
	// rather than added per-Blueprint so every instance in every level - including ones nobody has
	// kitbashed yet - chars while it burns and keeps smoking after the flames have gone out.
	// Hand-wiring this in a Blueprint is exactly the step someone forgets on the one the playtest
	// happens to stand in front of.
	BurnFXComponent = CreateDefaultSubobject<UGSBurnFXComponent>(TEXT("BurnFX"));

	// AGSObjective_ToppleStatue finds its targets by this tag - "counts what it finds".
	// Renamed from "Objective.Granary" 2026-08-18 with queue #156; a CoreRedirect covers the
	// class rename, but a tag typed by hand onto a placed actor is NOT redirected - see #189.
	Tags.Add(FName(TEXT("Objective.Statue")));
}

void AGSDestructibleObjective::BeginPlay()
{
	Super::BeginPlay();

	if (CrumbleComponent)
	{
		CrumbleComponent->OnCrumbled.AddDynamic(this, &AGSDestructibleObjective::HandleCrumbled);
	}
}

void AGSDestructibleObjective::HandleCrumbled()
{
	if (bDestroyed)
	{
		return;
	}
	bDestroyed = true;

	// No physics work here any more, and that is the point. Releasing the collection is the crumble
	// component's job and it has already happened by the time this fires; this class only has to say
	// that the objective is finished.
	OnObjectiveDestroyed.Broadcast();
}
