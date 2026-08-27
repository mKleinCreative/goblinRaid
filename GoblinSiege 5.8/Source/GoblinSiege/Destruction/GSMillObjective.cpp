#include "Destruction/GSMillObjective.h"
#include "Destruction/GSFlammableComponent.h"
#include "Destruction/GSBurnFXComponent.h"
#include "Core/GSGameState.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/BoxComponent.h"
#include "Destruction/GSCrumbleComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "EngineUtils.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

AGSMillObjective::AGSMillObjective()
{
	PrimaryActorTick.bCanEverTick = false;
	ObjectiveType = EGSBurnObjectiveType::Windmill;

	// The mill is binary - it is standing or it is not. Completion is driven straight from the
	// buildup timer so the HUD can show a fuse, but only 1.0 counts.
	CompletionThreshold01 = 1.f;
	AlarmOnCompletion = 60.f; // a windmill going up is the loudest thing in the raid

	// Attach to the base's ObjectiveRoot rather than replacing it as root - replacing would leave
	// the inherited scene component dangling and unattached.
	MillMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MillMesh"));
	MillMesh->SetupAttachment(GetRootComponent());

	// Q-34 ruling (2026-08-01): the sails. Attached to the mill body; a Blueprint that never
	// assigns SailMesh a mesh simply has no sails and the spin machinery idles harmlessly.
	// They keep turning while the fuse burns - "having the windmill spinning while burning would
	// be a great sight" - and stop at detonation (see OnRep_Stage).
	SailMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SailMesh"));
	SailMesh->SetupAttachment(MillMesh);

	// Rate and updated component are applied in BeginPlay: SetUpdatedComponent on a CDO is
	// unreliable (movement components resolve against the owner's root during registration).
	// The way in. Tagged here so the BeginPlay binding picks it up through the same tag lookup a
	// hand-placed component would have used - no special case, and a level that DOES author its own
	// tagged window just gets two valid triggers rather than a conflict.
	WreckFireSystem = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(TEXT("/Game/VFX/NS_GS_SurfaceFire.NS_GS_SurfaceFire")));

	WindowTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("WindowTrigger"));
	WindowTrigger->SetupAttachment(GetRootComponent());
	WindowTrigger->ComponentTags.Add(WindowComponentTag);
	WindowTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WindowTrigger->SetCollisionResponseToAllChannels(ECR_Overlap);
	WindowTrigger->SetGenerateOverlapEvents(true);
	// Never blocks. A torch that bounced off the window instead of going through it would be the
	// same bug in a new costume.
	WindowTrigger->SetCollisionObjectType(ECC_WorldDynamic);

	SailSpin = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("SailSpin"));
	SailSpin->bAutoActivate = true;

	// 2026-07-31 (Q-34): the mill chars as its fuse burns, without gaining a flammable component.
	//
	// Every other burnable in the hamlet gets char for free, because UGSBurnFXComponent binds
	// itself to a UGSFlammableComponent's OnIgnited/OnBurnedDown in BeginPlay. The mill has no
	// flammable component ON PURPOSE - see BurnFXComponent in the header for why adding one would
	// change its ignition rules - so the drive is manual: TickBuildup and OnRep_Stage push
	// SetBurnAmount directly. UGSBurnFXComponent is explicitly built to work that way ("Deliberately
	// drop-on-anything... a missing flammable component is a warning, not a crash"); with none
	// present it logs exactly once in BeginPlay and skips the auto-drive, and SetBurnAmount remains
	// a pure material driver. Not a UPROPERTY(EditAnywhere) - the mill always wants it.
	//
	// This gets the burn-down spec's 0.33 / 0.66 / 1.0 char steps onto the windmill with zero
	// change to what lights it, which was the whole requirement.
	BurnFXComponent = CreateDefaultSubobject<UGSBurnFXComponent>(TEXT("BurnFX"));
}

