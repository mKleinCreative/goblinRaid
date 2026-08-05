#include "Combat/GSAimComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

// The old arc WAS debug draw. This cvar keeps that around as a second opinion for when the shipping
// ribbon itself is the thing under suspicion - "is the ribbon wrong, or is the prediction wrong" is
// otherwise an unanswerable question. Default 0: the ribbon is the feature now.
//   GS.Aim.Debug 1  -> also draw the raw predicted path and impact point as debug lines
static int32 GSAimDebug = 0;
static FAutoConsoleVariableRef CVarGSAimDebug(
	TEXT("GS.Aim.Debug"),
	GSAimDebug,
	TEXT("0 = the shipping arc ribbon only. 1 = additionally draw the raw predicted path as debug "
		 "lines, for checking the ribbon against the prediction it is drawn from."),
	ECVF_Cheat);

UGSAimComponent::UGSAimComponent()
{
	// Ticks ONLY while aiming - enabled in BeginAim, disabled in EndAim. A component that predicted
	// a projectile path every frame of a raid to draw nothing would be a real cost for no reason.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SetIsReplicatedByDefault(true);
}

void UGSAimComponent::BeginPlay()
{
	Super::BeginPlay();

	// Deliberately does NOT build the arc visual here. A goblin who never aims never pays for a
	// spline mesh pool, and on a dedicated server nothing ever aims - EnsureArcVisual refuses
	// outright on any machine that does not locally control this pawn.
}

void UGSAimComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Nothing this component builds may outlive it. Same rule as UGSWeaponComponent::EndPlay: a
	// registered primitive left on a destroyed pawn is the kind of leak that only shows up after an
	// hour of respawns.
	for (USplineMeshComponent* Segment : ArcSegments)
	{
		if (Segment)
		{
			Segment->DestroyComponent();
		}
	}
	ArcSegments.Reset();

	if (LandingDecal)
	{
		LandingDecal->DestroyComponent();
		LandingDecal = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UGSAimComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// The MODE replicates so a remote client can play an aim pose (the "replicate cheap root state
	// only" pattern, decision Q-36). The predicted path does not and must not: it is a local
	// prediction that every client can compute for itself from the mode and the aim rotation.
	DOREPLIFETIME(UGSAimComponent, AimMode);
}

bool UGSAimComponent::IsLocallyControlledPawn() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled();
}

void UGSAimComponent::BeginAim(EGSAimMode Mode, TSubclassOf<AActor> ProjectileClass)
{
	if (Mode == EGSAimMode::None)
	{
		EndAim();
		return;
	}

	ArmedProjectileClass = ProjectileClass;

	// Re-arming the same mode is not a state change - the input handlers are bound to Started on
	// keys a player will mash, and re-broadcasting would make the camera restart its blend.
	if (AimMode == Mode)
	{
		return;
	}

	AimMode = Mode;
	SetComponentTickEnabled(true);
	OnAimStateChanged.Broadcast(true);
}

void UGSAimComponent::EndAim()
{
	if (AimMode == EGSAimMode::None)
	{
		return; // release with no matching press - Enhanced Input fires Canceled as well as Completed
	}

	AimMode = EGSAimMode::None;
	ArmedProjectileClass = nullptr;
	SetComponentTickEnabled(false);
	HideArcVisual();
	OnAimStateChanged.Broadcast(false);
}

void UGSAimComponent::OnRep_AimMode()
{
	// Remote proxies get the state change for pose purposes only; they never build an arc, which
	// EnsureArcVisual enforces independently of this path.
	OnAimStateChanged.Broadcast(IsAiming());
}

FRotator UGSAimComponent::GetAimRotation() const
{
	// See the header for the full argument. Short version: the server's control rotation for a
	// remote pawn has a byte-quantised pitch, and pitch is the whole game of a lobbed projectile.
	if (bHasReplicatedAimRotation && GetOwner() && GetOwner()->HasAuthority() && !IsLocallyControlledPawn())
	{
		return ReplicatedAimRotation;
	}

	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		return Pawn->GetControlRotation();
	}
	return GetOwner() ? GetOwner()->GetActorRotation() : FRotator::ZeroRotator;
}

