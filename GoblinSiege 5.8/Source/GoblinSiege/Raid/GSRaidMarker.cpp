#include "Raid/GSRaidMarker.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_EDITORONLY_DATA
#include "Components/BillboardComponent.h"
#include "Components/ArrowComponent.h"
#endif

AGSRaidMarker::AGSRaidMarker()
{
	PrimaryActorTick.bCanEverTick = false;

	// Markers are authoring data that happens to live in a level. They never replicate: everything
	// that reads them (a spawner deciding where to put a guard, a patrol director building a
	// route) runs on the server, and the results of those decisions replicate instead.
	bReplicates = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("MarkerRoot"));
	SetRootComponent(Root);

#if WITH_EDITORONLY_DATA
	MarkerBillboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("MarkerBillboard"));
	if (MarkerBillboard)
	{
		MarkerBillboard->SetupAttachment(Root);
		MarkerBillboard->bIsScreenSizeScaled = true;
	}

	MarkerArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("MarkerArrow"));
	if (MarkerArrow)
	{
		MarkerArrow->SetupAttachment(Root);
		MarkerArrow->ArrowSize = 1.5f;
	}
#endif
}

void AGSRaidMarker::GatherByType(const UObject* WorldContextObject, FGameplayTag InMarkerType,
	TArray<AGSRaidMarker*>& OutMarkers)
{
	OutMarkers.Reset();

	if (!WorldContextObject || !InMarkerType.IsValid())
	{
		return;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AGSRaidMarker> It(World); It; ++It)
	{
		AGSRaidMarker* Marker = *It;
		// MatchesTag, not exact equality: asking for Marker.ObjectiveAnchor should find
		// Marker.ObjectiveAnchor.Field. That hierarchy is the reason these are tags at all.
		if (Marker && Marker->MarkerType.MatchesTag(InMarkerType))
		{
			OutMarkers.Add(Marker);
		}
	}
}

void AGSRaidMarker::GatherLoop(const UObject* WorldContextObject, FName InGroupId,
	TArray<AGSRaidMarker*>& OutMarkers)
{
	OutMarkers.Reset();

	if (!WorldContextObject || InGroupId.IsNone())
	{
		return;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AGSRaidMarker> It(World); It; ++It)
	{
		AGSRaidMarker* Marker = *It;
		if (Marker && Marker->GroupId == InGroupId)
		{
			OutMarkers.Add(Marker);
		}
	}

	// A patrol loop is a sequence. Actor iteration order is whatever the level happened to
	// serialise, so sorting here is what makes OrderIndex mean anything at all.
	OutMarkers.Sort([](const AGSRaidMarker& A, const AGSRaidMarker& B)
	{
		return A.OrderIndex < B.OrderIndex;
	});
}