void AGSMillObjective::BeginPlay()
{
	Super::BeginPlay();
	BindWindowVolumes();

	// Q-34: aim the spinner at the sails and wind it up. Done here rather than in the constructor
	// because a movement component left to its own devices resolves UpdatedComponent to the owner's
	// ROOT during registration - which would rotate the whole mill.
	if (SailSpin)
	{
		SailSpin->SetUpdatedComponent(SailMesh);
		SailSpin->RotationRate = SailSpinRate;

		// Stream-in / late-join catch-up: an already-razed mill must not arrive with turning sails.
		if (Stage == EGSMillStage::Detonated)
		{
			SailSpin->Deactivate();
		}
	}
}

void AGSMillObjective::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BuildupTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AGSMillObjective::BindWindowVolumes()
{
	if (!HasAuthority())
	{
		return;
	}

	// Offset and extent are applied here rather than in the constructor so a designer's edit takes
	// effect without a recompile.
	if (WindowTrigger)
	{
		WindowTrigger->SetRelativeLocation(WindowTriggerOffset);
		WindowTrigger->SetBoxExtent(WindowTriggerExtent);
	}

	TArray<UPrimitiveComponent*> Primitives;
	GetComponents<UPrimitiveComponent>(Primitives);

	for (UPrimitiveComponent* Prim : Primitives)
	{
		if (Prim && Prim->ComponentHasTag(WindowComponentTag))
		{
			Prim->OnComponentBeginOverlap.AddDynamic(this, &AGSMillObjective::OnWindowOverlap);
		}
	}
}

void AGSMillObjective::OnWindowOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	if (!OtherActor || !OtherActor->ActorHasTag(LitTorchActorTag))
	{
		return;
	}

	IgniteInterior();
}

// ====================================================================== ignition

void AGSMillObjective::IgniteAtLocation(const FVector& /*WorldLocation*/)
{
	// Deliberate no-op. Stone base, sails out of reach: exterior fire does not take the mill.
	// Announce the refusal so it reads as a rule rather than a dud.
	if (Stage == EGSMillStage::Intact)
	{
		OnExteriorIgnitionRefused.Broadcast();
	}
}

void AGSMillObjective::IgniteInterior()
{
	if (!HasAuthority() || Stage != EGSMillStage::Intact)
	{
		return;
	}

	BuildupElapsed = 0.f;
	SetStage(EGSMillStage::Smouldering);

	// Dust catching inside is visible through the windows - the town gets to notice early.
	if (UWorld* World = GetWorld())
	{
		if (AGSGameState* GS = World->GetGameState<AGSGameState>())
		{
			// The mill's interior burn never touches UGSFlammableComponent, so arm the
			// unseen-fire fuse here or a mill lit from a dark lane would tick down to
			// detonation with the town still asleep.
			GS->ReportFireStarted();
			GS->AddAlarm(10.f, EGSAlarmSource::ObjectiveProgress);
		}

		World->GetTimerManager().SetTimer(BuildupTimerHandle, this,
			&AGSMillObjective::TickBuildup, BuildupTickInterval, true);
	}
}

void AGSMillObjective::DrawDebugState() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FColor Colour = FColor(120, 120, 130);
	FString StageName = TEXT("Intact (windows only)");

	switch (Stage)
	{
	case EGSMillStage::Smouldering:
		Colour = FColor::Orange;
		StageName = FString::Printf(TEXT("SMOULDERING - %.1fs to detonation"), GetSecondsToDetonation());
		break;
	case EGSMillStage::Detonated:
		Colour = FColor::Red;
		StageName = TEXT("DETONATED");
		break;
	default:
		break;
	}

	DrawDebugBox(World, GetActorLocation() + FVector(0, 0, 300.f), FVector(200.f, 200.f, 300.f),
		Colour, false, 0.3f, 0, 8.f);

	DrawDebugString(World, GetActorLocation() + FVector(0, 0, 800.f),
		FString::Printf(TEXT("%s  %s"), *GetName(), *StageName),
		nullptr, IsComplete() ? FColor::Green : FColor::White, 0.3f, true);
}

