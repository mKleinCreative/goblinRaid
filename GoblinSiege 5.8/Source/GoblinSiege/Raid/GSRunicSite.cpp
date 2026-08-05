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

void AGSRunicSite::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	// Left the circle: from now on, a portal opening around this pawn counts. See ArmedPawns.
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		ArmedPawns.Add(Pawn);
	}
}

void AGSRunicSite::TryExtractOverlappingPawns()
{
	TArray<AActor*> Overlapping;
	ExtractionSphere->GetOverlappingActors(Overlapping, APawn::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		// ONLY pawns that have been away. A pawn still standing where it spawned has not raided
		// anything yet, and opening the portal underneath it must not end the raid for it.
		APawn* Pawn = Cast<APawn>(Actor);
		if (Pawn && ArmedPawns.Contains(Pawn))
		{
			TryExtract(Actor);
		}
		else if (Pawn)
		{
			UE_LOG(LogGSRunicSite, Log,
				TEXT("[GoblinSiege] Portal opened around '%s', which has not left the circle yet - ")
				TEXT("not extracting. Step out and back in."), *Pawn->GetName());
		}
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

bool AGSRunicSite::FindStandableSpot(const FVector& Candidate, FVector& OutSpot) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSRunicSiteSpawn), false, this);

	// Ground first. Starting only just above the site's own height, not far overhead - see
	// SpawnTraceUpDistance for why a high start finds rooftops.
	FHitResult Hit;
	const FVector Start = Candidate + FVector(0.f, 0.f, SpawnTraceUpDistance);
	const FVector End = Candidate - FVector(0.f, 0.f, SpawnTraceDownDistance);
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return false; // nothing under it at all
	}

	const FVector Centre = Hit.Location + FVector(0.f, 0.f, SpawnCapsuleHalfHeight + 10.f);

	// Then: would the player actually FIT? This is the check the original code lacked entirely, and
	// the only one that can catch a point buried inside a building.
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(SpawnCapsuleRadius, SpawnCapsuleHalfHeight);
	if (World->OverlapBlockingTestByChannel(Centre, FQuat::Identity, ECC_Pawn, Capsule, Params))
	{
		return false; // occupied - a wall, a floor above, the inside of a basement
	}

	OutSpot = Centre;
	return true;
}

FTransform AGSRunicSite::GetSpawnTransform() const
{
	// ON THE SITE, on solid ground. Michael's call, 2026-08-05: "spawn me on the mesh that I gave
	// you, not underneath the map."
	//
	// The previous version pushed the spawn SpawnForwardOffset along the site's forward vector to
	// keep it outside the extraction sphere. That was solving the wrong problem in the worst way:
	// it took a point that is known-good by construction (the site, which a designer placed on
	// walkable ground) and threw it 1400 uu in whatever direction the site happened to face -
	// straight into a house, 122 uu below SM_House_Floor_5x4_662. The instant-extraction case it
	// was guarding is handled properly now by the arming rule in TryExtractOverlappingPawns, which
	// is where it belonged: a spawn position is a bad place to encode a rule about overlaps.
	//
	// The ground trace is what makes "not underneath the map" a guarantee rather than a hope: the
	// site's Z is used only as the search origin, never as the answer.
	const FVector Origin = GetActorLocation();
	const FRotator Facing(0.f, GetActorRotation().Yaw, 0.f);

	FVector Spot;
	if (FindStandableSpot(Origin, Spot))
	{
		return FTransform(Facing, Spot);
	}

	// The site itself is blocked - something was built on top of it. Fan outward rather than give
	// up, still preferring to stay close, and only then accept the raw location.
	const int32 Bearings = FMath::Max(1, SpawnBearingCount);
	for (float Ring : { 300.f, 700.f, 1200.f })
	{
		for (int32 Step = 0; Step < Bearings; ++Step)
		{
			const FVector Dir = FRotator(0.f, Facing.Yaw + (360.f / Bearings) * Step, 0.f).Vector();
			if (FindStandableSpot(Origin + Dir * Ring, Spot))
			{
				UE_LOG(LogGSRunicSite, Warning,
					TEXT("[GoblinSiege] '%s' is obstructed; spawning %.0f uu away instead. ")
					TEXT("Move the site onto clear ground."), *GetName(), Ring);
				return FTransform(Facing, Spot);
			}
		}
	}

	UE_LOG(LogGSRunicSite, Error,
		TEXT("[GoblinSiege] '%s' found NO standable ground anywhere near it. Spawning on the site ")
		TEXT("itself - if that is inside geometry, the site is placed badly."), *GetName());
	return FTransform(Facing, Origin);
}
