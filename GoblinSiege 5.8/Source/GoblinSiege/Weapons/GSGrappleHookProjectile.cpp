#include "Weapons/GSGrappleHookProjectile.h"
#include "Weapons/GSGrappleHaulComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSGrappleHook, Log, All);

// Warn-once latches, file-scope static rather than members - a hook is spawned fresh per throw
// (AGSTorchProjectile's own comment explains why a per-instance latch would be pointless here:
// each new hook starts false, retries the load, and warns again on every throw of a broken build).
static bool GGrappleHookMeshResolveFailed = false;
static bool GGrappleRopeMeshResolveFailed = false;
static bool GGrappleRopeMaterialResolveFailed = false;

AGSGrappleHookProjectile::AGSGrappleHookProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // only while attached, updating the rope
	bReplicates = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(10.f);
	CollisionSphere->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionSphere->SetNotifyRigidBodyCollision(true);
	RootComponent = CollisionSphere;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	// Matches the tuned values UGSGrappleHaulComponent::MaxGrappleDistanceUU's header cites as the
	// source of the observed 760-1561uu attach-distance spread - unchanged here, this ability's
	// throw physics were never the bug.
	ProjectileMovement->InitialSpeed = 2600.f;
	ProjectileMovement->MaxSpeed = 2600.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = 0.45f;

	HookMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookMesh"));
	HookMesh->SetupAttachment(RootComponent);
	HookMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HookMesh->SetGenerateOverlapEvents(false);

	RopeSegments = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RopeSegments"));
	RopeSegments->SetupAttachment(RootComponent);
	RopeSegments->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RopeSegments->SetGenerateOverlapEvents(false);
	RopeSegments->SetMobility(EComponentMobility::Movable);

	// C++ defaults, same reasoning as AGSTorchProjectile::FireVolumeClass: a soft reference assigned
	// only in a content folder is one bad merge away from being null again, and null here degrades
	// silently (an invisible rope, not an error). This is the one dedicated rope asset already in
	// the project (Content/Props/Rope), used by nothing else that would conflict with reassigning it.
	RopeSegmentMeshAsset = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Game/Props/Rope/SM_Rope_Segment01.SM_Rope_Segment01")));
	RopeMaterialAsset = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/Props/Rope/MI_GS_Rope.MI_GS_Rope")));
}

void AGSGrappleHookProjectile::BeginPlay()
{
	Super::BeginPlay();
	CollisionSphere->OnComponentHit.AddDynamic(this, &AGSGrappleHookProjectile::HandleHit);

	// Never collide with the thrower, same fix and same reasoning as AGSTorchProjectile::BeginPlay -
	// BlockAllDynamic blocks the Pawn channel, so without this the hook welds itself to whoever threw
	// it on the very first tick.
	ThrowingOwner = GetInstigator();
	if (!ThrowingOwner.IsValid())
	{
		ThrowingOwner = GetOwner();
	}
	if (AActor* Thrower = ThrowingOwner.Get())
	{
		CollisionSphere->IgnoreActorWhenMoving(Thrower, true);
	}

	if (!HookMeshAsset.IsNull() && !GGrappleHookMeshResolveFailed && HookMesh)
	{
		if (UStaticMesh* Mesh = HookMeshAsset.LoadSynchronous())
		{
			HookMesh->SetStaticMesh(Mesh);
		}
		else
		{
			GGrappleHookMeshResolveFailed = true;
			UE_LOG(LogGSGrappleHook, Warning,
				TEXT("[GS.Grapple] HookMeshAsset failed to resolve - the hook will be invisible in "
					 "flight. It still throws, sticks and hauls."));
		}
	}

	UStaticMesh* RopeMesh = nullptr;
	if (!RopeSegmentMeshAsset.IsNull() && !GGrappleRopeMeshResolveFailed)
	{
		RopeMesh = RopeSegmentMeshAsset.LoadSynchronous();
		if (RopeMesh)
		{
			RopeSegments->SetStaticMesh(RopeMesh);
		}
		else
		{
			GGrappleRopeMeshResolveFailed = true;
			UE_LOG(LogGSGrappleHook, Warning,
				TEXT("[GS.Grapple] RopeSegmentMeshAsset failed to resolve - the rope will be invisible. "
					 "The haul itself is unaffected."));
		}
	}

	if (RopeMesh && !RopeMaterialAsset.IsNull() && !GGrappleRopeMaterialResolveFailed)
	{
		if (UMaterialInterface* Material = RopeMaterialAsset.LoadSynchronous())
		{
			RopeSegments->SetMaterial(0, Material);
		}
		else
		{
			GGrappleRopeMaterialResolveFailed = true;
			UE_LOG(LogGSGrappleHook, Warning,
				TEXT("[GS.Grapple] RopeMaterialAsset failed to resolve - the rope will use its mesh's "
					 "default material."));
		}
	}
}

void AGSGrappleHookProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bAttached)
	{
		return;
	}

	AActor* Thrower = ThrowingOwner.Get();
	if (!Thrower)
	{
		return;
	}

	const FVector RawRopeFrom = Thrower->GetActorLocation() + FVector(0.f, 0.f, RopeSourceHeightOffsetUU);
	if (!bRopeSourceSeeded)
	{
		SmoothedRopeSource = RawRopeFrom;
		bRopeSourceSeeded = true;
	}
	else
	{
		SmoothedRopeSource = FMath::VInterpTo(SmoothedRopeSource, RawRopeFrom, DeltaTime, RopeSourceSmoothingSpeed);
	}

	RebuildRopeSegments(SmoothedRopeSource, AttachedAnchorPoint);
}

FVector AGSGrappleHookProjectile::FindVisualAnchorPoint(const FHitResult& Hit) const
{
	if (!Hit.GetActor())
	{
		return Hit.ImpactPoint;
	}

	FVector TraceDirection = GetVelocity().GetSafeNormal();
	if (TraceDirection.IsNearlyZero())
	{
		TraceDirection = -Hit.ImpactNormal;
	}

	// Short trace straddling the reported impact, complex collision so it finds the RENDERED
	// surface rather than the simplified hull that produced Hit.ImpactPoint in the first place.
	const FVector TraceStart = Hit.ImpactPoint - TraceDirection * 100.f;
	const FVector TraceEnd = Hit.ImpactPoint + TraceDirection * 100.f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSGrappleVisualAnchor), /*bTraceComplex*/ true);
	Params.AddIgnoredActor(this);

	FHitResult ComplexHit;
	if (GetWorld()->LineTraceSingleByChannel(ComplexHit, TraceStart, TraceEnd, ECC_Visibility, Params)
		&& ComplexHit.GetActor() == Hit.GetActor())
	{
		return ComplexHit.ImpactPoint;
	}

	// No complex hull to find (simple-collision-only geometry) - the original impact point is the
	// best answer available, not a failure.
	return Hit.ImpactPoint;
}