void AGSMillObjective::TickBuildup()
{
	if (!HasAuthority() || Stage != EGSMillStage::Smouldering)
	{
		return;
	}

	BuildupElapsed += BuildupTickInterval;

	const float Alpha = DustBuildupSeconds > 0.f
		? FMath::Clamp(BuildupElapsed / DustBuildupSeconds, 0.f, 1.f)
		: 1.f;

	// 2026-07-31 (Q-34): char tracks the fuse. Pushed BEFORE SetCompletion01, because at Alpha 1.0
	// that call runs straight through HandleCompleted -> Detonate -> SetStage(Detonated) ->
	// OnRep_Stage, which pins the char to 1.0 itself; doing it in this order means the final value
	// is written once by the terminal path rather than raced by this one.
	//
	// SetBurnAmount rather than any flammable-driven route: the mill has no UGSFlammableComponent
	// and deliberately never will (see BurnFXComponent in the header). Alpha is monotonic for the
	// whole fuse - BuildupElapsed only rises and IgniteInterior is idempotent past Intact - so
	// pushing it raw through a setter that does NOT ratchet is safe here, unlike the flammable
	// path, whose progress can be reset by a re-ignite and which therefore needs BurnFXTick's
	// ratchet. The result is a smooth 0 -> 1 scorch over DustBuildupSeconds, which is the
	// burn-down spec's 0.33 / 0.66 / 1.0 steps read continuously.
	if (BurnFXComponent)
	{
		BurnFXComponent->SetBurnAmount(Alpha);
	}

	// Completion doubles as the fuse readout. Threshold is 1.0, so SetCompletion01 fires
	// HandleCompleted() exactly when the fuse runs out.
	SetCompletion01(Alpha);
}

float AGSMillObjective::GetSecondsToDetonation() const
{
	if (Stage != EGSMillStage::Smouldering)
	{
		return 0.f;
	}
	return FMath::Max(0.f, DustBuildupSeconds - BuildupElapsed);
}

// ====================================================================== detonation

void AGSMillObjective::HandleCompleted()
{
	Super::HandleCompleted();
	Detonate();
}

void AGSMillObjective::Detonate()
{
	if (!HasAuthority() || Stage == EGSMillStage::Detonated)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BuildupTimerHandle);
	}

	SetStage(EGSMillStage::Detonated);

	// The tower comes down. After SetStage so the char, the stopped sails and the objective all land
	// first - the sink is the payoff on top of that, not a replacement for it.
	if (bSinkOnDetonation)
	{
		SinkTower();
	}

	// Take the yard with it: everything flammable in reach catches.
	UWorld* World = GetWorld();
	if (World && DetonationIgniteRadius > 0.f)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GSMillDetonation), false, this);

		if (World->OverlapMultiByObjectType(
				Overlaps,
				GetActorLocation(),
				FQuat::Identity,
				FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
				FCollisionShape::MakeSphere(DetonationIgniteRadius),
				Params))
		{
			TSet<AActor*> Seen;
			for (const FOverlapResult& Result : Overlaps)
			{
				AActor* Other = Result.GetActor();
				if (!Other || Other == this)
				{
					continue;
				}

				bool bAlready = false;
				Seen.Add(Other, &bAlready);
				if (bAlready)
				{
					continue;
				}

				if (UGSFlammableComponent* Flammable = Other->FindComponentByClass<UGSFlammableComponent>())
				{
					Flammable->Ignite();
				}
			}
		}
	}
}

// ====================================================================== state

void AGSMillObjective::SetStage(EGSMillStage NewStage)
{
	if (Stage == NewStage)
	{
		return;
	}

	Stage = NewStage;
	OnRep_Stage();
}