void UGSAimComponent::PushAimRotationToServer()
{
	// Only a client driving its own pawn has anything to say. A listen-server host, a standalone
	// game and an AI pawn all already have the exact rotation on the authority.
	if (!IsLocallyControlledPawn() || !GetOwner() || GetOwner()->HasAuthority())
	{
		return;
	}

	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		Server_SetAimRotation(Pawn->GetControlRotation());
	}
}

bool UGSAimComponent::Server_SetAimRotation_Validate(FRotator NewAimRotation)
{
	// A rotation is not a position - there is no cheat to catch here, only garbage to reject.
	return !NewAimRotation.ContainsNaN();
}

void UGSAimComponent::Server_SetAimRotation_Implementation(FRotator NewAimRotation)
{
	ReplicatedAimRotation = NewAimRotation;
	bHasReplicatedAimRotation = true;
}

FTransform UGSAimComponent::GetMuzzleTransform() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FTransform::Identity;
	}

	const FRotator AimRotation = GetAimRotation();
	const FVector Location = Owner->GetActorLocation()
		+ AimRotation.Vector() * MuzzleForwardOffset
		+ FVector(0.f, 0.f, MuzzleHeightOffset);

	return FTransform(AimRotation, Location);
}

void UGSAimComponent::TickComponent(float DeltaSeconds, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);

	if (!IsAiming())
	{
		SetComponentTickEnabled(false); // belt and braces: nothing should tick us while not aiming
		return;
	}

	// A prediction is only ever consumed by the local player's own display. The server does not need
	// one (it spawns along GetAimRotation() and lets physics decide) and other clients cannot use one.
	if (IsLocallyControlledPawn())
	{
		UpdatePrediction();
	}
}

void UGSAimComponent::UpdatePrediction()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	// Read the flight parameters off the projectile's own CDO rather than duplicating them. Carried
	// over from AGSPlayerCharacter::DrawTorchAimArc, and it is why one component can serve both the
	// torch and the bow: a projectile describes its own flight, and the arc just believes it. If the
	// torch is ever retuned from 1400 the arc follows on the same frame.
	float Speed = 1400.f;
	float GravityScale = 1.f;
	if (ArmedProjectileClass)
	{
		if (const AActor* ProjectileCDO = ArmedProjectileClass->GetDefaultObject<AActor>())
		{
			if (const UProjectileMovementComponent* Move =
					ProjectileCDO->FindComponentByClass<UProjectileMovementComponent>())
			{
				Speed = Move->InitialSpeed > 0.f ? Move->InitialSpeed : Speed;
				GravityScale = Move->ProjectileGravityScale;
			}
		}
	}

	const FTransform Muzzle = GetMuzzleTransform();
	const FVector Start = Muzzle.GetLocation();
	const FVector LaunchVelocity = Muzzle.GetRotation().Vector() * Speed;

	FPredictProjectilePathParams Params(PredictionTraceRadius, Start, LaunchVelocity, MaxSimSeconds);
	Params.OverrideGravityZ = World->GetGravityZ() * GravityScale;
	Params.bTraceWithCollision = true;
	Params.bTraceComplex = false;
	Params.ActorsToIgnore.Add(Owner);
	Params.SimFrequency = SimFrequency;
	Params.DrawDebugType = GSAimDebug > 0 ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;
	Params.DrawDebugTime = 0.f;

	FPredictProjectilePathResult Result;
	const bool bHit = UGameplayStatics::PredictProjectilePath(this, Params, Result);

	ScratchPathPoints.Reset(Result.PathData.Num());
	for (const FPredictProjectilePathPointData& Point : Result.PathData)
	{
		ScratchPathPoints.Add(Point.Location);
	}

	const FVector ImpactPoint = bHit ? Result.HitResult.ImpactPoint
		: (ScratchPathPoints.Num() > 0 ? ScratchPathPoints.Last() : Start);
	const FVector ImpactNormal = bHit ? Result.HitResult.ImpactNormal : FVector::UpVector;

	UpdateArcVisual(ScratchPathPoints, bHit, ImpactPoint, ImpactNormal);
}