void AGSGrappleHookProjectile::RebuildRopeSegments(const FVector& From, const FVector& To)
{
	if (!RopeSegments)
	{
		return;
	}

	const FVector Delta = To - From;
	const float Distance = Delta.Size();
	if (Distance < KINDA_SMALL_NUMBER || RopeSegmentLengthUU <= 0.f)
	{
		if (RopeSegments->GetInstanceCount() > 0)
		{
			RopeSegments->ClearInstances();
		}
		return;
	}

	const FVector Direction = Delta / Distance;
	// SM_Rope_Segment01's real bounding box, read back live via VibeUE (2026-08-31): 7.32 x 7.32 x
	// 50.0 - its long axis is local Z, not X. Direction.Rotation() orients local X toward Direction,
	// which was the actual bug behind "a row of sticks": each instance's real 50uu-long axis pointed
	// straight up (world Z) instead of along the rope, while the 7.32uu-short axis pointed along the
	// rope - a field of little vertical pegs spaced along the line, exactly what the screenshot
	// showed. FindBetweenNormals(UpVector, Direction) instead rotates local Z onto Direction, and the
	// scale below moves to match (Z = along-rope, X/Y = thickness).
	const FRotator Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Direction).Rotator();
	const int32 NumSegments = FMath::Max(1, FMath::RoundToInt(Distance / RopeSegmentLengthUU));
	const float ActualSegmentLength = Distance / NumSegments;
	// Overlapped rather than exactly abutting - see RopeSegmentOverlapFactor's comment. The overlap
	// only stretches each segment's own mesh; segment CENTERS stay evenly spaced at ActualSegmentLength,
	// so this cannot shorten or lengthen the rope's total reach.
	const float ScaleAlongRope = (ActualSegmentLength / RopeSegmentLengthUU) * RopeSegmentOverlapFactor;

	if (RopeSegments->GetInstanceCount() != NumSegments)
	{
		RopeSegments->ClearInstances();
		for (int32 Index = 0; Index < NumSegments; ++Index)
		{
			RopeSegments->AddInstance(FTransform::Identity);
		}
	}

	for (int32 Index = 0; Index < NumSegments; ++Index)
	{
		// Pivot is at one END of the mesh, not centered - also read back live via VibeUE (2026-08-31):
		// the bounding box runs Z 0..50, not -25..25. A centered-pivot placement (Index + 0.5f) was
		// the second live bug in a row on this function - it put each instance's pivot at its
		// interval's MIDPOINT, so scaling stretched the mesh from that midpoint and left the other
		// half of the interval empty, which read as visible gaps between segments (a playtest
		// screenshot showed four short bars hanging apart, not one rope). Placing the pivot at the
		// interval's START instead means the scaled mesh fills exactly [Index, Index+1) with nothing
		// left over.
		const FVector SegmentLocation = From + Direction * (ActualSegmentLength * Index);
		const FTransform SegmentTransform(Rotation, SegmentLocation,
			FVector(RopeThicknessScale, RopeThicknessScale, ScaleAlongRope));

		RopeSegments->UpdateInstanceTransform(Index, SegmentTransform, /*bWorldSpace*/ true,
			/*bMarkRenderStateDirty*/ Index == NumSegments - 1);
	}
}

void AGSGrappleHookProjectile::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bAttached)
	{
		return;
	}

	AActor* Thrower = ThrowingOwner.Get();

	// Belt and braces against the same sweep-vs-move-ignore gap AGSTorchProjectile::OnProjectileHit
	// documents: IgnoreActorWhenMoving only suppresses hits generated by THIS actor's own movement,
	// not the thrower's capsule sweeping into an already-flying hook.
	if (OtherActor && (OtherActor == Thrower || OtherActor == GetInstigator()))
	{
		return;
	}

	bAttached = true;

	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->ProjectileGravityScale = 0.f;
	SetActorEnableCollision(false);

	const FVector AnchorPoint = FindVisualAnchorPoint(Hit);
	AttachedAnchorPoint = AnchorPoint;

	if (OtherComp)
	{
		AttachToComponent(OtherComp, FAttachmentTransformRules::KeepWorldTransform);
	}

	SetActorTickEnabled(true);

	UE_LOG(LogGSGrappleHook, Log,
		TEXT("[GS.Grapple] Hook stuck: actor='%s' component='%s' anchor=(%.0f, %.0f, %.0f)"),
		OtherActor ? *OtherActor->GetName() : TEXT("<none>"),
		OtherComp ? *OtherComp->GetName() : TEXT("<none>"),
		AnchorPoint.X, AnchorPoint.Y, AnchorPoint.Z);

	if (HasAuthority() && Thrower)
	{
		if (UGSGrappleHaulComponent* Haul = Thrower->FindComponentByClass<UGSGrappleHaulComponent>())
		{
			Haul->NotifyHookAttached(this, OtherActor, AnchorPoint);
		}
		else
		{
			UE_LOG(LogGSGrappleHook, Warning,
				TEXT("[GS.Grapple] '%s' threw a hook but has no UGSGrappleHaulComponent - it will "
					 "stick, but nothing can ever haul on it."),
				*Thrower->GetName());
		}
	}
}
