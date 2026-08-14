#include "Horde/GSHordeOrderMarker.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSHordeOrder, Log, All);

AGSHordeOrderMarker::AGSHordeOrderMarker()
{
	// Nothing here moves, senses or decides. A tick would be pure cost.
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	// The beacon never moves after it is planted, so movement replication would spend bandwidth
	// re-sending a transform that cannot change. Same call, same reason, as AGSRunicSite.
	SetReplicateMovement(false);

	// A marker is meaningful precisely when it is far away and out of sight - "where did I send
	// them" is a question you ask from across the hamlet. Standard distance-based relevancy would
	// cull the one case the feature exists for. There is at most one of these per player.
	bAlwaysRelevant = true;

	MarkerRoot = CreateDefaultSubobject<USceneComponent>(TEXT("MarkerRoot"));
	SetRootComponent(MarkerRoot);

	BeaconMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeaconMesh"));
	BeaconMesh->SetupAttachment(MarkerRoot);

	// A marker you can walk into, shoot, or that blocks a goblin's path would be a bug wearing the
	// costume of a feature. It is a diegetic HUD element, not scenery.
	BeaconMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeaconMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	BeaconMesh->SetGenerateOverlapEvents(false);
	BeaconMesh->SetCanEverAffectNavigation(false);
	BeaconMesh->bCastDynamicShadow = false;

	// The colour-capable material, as a C++ default. See the header for why this cannot live on the
	// Blueprint: a native component's material override does not survive a recompile, and #141's
	// Blueprint-side assignment silently reverted to the engine default material, which has no
	// MarkerColour parameter - so every verb's beacon drew grey.
	BeaconMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/Blueprints/M_HordeOrderMarker.M_HordeOrderMarker")));
}

void AGSHordeOrderMarker::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGSHordeOrderMarker, Verb);
	DOREPLIFETIME(AGSHordeOrderMarker, Subject);
	DOREPLIFETIME(AGSHordeOrderMarker, Issuer);
}

void AGSHordeOrderMarker::BeginPlay()
{
	Super::BeginPlay();

	// A client that receives this actor in the same bunch as its Verb has already had OnRep_Verb
	// fire before BeginPlay; one that receives it with the default runs this and gets the right
	// colour on the next update. Painting from both ends is free and covers the ordering either way.
	ApplyVisuals();
}

void AGSHordeOrderMarker::InitialiseOrder(EGSHordeOrder InVerb, AActor* InSubject, AController* InIssuer)
{
	if (!HasAuthority())
	{
		// Not an assert: a Blueprint could reasonably wire this up on the wrong side, and refusing
		// loudly beats writing state the next replication update silently reverts.
		UE_LOG(LogGSHordeOrder, Warning,
			TEXT("InitialiseOrder called on a non-authoritative %s - ignored. Orders are issued "
			     "server-side through UGSHordeSubsystem::IssueOrder."), *GetName());
		return;
	}

	Verb = InVerb;
	Subject = InSubject;
	Issuer = InIssuer;

	// OnRep does not fire on the authority, so paint directly. On a listen server this is the only
	// thing that makes the host see his own beacon.
	ApplyVisuals();
}

void AGSHordeOrderMarker::Retire()
{
	if (!HasAuthority())
	{
		return;
	}
	Destroy();
}

void AGSHordeOrderMarker::OnRep_Verb()
{
	ApplyVisuals();
}

FLinearColor AGSHordeOrderMarker::ColourForOrder(EGSHordeOrder InVerb)
{
	// Read at a glance from across a field, which rules out subtlety. Follow is included even though
	// a Follow order retires the marker rather than planting one - the wheel still has to draw that
	// sector, and this is the one table both halves read.
	switch (InVerb)
	{
	case EGSHordeOrder::Attack: return FLinearColor(1.f, 0.16f, 0.12f, 1.f);   // blood red
	case EGSHordeOrder::Hold:   return FLinearColor(0.25f, 0.55f, 1.f, 1.f);   // cold blue
	case EGSHordeOrder::Loot:   return FLinearColor(1.f, 0.78f, 0.18f, 1.f);   // gold
	case EGSHordeOrder::Follow: return FLinearColor(0.4f, 0.95f, 0.4f, 1.f);   // green
	default:                    return FLinearColor(1.f, 1.f, 1.f, 0.5f);      // None - dimmed white
	}
}

void AGSHordeOrderMarker::ApplyVisuals()
{
	if (!BeaconMesh)
	{
		return;
	}

	// An unset mesh is the expected state until BP_HordeOrderMarker is authored, and it is not worth
	// a warning on every spawn - the order still works, and GS.Horde.Status reports the verb. Silence
	// here, loudly diagnosable there.
	if (!BeaconMesh->GetStaticMesh())
	{
		return;
	}

	// Apply the colour-capable material BEFORE the MID is made, or the MID is built from whatever the
	// mesh shipped with (the engine default, which has no MarkerColour parameter) and every verb
	// draws the same grey.
	if (!BeaconMID)
	{
		if (UMaterialInterface* Resolved = BeaconMaterial.LoadSynchronous())
		{
			BeaconMesh->SetMaterial(0, Resolved);
		}
		BeaconMID = BeaconMesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (!BeaconMID)
	{
		return;
	}

	// SetVectorParameterValue against a material that has no such parameter is a SILENT no-op, and
	// there is no cheap, version-stable "does this parameter exist" query to guard it with. So the
	// failure mode is deliberately chosen rather than detected: a beacon whose material lacks
	// MarkerColour draws in whatever colour it was authored in, for every verb. If every order plants
	// the same colour marker, that is this line finding nothing to write - check M_HordeOrderMarker
	// for the parameter name before looking anywhere else.
	BeaconMID->SetVectorParameterValue(ColourParameterName, ColourForOrder(Verb));
}
