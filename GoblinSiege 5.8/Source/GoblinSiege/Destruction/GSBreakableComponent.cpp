#include "Destruction/GSBreakableComponent.h"
#include "Destruction/GSBuildingObjective.h"
#include "Components/StaticMeshComponent.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSBreakable, Log, All);

UGSBreakableComponent::UGSBreakableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGSBreakableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGSBreakableComponent, bBroken);
}

void UGSBreakableComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UGSBreakableComponent::Break(const FVector& ImpactPoint, const FVector& ImpactVelocity)
{
	// Idempotent, and the guard is load-bearing rather than defensive: a torch sticks where it lands
	// and a second one can easily hit the same hole. Without this the window would re-shatter and,
	// worse, re-report an opening to the building.
	if (bBroken)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	bBroken = true;
	OnRep_Broken();

	const bool bFractured = SpawnFracture(ImpactPoint, ImpactVelocity);

	UE_LOG(LogGSBreakable, Log, TEXT("[GoblinSiege] '%s' broken (%s)."),
		*Owner->GetName(),
		bFractured ? TEXT("fractured") : TEXT("no BrokenCollection - hidden + FX; assign one to upgrade"));

	// The point of the whole feature: a broken window is a way in.
	if (bOpensBuilding)
	{
		if (AGSBuildingObjective* Building = AGSBuildingObjective::FindBuildingOwning(this, Owner))
		{
			Building->IgniteInterior(EGSBuildingIgnitionSource::Window);
		}
		else
		{
			// Not fatal - a window in a wall that belongs to no building simply opens onto nothing.
			// Worth saying, though: the usual cause is a building whose AdoptRadius did not reach
			// this piece, which is the same silent-shortfall class of bug the market had.
			UE_LOG(LogGSBreakable, Warning,
				TEXT("[GoblinSiege] '%s' broke but belongs to no building - nothing caught fire. ")
				TEXT("Is it inside a AGSBuildingObjective's AdoptRadius?"), *Owner->GetName());
		}
	}

	OnBroken.Broadcast();
}

void UGSBreakableComponent::OnRep_Broken()
{
	if (!bBroken)
	{
		return;
	}

	RetireIntactMesh();

	if (UNiagaraSystem* FX = BreakFX.LoadSynchronous())
	{
		if (const AActor* Owner = GetOwner())
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FX, Owner->GetActorLocation(),
				Owner->GetActorRotation());
		}
	}
}

void UGSBreakableComponent::RetireIntactMesh()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Collision off as well as hidden. Hiding alone would leave an invisible pane the next torch
	// bounces off, which reads as the window having repaired itself.
	for (UStaticMeshComponent* Mesh : TInlineComponentArray<UStaticMeshComponent*>(Owner))
	{
		Mesh->SetHiddenInGame(true);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

bool UGSBreakableComponent::SpawnFracture(const FVector& ImpactPoint, const FVector& ImpactVelocity)
{
	UGeometryCollection* Collection = BrokenCollection.LoadSynchronous();
	if (!Collection)
	{
		return false; // no fracture authored yet - the hide-and-puff path already ran
	}

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return false;
	}

	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AGeometryCollectionActor* Debris = World->SpawnActor<AGeometryCollectionActor>(
		AGeometryCollectionActor::StaticClass(), Owner->GetActorTransform(), Params);
	if (!Debris)
	{
		return false;
	}

	if (UGeometryCollectionComponent* GC = Debris->GetGeometryCollectionComponent())
	{
		GC->SetRestCollection(Collection);

		// Shards go the way the torch was going, so a throw through a window sprays glass INTO the
		// room rather than back at the thrower.
		const FVector Dir = ImpactVelocity.GetSafeNormal();
		GC->AddImpulseAtLocation(Dir * DebrisImpulse, ImpactPoint);
	}

	// Debris is litter, not a permanent physics tenant. A raid can break a lot of windows, and every
	// collection left lying around is rigid bodies and draw calls on a render thread already at
	// 15.4ms of its 16.67ms budget (perf ticket 002).
	Debris->SetLifeSpan(DebrisLifeSeconds);
	return true;
}
