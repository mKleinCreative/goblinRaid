#include "Weapons/GSAmmoPickup.h"

#include "Components/ACFInventoryComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Items/ACFItem.h"

AGSAmmoPickup::AGSAmmoPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	BundleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BundleMesh"));
	SetRootComponent(BundleMesh);
	// The mesh is decoration. All the collision lives on the sphere below, so a bundle can never
	// block a shot or shove the player off a ledge.
	BundleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->SetupAttachment(BundleMesh);
	PickupSphere->SetSphereRadius(PickupRadius);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AGSAmmoPickup::BeginPlay()
{
	Super::BeginPlay();

	// Radius is EditAnywhere, so a designer's value has to reach the component - the constructor's
	// copy was made before any override was loaded.
	PickupSphere->SetSphereRadius(PickupRadius);

	if (!HasAuthority())
	{
		return;
	}

	if (!ArrowItemClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] '%s' has no ArrowItemClass, so walking over it will do nothing. "
				 "Set it on the Blueprint."), *GetName());
		return;
	}

	PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &AGSAmmoPickup::HandleOverlap);

	// A bundle that spawns UNDER someone - a corpse drop at the feet of the goblin who made the
	// corpse - would otherwise sit there until they stepped off and back on, because BeginOverlap
	// has already been and gone. Sweep once for anyone already inside.
	TArray<AActor*> Already;
	PickupSphere->GetOverlappingActors(Already, APawn::StaticClass());
	for (AActor* Actor : Already)
	{
		if (APawn* Pawn = Cast<APawn>(Actor))
		{
			if (TryGiveTo(Pawn))
			{
				Destroy();
				return;
			}
		}
	}
}

void AGSAmmoPickup::HandleOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	if (!HasAuthority())
	{
		return;
	}

	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		if (TryGiveTo(Pawn))
		{
			Destroy();
		}
		// Refused: deliberately NOT destroyed. See TryGiveTo.
	}
}

bool AGSAmmoPickup::TryGiveTo(APawn* Pawn)
{
	if (!IsValid(Pawn) || !ArrowItemClass)
	{
		return false;
	}

	UACFInventoryComponent* Inventory = Pawn->FindComponentByClass<UACFInventoryComponent>();
	if (!Inventory)
	{
		// Not a complaint: goblins and defenders walk over these all the time and have no business
		// picking them up.
		return false;
	}

	const int32 Before = Inventory->GetTotalCountOfItemsByClass(ArrowItemClass);

	// bAutoEquip FALSE, and it matters. True routes the add through HandleItemAdded into
	// CanBeEquipped, which returns false with NO LOG for anything the pawn has no slot or weapon
	// type for (#270's second fault). Arrows are never equipped; they must not touch that path, and
	// passing false explicitly means a REAL equip failure later cannot be mistaken for this one.
	Inventory->AddItemToInventoryByClass(ArrowItemClass, ArrowCount, /*bAutoEquip=*/ false);

	const int32 After = Inventory->GetTotalCountOfItemsByClass(ArrowItemClass);
	if (After <= Before)
	{
		// ACF refused it - almost certainly MaxInventoryWeight or MaxInventorySlots, both of which
		// bail out of Internal_AddItem with no log at all. Say it once and LEAVE THE BUNDLE THERE,
		// so a full player can come back for it rather than watching their arrows evaporate.
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] '%s' could not take %d arrows from '%s' - the inventory refused the "
				 "add and still holds %d. Check MaxInventoryWeight and MaxInventorySlots against the "
				 "item's ItemWeight and MaxInventoryStack. The bundle has been left in place."),
			*Pawn->GetName(), ArrowCount, *GetName(), After);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[GoblinSiege] '%s' picked up %d arrows (%d -> %d)."),
		*Pawn->GetName(), After - Before, Before, After);
	return true;
}
