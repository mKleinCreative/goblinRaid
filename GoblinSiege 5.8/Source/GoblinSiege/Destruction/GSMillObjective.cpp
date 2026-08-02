#include "Destruction/GSMillObjective.h"
#include "Destruction/GSFlammableComponent.h"
#include "Destruction/GSBurnFXComponent.h"
#include "Core/GSGameState.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
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
