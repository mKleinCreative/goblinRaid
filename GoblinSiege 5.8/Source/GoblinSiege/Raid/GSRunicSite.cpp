#include "Raid/GSRunicSite.h"
#include "Raid/GSRaidDirector.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSRunicSite, Log, All);

AGSRunicSite::AGSRunicSite()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SiteRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SiteRoot"));
	SetRootComponent(SiteRoot);

	ExtractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ExtractionSphere"));
	ExtractionSphere->SetupAttachment(SiteRoot);
	ExtractionSphere->SetSphereRadius(ExtractionRadius);
	// Overlap-only against pawns. QueryOnly so the circle never pushes the player around - a
	// collision response here would feel like walking into glass at the one moment the game most
	// wants to feel like relief.
	ExtractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ExtractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	ExtractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	ExtractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ExtractionSphere->SetGenerateOverlapEvents(true);

	PortalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMesh"));
	PortalMesh->SetupAttachment(SiteRoot);
	// The portal is scenery, not geometry: the player walks THROUGH it. Collision here would mean
	// standing in an open portal and being unable to enter it.
	PortalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PortalMesh->SetHiddenInGame(true);

	PortalFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PortalFX"));
	PortalFX->SetupAttachment(SiteRoot);
	PortalFX->bAutoActivate = false;
}

void AGSRunicSite::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGSRunicSite, bPortalOpen);
}

void AGSRunicSite::BeginPlay()
{
	Super::BeginPlay();

	ExtractionSphere->SetSphereRadius(ExtractionRadius);

	// Paint the closed state immediately. Without this the portal shows whatever the Blueprint's
	// editor defaults left behind until the first state change - the same class of bug the health
	// bar had, where a decorative visual reads as a live one.
	ApplyPortalVisuals();

	if (!HasAuthority())
	{
		return;
	}

	if (bStartOpen)
	{
		SetPortalOpen(true);
	}

	UGSRaidDirector* Director = UGSRaidDirector::Get(this);
	if (!Director)
	{
		UE_LOG(LogGSRunicSite, Error,
			TEXT("[GoblinSiege] '%s' found no UGSRaidDirector - the portal will never open on its own."),
			*GetName());
		return;
	}

	if (bOpenOnObjectivesComplete)
	{
		Director->OnRaidObjectivesComplete.AddDynamic(this, &AGSRunicSite::HandleObjectivesComplete);

		// Cover the case where the objectives were already satisfied before this site existed -
		// a site spawned mid-raid, or a BeginPlay ordering surprise. Binding alone would miss a
		// broadcast that has already happened.
		if (Director->AreObjectivesComplete())
		{
			SetPortalOpen(true);
		}
	}
}

void AGSRunicSite::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGSRaidDirector* Director = UGSRaidDirector::Get(this))
	{
		Director->OnRaidObjectivesComplete.RemoveDynamic(this, &AGSRunicSite::HandleObjectivesComplete);
	}
	Super::EndPlay(EndPlayReason);
}

void AGSRunicSite::HandleObjectivesComplete()
{
	SetPortalOpen(true);
}

void AGSRunicSite::SetPortalOpen(bool bOpen)
{
	if (!HasAuthority() || bPortalOpen == bOpen)
	{
		return;
	}

	bPortalOpen = bOpen;

	// Authority runs its own OnRep - the standard pattern in this module (see
	// AGSBurnObjectiveBase::SetCompletion01), so listen-server and client take the same path.
	OnRep_PortalOpen();

	UE_LOG(LogGSRunicSite, Log, TEXT("[GoblinSiege] Portal at '%s' is now %s."),
		*GetName(), bOpen ? TEXT("OPEN") : TEXT("CLOSED"));

	if (bOpen)
	{
		TryExtractOverlappingPawns();
	}
}

void AGSRunicSite::OnRep_PortalOpen()
{
	ApplyPortalVisuals();
	OnPortalOpenChanged.Broadcast(bPortalOpen);
}

void AGSRunicSite::ApplyPortalVisuals()
{
	if (PortalMesh)
	{
		PortalMesh->SetHiddenInGame(!bPortalOpen);
	}

	if (PortalFX)
	{
		if (bPortalOpen)
		{
			PortalFX->Activate(true);
		}
		else
		{
			PortalFX->Deactivate();
		}
	}
}

// ====================================================================== extraction

bool AGSRunicSite::IsExtractablePawn(const AActor* Actor)
{
	const APawn* Pawn = Cast<APawn>(Actor);
	return Pawn && Pawn->IsPlayerControlled();
}

void AGSRunicSite::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	TryExtract(OtherActor);
}

void AGSRunicSite::TryExtractOverlappingPawns()
{
	TArray<AActor*> Overlapping;
	ExtractionSphere->GetOverlappingActors(Overlapping, APawn::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		TryExtract(Actor);
	}
}

void AGSRunicSite::TryExtract(AActor* OtherActor)
{
	if (!HasAuthority() || !bPortalOpen || !IsExtractablePawn(OtherActor))
	{
		return;
	}

	UGSRaidDirector* Director = UGSRaidDirector::Get(this);
	if (!Director || Director->HasRaidEnded())
	{
		return;
	}

	UE_LOG(LogGSRunicSite, Log, TEXT("[GoblinSiege] '%s' extracted through '%s'."),
		*OtherActor->GetName(), *GetName());

	Director->EndRaid(EGSRaidResult::Extracted);
}

// ====================================================================== spawning

FTransform AGSRunicSite::GetSpawnTransform() const
{
	// In front of the portal plane, facing away from it, and always OUTSIDE the extraction sphere.
	//
	// The clamp is the load-bearing part. Spawning inside an open circle would extract the player
	// the instant they respawned - so dying once with the objectives already burned would end the
	// raid as a win the player never chose. Deriving the distance from ExtractionRadius rather
	// than trusting the authored offset means a designer widening the circle cannot reintroduce
	// that by editing one number and not the other.
	const FVector Forward = GetActorForwardVector();
	const float SafeOffset = FMath::Max(SpawnForwardOffset, ExtractionRadius + 200.f);
	const FVector Location = GetActorLocation() + Forward * SafeOffset;
	return FTransform(Forward.Rotation(), Location);
}