FLinearColor UGSAimComponent::GetArcColourForMode() const
{
	return AimMode == EGSAimMode::Bow ? BowArcColour : TorchArcColour;
}

bool UGSAimComponent::EnsureArcVisual()
{
	// The single hard rule of this whole visual: it exists on the machine that owns the input and
	// nowhere else. It is a local prediction, so a server-side or remote-client copy would be both
	// wasted work and a lie about someone else's aim.
	if (!IsLocallyControlledPawn())
	{
		return false;
	}

	if (bVisualBuilt)
	{
		return true;
	}
	if (bVisualResolveFailed)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	USceneComponent* Root = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Root)
	{
		return false;
	}

	UStaticMesh* Mesh = ArcSegmentMesh.LoadSynchronous();
	UMaterialInterface* SegmentMaterial = ArcMaterial.LoadSynchronous();
	if (!Mesh || !SegmentMaterial)
	{
		// Warn ONCE and never retry: LoadSynchronous does not cache a failure, and this runs on a
		// frame where the player is holding the aim key, so a retry is a failed package lookup per
		// frame for as long as they hold it.
		bVisualResolveFailed = true;
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] %s cannot build its aim arc: ArcSegmentMesh (%s) or ArcMaterial (%s) "
				 "did not resolve. Aiming and shooting still work - there is simply no visible arc. "
				 "Assign both on the character Blueprint's aim component."),
			*GetNameSafe(Owner), *ArcSegmentMesh.ToString(), *ArcMaterial.ToString());
		return false;
	}

	ResolvedArcMesh = Mesh;
	ArcMID = UMaterialInstanceDynamic::Create(SegmentMaterial, this);

	ArcSegments.Reserve(MaxArcSegments);
	for (int32 Index = 0; Index < MaxArcSegments; ++Index)
	{
		USplineMeshComponent* Segment = NewObject<USplineMeshComponent>(Owner,
			USplineMeshComponent::StaticClass(), NAME_None, RF_Transient);
		if (!Segment)
		{
			continue;
		}

		Segment->SetMobility(EComponentMobility::Movable);
		Segment->SetupAttachment(Root);
		Segment->SetStaticMesh(ResolvedArcMesh);
		if (ArcMID)
		{
			Segment->SetMaterial(0, ArcMID);
		}
		Segment->SetForwardAxis(ESplineMeshAxis::X, false);
		// The arc is decoration on a prediction: it must not collide with anything, must not cast a
		// shadow, and must not be visible to the goblin's own aim trace - a ribbon that shadowed the
		// ground it was predicting would be genuinely confusing.
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Segment->SetGenerateOverlapEvents(false);
		Segment->SetCastShadow(false);
		// Owner-only: this is one player's private aim line, and in split-screen or a listen-server
		// host's viewport an unowned copy would be somebody else's guess drawn on your screen.
		Segment->SetOnlyOwnerSee(true);
		Segment->SetVisibility(false);
		Segment->RegisterComponent();
		ArcSegments.Add(Segment);
	}

	if (UMaterialInterface* DecalMaterial = LandingDecalMaterial.LoadSynchronous())
	{
		LandingDecal = NewObject<UDecalComponent>(Owner, UDecalComponent::StaticClass(), NAME_None, RF_Transient);
		if (LandingDecal)
		{
			LandingMID = UMaterialInstanceDynamic::Create(DecalMaterial, this);
			LandingDecal->SetMobility(EComponentMobility::Movable);
			LandingDecal->SetupAttachment(Root);
			// ABSOLUTE transform, which is the whole reason this line exists: the decal marks a spot
			// in the WORLD, and a decal that kept a relative transform to the goblin's root would
			// slide across the ground as he walked - the marker would follow the player instead of
			// staying on the ground he aimed at.
			LandingDecal->SetAbsolute(true, true, true);
			LandingDecal->SetDecalMaterial(LandingMID ? static_cast<UMaterialInterface*>(LandingMID) : DecalMaterial);
			LandingDecal->DecalSize = LandingDecalSize;
			// No SetOnlyOwnerSee here: UDecalComponent is a USceneComponent, not a UPrimitiveComponent,
			// so it has no owner-visibility filter. In split-screen both local players would see both
			// landing rings. Acceptable for a single-player slice; the fix if it ever matters is a
			// per-player decal render-target channel, not a flag.
			LandingDecal->SetVisibility(false);
			LandingDecal->RegisterComponent();
		}
	}
	else
	{
		// Not fatal and not latched with the mesh failure: the ribbon alone is still a usable
		// indicator, and the decal is the more likely of the two to be left unassigned early on.
		UE_LOG(LogTemp, Log,
			TEXT("[GoblinSiege] %s has no LandingDecalMaterial - the aim arc will draw without a "
				 "landing marker."),
			*GetNameSafe(Owner));
	}

	bVisualBuilt = true;
	return true;
}