void AGSMillObjective::OnRep_Stage()
{
	OnMillStageChanged.Broadcast(Stage);

	switch (Stage)
	{
	case EGSMillStage::Smouldering:
		OnInteriorIgnited.Broadcast();
		break;

	case EGSMillStage::Detonated:
		// Cosmetic swap runs on every machine off the replicated stage, so late joiners and
		// clients see the ruin without needing the whole sequence replayed.
		if (MillMesh && DestroyedMesh)
		{
			MillMesh->SetStaticMesh(DestroyedMesh);

			// A ruin with pristine sails still bolted on reads as a bug; hide them only when a
			// ruin mesh actually swapped in. With no DestroyedMesh (the current dressing mills)
			// the sails stay - charred and stopped - which is exactly Michael's "texture turns
			// black, breaking mesh later" prototype read.
			if (SailMesh)
			{
				SailMesh->SetHiddenInGame(true);
			}
		}

		// Q-34 (2026-08-01): the sails stop dead the moment the mill is gone. Runs on server and
		// every client off the replicated stage, same as the char pin below.
		if (SailSpin)
		{
			SailSpin->Deactivate();
		}

		// 2026-07-31 (Q-34): pin the char at full on the terminal stage.
		//
		// This is the second of the two drive points, and the one that matters in co-op. TickBuildup
		// is authority-gated, so on a remote client the fuse ramp never runs and the mill would
		// otherwise stay pristine right up to the moment it exploded. OnRep_Stage runs on the
		// server AND on every client (SetStage calls it directly on the authority), and Stage is
		// replicated, so this line is what guarantees a razed mill reads as razed on every machine
		// - including for a late joiner, who gets the RepNotify on initial replication and so
		// arrives to a blackened ruin rather than a clean one.
		//
		// KNOWN AND ACCEPTED: a remote client sees the char STEP to 1.0 at detonation rather than
		// ramp over the 9 s fuse. Replicating the ramp needs Completion01's RepNotify, which is
		// non-virtual on AGSBurnObjectiveBase, so it is out of scope here (Q-36 covers the same
		// class of problem for the flammable component). The detonation is a mesh swap plus a large
		// VFX beat, so a char step underneath it is not a visible artefact.
		if (BurnFXComponent)
		{
			BurnFXComponent->SetBurnAmount(1.f);

			// Q-34 (2026-08-01): black AND smouldering. The flammable path starts its smolder from
			// HandleBurnedDown, which the mill's manual drive never reaches - without this call the
			// mill would char and then sit there smokeless, and Michael's ruling is explicit that
			// a burnt thing "turns black and smoulders after it's been burnt". Idempotent, and runs
			// on every machine off the replicated stage like everything else in this branch.
			BurnFXComponent->SpawnSmolder();
		}

		OnMillDetonated.Broadcast();
		break;

	default:
		break;
	}
}

void AGSMillObjective::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGSMillObjective, Stage);
}