void UGSAimComponent::UpdateArcVisual(const TArray<FVector>& Path, bool bHit,
	const FVector& ImpactPoint, const FVector& ImpactNormal)
{
	if (!EnsureArcVisual())
	{
		return;
	}

	const FLinearColor Colour = GetArcColourForMode();
	if (ArcMID)
	{
		ArcMID->SetVectorParameterValue(ArcColourParameterName, Colour);
	}
	if (LandingMID)
	{
		LandingMID->SetVectorParameterValue(ArcColourParameterName, Colour);
	}

	// One segment per pair of path points, capped by the pool. Running out of pool shortens the
	// drawn arc rather than costing frames, and MaxArcSegments is the knob for that trade.
	const int32 SegmentCount = FMath::Min(Path.Num() - 1, ArcSegments.Num());

	for (int32 Index = 0; Index < ArcSegments.Num(); ++Index)
	{
		USplineMeshComponent* Segment = ArcSegments[Index];
		if (!Segment)
		{
			continue;
		}

		if (Index >= SegmentCount)
		{
			// Hidden, not destroyed - the pool is built once and lives as long as the component.
			Segment->SetVisibility(false);
			continue;
		}

		// Spline meshes want positions in the component's own space, and every segment is parented
		// to the pawn's root, so the world-space path has to come back into local space or the whole
		// ribbon travels with the goblin.
		const FTransform ToLocal = Segment->GetComponentTransform().Inverse();
		const FVector LocalStart = ToLocal.TransformPosition(Path[Index]);
		const FVector LocalEnd = ToLocal.TransformPosition(Path[Index + 1]);
		const FVector Tangent = LocalEnd - LocalStart;

		Segment->SetStartAndEnd(LocalStart, Tangent, LocalEnd, Tangent, /*bUpdateMesh*/ true);
		Segment->SetStartScale(FVector2D(ArcSegmentWidth, ArcSegmentWidth), false);
		Segment->SetEndScale(FVector2D(ArcSegmentWidth, ArcSegmentWidth), true);
		Segment->SetVisibility(true);
	}

	if (LandingDecal)
	{
		if (bHit)
		{
			// Orient the decal INTO the surface it marks. A decal projects along its own -X, so the
			// rotation has to come from the impact normal; this is what makes the ring lie flat on a
			// hillside instead of hovering over it, which is the specific failure of the
			// DrawDebugCircle this replaced.
			LandingDecal->SetWorldLocation(ImpactPoint);
			LandingDecal->SetWorldRotation((-ImpactNormal).Rotation());
			LandingDecal->SetVisibility(true);
		}
		else
		{
			// The shot lands on nothing within the simulated window. Hiding the marker says that
			// honestly; leaving it at the last path point would claim a landing spot that is really
			// just where we stopped simulating.
			LandingDecal->SetVisibility(false);
		}
	}

	if (GSAimDebug > 0 && bHit)
	{
		if (UWorld* World = GetWorld())
		{
			DrawDebugSphere(World, ImpactPoint, 20.f, 12, Colour.ToFColor(true), false, -1.f, 0, 2.f);
		}
	}
}

void UGSAimComponent::HideArcVisual()
{
	for (USplineMeshComponent* Segment : ArcSegments)
	{
		if (Segment)
		{
			Segment->SetVisibility(false);
		}
	}
	if (LandingDecal)
	{
		LandingDecal->SetVisibility(false);
	}
}