void AGSMillObjective::SinkTower()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}

	// Find the mill's geometry. See MillGeometryNameFilter: the objective owns no mesh of its own on
	// this map, so the thing the player sees is a separate actor standing at the same spot.
	const FVector Here = GetActorLocation();
	AActor* Geometry = nullptr;
	float Best = MillGeometrySearchRadius;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Other = *It;
		if (!Other || Other == this)
		{
			continue;
		}
		UStaticMeshComponent* SMC = Other->FindComponentByClass<UStaticMeshComponent>();
		UStaticMesh* Mesh = SMC ? SMC->GetStaticMesh() : nullptr;
		if (!Mesh || !Mesh->GetName().Contains(MillGeometryNameFilter))
		{
			continue;
		}
		// HORIZONTAL distance, not 3D. A windmill is a vertical stack: its sails sit 4357uu ABOVE the
		// objective, so a 3D test measured them at 4361uu and threw them away as too far - which is
		// exactly what happened, logged as "found no sail actor within 2600uu" while the sails stood
		// there in plain view. What "belongs to this mill" means is footprint, not distance.
		const float Dist = FVector::Dist2D(Other->GetActorLocation(), Here);
		if (Dist <= Best)
		{
			Best = Dist;
			Geometry = Other;
		}
	}

	if (!Geometry)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] '%s' found no mill geometry matching '%s' within %.0fuu - it will detonate ")
			TEXT("without sinking."),
			*GetName(), *MillGeometryNameFilter, MillGeometrySearchRadius);
		return;
	}

	// The stump: spawned, revealed, and never released. It is simply the lower 27 pieces of the same
	// fracture, standing exactly where the tower was.
	AActor* Stump = UGSCrumbleComponent::SpawnProxyFor(Geometry, CrumbleCollectionFolder, StumpCollectionName);
	if (Stump)
	{
		if (UGeometryCollectionComponent* SGC = Stump->FindComponentByClass<UGeometryCollectionComponent>())
		{
			SGC->SetHiddenInGame(false);

			// THE STUMP MUST NOT BE BREAKABLE BY WHAT LANDS ON IT.
			//
			// Measured 2026-08-26: the stump spawned correctly as Chaos_Object_Static with no crumble
			// component, and 25 of its 27 pieces came apart anyway. Collections default to
			// bEnableDamageFromCollision with thresholds [500000, 50000, 5000], and a 100,000 kg tower
			// top landing on it clears those by a wide margin - so the falling half was demolishing
			// the standing half on impact.
			//
			// The stump is scenery from the moment it exists. It has already been destroyed once.
			SGC->SetEnableDamageFromCollision(false);
			// Charred like everything else that burned - the stump went through the same fire.
			const int32 Slots = SGC->GetNumMaterials();
			for (int32 i = 0; i < Slots; ++i)
			{
				if (UMaterialInstanceDynamic* MID = SGC->CreateDynamicMaterialInstance(i, nullptr))
				{
					MID->SetScalarParameterValue(FName("GS_BurnAmount"), 1.f);
				}
			}
		}
	}

	AActor* Proxy = UGSCrumbleComponent::SpawnProxyFor(Geometry, CrumbleCollectionFolder, TopCollectionName);
	if (!Proxy)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] '%s' has no %s collection - author one and the sink starts working with ")
			TEXT("no code change. Detonating without it."),
			*GetName(), *TopCollectionName.ToString());
		return;
	}

	// Retire the standing mill.
	for (UStaticMeshComponent* M : TInlineComponentArray<UStaticMeshComponent*>(Geometry))
	{
		if (M)
		{
			M->SetHiddenInGame(true);
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	if (UGSCrumbleComponent* Crumble = UGSCrumbleComponent::FindOrAdd(Proxy))
	{
		// NO anchoring. The top collection contains only the pieces that are meant to fall, so there
		// is nothing to hold back and nothing to get the ordering wrong about.
		Crumble->KeepAnchoredBelowFraction = 0.f;
		Crumble->ClusterCrumblePasses = 3;
		Crumble->CollapseShoveCount = 0;
		Crumble->CharAmountOnRelease = 1.f;
		// Straight down, applied at the cap's own centre so it drops rather than tips.
		FVector TopOrigin, TopExtent;
		Proxy->GetActorBounds(false, TopOrigin, TopExtent);
		Crumble->Crumble(FVector(0.f, 0.f, -TopDropImpulse), TopOrigin);
	}

	if (bDropSails)
	{
		DropSails();
	}

	// "and it all be on fire" - the wreck burns. Spawned around the base rather than attached to the
	// pieces: the pieces move, and a fire riding a tumbling chunk reads as a firework.
	if (UNiagaraSystem* Fire = WreckFireSystem.LoadSynchronous())
	{
		FVector Origin, Extent;
		Geometry->GetActorBounds(false, Origin, Extent);
		const float BaseZ = Origin.Z - Extent.Z;
		const float Radius = FMath::Min(Extent.X, Extent.Y) * 0.7f;
		for (int32 i = 0; i < WreckFireCount; ++i)
		{
			const float Angle = (2.f * PI * i) / FMath::Max(WreckFireCount, 1);
			const FVector At(Origin.X + FMath::Cos(Angle) * Radius,
							 Origin.Y + FMath::Sin(Angle) * Radius,
							 BaseZ + 150.f);
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Fire, At, FRotator::ZeroRotator,
														   FVector(3.f, 3.f, 3.f));
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("[GoblinSiege] '%s' sank: '%s' retired, %s standing, %s released, %d fire(s) on the wreck."),
		*GetName(), *Geometry->GetName(),
		Stump ? *StumpCollectionName.ToString() : TEXT("<no stump>"),
		*TopCollectionName.ToString(), WreckFireCount);
}

void AGSMillObjective::DropSails()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}

	const FVector Here = GetActorLocation();
	int32 Dropped = 0;

	for (const FString& Filter : LoosePartNameFilters)
	{
		// Nearest match per filter, measured HORIZONTALLY. A windmill is a vertical stack: its sails
		// sit 4357uu above the objective, so a 3D test measured them as too far away and threw them
		// out - logged as "found no sail actor" while they stood in plain view.
		AActor* Part = nullptr;
		float Best = MillGeometrySearchRadius;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Other = *It;
			if (!Other || Other == this)
			{
				continue;
			}
			UStaticMeshComponent* SMC = Other->FindComponentByClass<UStaticMeshComponent>();
			UStaticMesh* Mesh = SMC ? SMC->GetStaticMesh() : nullptr;
			if (!Mesh || !Mesh->GetName().Contains(Filter))
			{
				continue;
			}
			const float Dist = FVector::Dist2D(Other->GetActorLocation(), Here);
			if (Dist <= Best)
			{
				Best = Dist;
				Part = Other;
			}
		}

		if (!Part)
		{
			UE_LOG(LogTemp, Log,
				TEXT("[GoblinSiege] '%s' found no loose part matching '%s' within %.0fuu."),
				*GetName(), *Filter, MillGeometrySearchRadius);
			continue;
		}

		// Fractured, not simulated whole. The sails as a single body were 311 tonnes and 5354uu
		// across: they flew off the map, ploughed the wreck and shoved the tower. Broken into pieces
		// none of that is possible.
		AActor* Proxy = UGSCrumbleComponent::SpawnProxyFor(Part, CrumbleCollectionFolder);
		if (!Proxy)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[GoblinSiege] '%s' has no fracture for '%s' - leaving it standing rather than ")
				TEXT("dropping a single huge body."),
				*GetName(), *Part->GetName());
			continue;
		}

		for (UStaticMeshComponent* M : TInlineComponentArray<UStaticMeshComponent*>(Part))
		{
			if (M)
			{
				M->SetHiddenInGame(true);
				M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}

		if (UGSCrumbleComponent* Crumble = UGSCrumbleComponent::FindOrAdd(Proxy))
		{
			Crumble->KeepAnchoredBelowFraction = 0.f;
			Crumble->ClusterCrumblePasses = 3;
			Crumble->CollapseShoveCount = 0;
			Crumble->CharAmountOnRelease = 1.f;

			FVector Away = Part->GetActorLocation() - Here;
			Away.Z = 0.f;
			Away = Away.GetSafeNormal();
			if (Away.IsNearlyZero())
			{
				Away = GetActorForwardVector();
			}
			Crumble->Crumble(Away * SailDropImpulse, Part->GetActorLocation());
		}

		++Dropped;
		UE_LOG(LogTemp, Log, TEXT("[GoblinSiege] '%s' dropped loose part '%s' (filter '%s')."),
			*GetName(), *Part->GetName(), *Filter);
	}

	UE_LOG(LogTemp, Log, TEXT("[GoblinSiege] '%s' dropped %d of %d loose part(s)."),
		*GetName(), Dropped, LoosePartNameFilters.Num());
}
