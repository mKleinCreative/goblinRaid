#include "Raid/GSWarrenPlacementComponent.h"

#include "Raid/GSWarren.h"
#include "Destruction/GSTopplableComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"   // TActorIterator
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSWarrenPlacement, Log, All);

UGSWarrenPlacementComponent::UGSWarrenPlacementComponent()
{
	// Ticks ONLY while the ghost is up. A component that tracked a spot nobody is looking at would
	// be two traces and an overlap test per frame, per player, forever.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Defaulted here so the feature works the moment the component is dropped on a Blueprint. All
	// three are EditDefaultsOnly and soft, so a designer overrides any of them without touching C++
	// and none of them cost a package load at module time.
	WarrenClass = TSoftClassPtr<AGSWarren>(
		FSoftObjectPath(TEXT("/Game/Blueprints/BP_GS_Warren.BP_GS_Warren_C")));
	GhostMaterialValid = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/VFX/Warren/MI_GS_WarrenGhost_Valid.MI_GS_WarrenGhost_Valid")));
	GhostMaterialInvalid = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/VFX/Warren/MI_GS_WarrenGhost_Invalid.MI_GS_WarrenGhost_Invalid")));

	// PLACEHOLDER, and deliberately an obvious one. The Warren has no mesh at all - its whole look
	// is the N_ChaosRune2 Niagara system - so there is nothing authored to preview. An engine
	// cylinder standing where the gate will open is honest about being a stand-in in a way that a
	// half-right custom mesh would not be. Replace it when the mouth gets real art.
	GhostMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
}

void UGSWarrenPlacementComponent::BeginPlay()
{
	Super::BeginPlay();

	// Said ONCE, at BeginPlay, rather than on the first press. A placement key that does nothing is
	// indistinguishable from an unbound key, and the player finds out by pressing T in the middle of
	// a raid and getting silence.
	if (WarrenClass.IsNull())
	{
		UE_LOG(LogGSWarrenPlacement, Warning,
			TEXT("[GoblinSiege] %s has a Warren placement component with no WarrenClass set. Holding "
				 "the plant key will raise a ghost and plant NOTHING. Set it on the owning Blueprint."),
			*GetNameSafe(GetOwner()));
	}
	if (GhostMesh.IsNull())
	{
		UE_LOG(LogGSWarrenPlacement, Warning,
			TEXT("[GoblinSiege] %s: Warren placement has no GhostMesh. Placement will still work and "
				 "still report green/red, but the player cannot SEE where the gate will land."),
			*GetNameSafe(GetOwner()));
	}
}

void UGSWarrenPlacementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Not merely tidy: the ghost is a registered component on the owner, and leaving it visible
	// through a possession change or a death would strand a translucent gate in the world.
	CancelPlacement();
	Super::EndPlay(EndPlayReason);
}

// ====================================================================== the channel

void UGSWarrenPlacementComponent::BeginPlacement()
{
	if (bPlacing)
	{
		// No-op rather than restart. Enhanced Input can repeat Started, and a preview that flickered
		// on key repeat would read as a bug.
		return;
	}

	bPlacing = true;
	RefreshWards();
	SetComponentTickEnabled(true);
	EnsureGhost();
	ShowGhost(true);

	// Evaluate immediately so the first frame is already the right colour, rather than green for a
	// frame and then red.
	FVector Loc; FRotator Rot;
	EvaluateSpot(Loc, Rot);
	OnPlacementChanged.Broadcast(true, BlockReason);
}

void UGSWarrenPlacementComponent::CancelPlacement()
{
	if (!bPlacing)
	{
		return;
	}
	bPlacing = false;
	SetComponentTickEnabled(false);
	ShowGhost(false);
	OnPlacementChanged.Broadcast(false, BlockReason);
}

AGSWarren* UGSWarrenPlacementComponent::ConfirmPlacement()
{
	if (!bPlacing)
	{
		return nullptr;
	}

	FVector Location; FRotator Rotation;
	const bool bValid = EvaluateSpot(Location, Rotation);

	// Lower the ghost FIRST, whatever happens next. If the spawn below fails we must not leave a
	// translucent gate hanging in the world with no way to dismiss it.
	CancelPlacement();

	if (!bValid)
	{
		UE_LOG(LogGSWarrenPlacement, Verbose,
			TEXT("[GoblinSiege] %s released the plant key on a blocked spot (%d). Nothing planted."),
			*GetNameSafe(GetOwner()), static_cast<int32>(BlockReason));
		return nullptr;
	}

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return nullptr;
	}

	// The spawn debits nothing and is cosmetic on a client, but the Warren is a gameplay actor that
	// the horde and the score subsystem both read, so only the server may make one.
	if (!Owner->HasAuthority())
	{
		return nullptr;
	}

	UClass* Class = WarrenClass.LoadSynchronous();
	if (!Class)
	{
		UE_LOG(LogGSWarrenPlacement, Error,
			TEXT("[GoblinSiege] %s tried to plant a Warren but WarrenClass is unset or failed to "
				 "load. The ghost was green and nothing appeared, which is the worst way for this "
				 "to fail."), *GetNameSafe(Owner));
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGSWarren* NewWarren = World->SpawnActor<AGSWarren>(Class, Location, Rotation, Params);
	if (!NewWarren)
	{
		UE_LOG(LogGSWarrenPlacement, Error,
			TEXT("[GoblinSiege] %s: SpawnActor returned null for the Warren at (%.0f, %.0f, %.0f)."),
			*GetNameSafe(Owner), Location.X, Location.Y, Location.Z);
		return nullptr;
	}

	// ONE PER PLAYER. The old one collapses where it stands - see the header for why this beats
	// refusing until the cooldown expires. Done AFTER the new one exists, so a failed spawn cannot
	// leave the player with none.
	if (AGSWarren* Previous = PlantedWarren.Get())
	{
		UE_LOG(LogGSWarrenPlacement, Log,
			TEXT("[GoblinSiege] %s planted a new Warren; the previous one at (%.0f, %.0f, %.0f) "
				 "collapses."), *GetNameSafe(Owner),
			Previous->GetActorLocation().X, Previous->GetActorLocation().Y, Previous->GetActorLocation().Z);
		Previous->Destroy();
	}

	PlantedWarren = NewWarren;
	LastPlantedTime = World->GetTimeSeconds();

	UE_LOG(LogGSWarrenPlacement, Log,
		TEXT("[GoblinSiege] %s planted a Warren at (%.0f, %.0f, %.0f). Next one in %.0fs."),
		*GetNameSafe(Owner), Location.X, Location.Y, Location.Z, PlacementCooldownSeconds);

	OnWarrenPlanted.Broadcast(NewWarren);
	return NewWarren;
}

void UGSWarrenPlacementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bPlacing)
	{
		// Belt and braces: something could enable the tick without going through BeginPlacement.
		SetComponentTickEnabled(false);
		return;
	}

	FVector Loc; FRotator Rot;
	const bool bValid = EvaluateSpot(Loc, Rot);

	if (GhostComponent)
	{
		// GhostVisualLift is applied HERE and nowhere else. Loc stays the real, traced ground point,
		// so the Warren plants where the ghost appears to stand rather than hovering above it.
		GhostComponent->SetWorldLocationAndRotation(Loc + FVector(0.f, 0.f, GhostVisualLift), Rot);
	}
	ApplyGhostMaterial(bValid);
}

// ====================================================================== the test

float UGSWarrenPlacementComponent::GetCooldownRemaining() const
{
	if (LastPlantedTime < 0.0)
	{
		return 0.f;
	}
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}
	const double Elapsed = World->GetTimeSeconds() - LastPlantedTime;
	return static_cast<float>(FMath::Max(0.0, PlacementCooldownSeconds - Elapsed));
}

bool UGSWarrenPlacementComponent::EvaluateSpot(FVector& OutLocation, FRotator& OutRotation)
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		SetBlockReason(EGSWarrenPlacementBlock::NoGround);
		return false;
	}

	// A FIXED distance along the owner's facing, flattened. Not the camera's aim - see the header.
	FVector Forward = Owner->GetActorForwardVector();
	Forward.Z = 0.f;
	if (!Forward.Normalize())
	{
		Forward = FVector::ForwardVector;
	}
	const FVector Base = Owner->GetActorLocation() + Forward * PlacementDistance;

	// Always face the gate back toward the player, so goblins climb out looking at them rather than
	// at whatever the player happened to be facing when they planted it.
	OutRotation = FRotator(0.f, (-Forward).Rotation().Yaw, 0.f);

	FCollisionQueryParams Query(SCENE_QUERY_STAT(GSWarrenPlacement), false, Owner);
	Query.AddIgnoredActor(Owner);

	// EXISTING WARRENS ARE NOT OBSTRUCTIONS. Their banking sphere is 500uu, QueryOnly, and
	// deliberately overlaps everything - so a 120uu clearance test reads a Warren as solid from
	// 620uu away, and the ghost goes red across a huge circle around every gate already planted.
	// Measured on the arena: the clearance test hit Warren_Arena_01 from 550uu.
	for (TActorIterator<AGSWarren> It(World); It; ++It)
	{
		Query.AddIgnoredActor(*It);
	}
	if (GhostComponent)
	{
		// .Get() is load-bearing: AddIgnoredComponent overloads on both a raw pointer and a
		// TWeakObjectPtr, and a TObjectPtr converts implicitly to EITHER, so passing it bare is
		// ambiguous. Resolving to the raw pointer picks the intended overload.
		Query.AddIgnoredComponent(GhostComponent.Get());
	}

	FHitResult Hit;
	const FVector Start = Base + FVector(0.f, 0.f, GroundTraceUp);
	const FVector End   = Base - FVector(0.f, 0.f, GroundTraceDown);
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Query))
	{
		// Nothing under the spot at all - a ledge, a gap, the void. Park the ghost at the trace
		// midpoint so it stays visible and obviously wrong rather than vanishing.
		OutLocation = Base;
		SetBlockReason(EGSWarrenPlacementBlock::NoGround);
		return false;
	}

	OutLocation = Hit.ImpactPoint;

	// Cooldown is checked here rather than at the key press so the ghost turns red and STAYS red
	// while the timer runs - the player can see the rule instead of pressing a dead key.
	if (GetCooldownRemaining() > 0.f)
	{
		SetBlockReason(EGSWarrenPlacementBlock::OnCooldown);
		return false;
	}

	// THE STATUE WARDS THIS GROUND. Michael's rule: while the monument stands you cannot summon a
	// gate inside the ground it protects - casting it down is the price of opening a portal here.
	//
	// Checked alongside the cooldown rather than with the ground tests, because it is the same KIND
	// of refusal: the spot is fine, the world says no. The ghost turns red and stays red while the
	// statue stands, so the player reads the rule off the world instead of pressing a dead key.
	if (const UGSTopplableComponent* Ward = FindWardBlocking(OutLocation))
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[GoblinSiege] Warren placement refused - '%s' still stands and seals %s."),
			*GetNameSafe(Ward->GetOwner()),
			Ward->bWardsEntireLevel ? TEXT("the whole land")
									: *FString::Printf(TEXT("%.0f uu around it"), Ward->WardRadius));
		SetBlockReason(EGSWarrenPlacementBlock::WardedByStatue);
		return false;
	}

	const float SlopeDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
		static_cast<float>(FVector::DotProduct(Hit.ImpactNormal, FVector::UpVector)), -1.f, 1.f)));
	if (SlopeDegrees > MaxGroundSlopeDegrees)
	{
		SetBlockReason(EGSWarrenPlacementBlock::GroundTooSteep);
		return false;
	}

	// Room for the mouth. Sphere rather than the Warren's own capsule because the Warren has no
	// collision primitive worth testing - its banking sphere is QueryOnly and deliberately overlaps
	// everything, so testing against that would call every spot blocked.
	//
	// THREE channels, not one. Static alone would let you plant a gate inside a dropped crate or on
	// top of another goblin, both of which are "the mouth is blocked" as far as anything climbing
	// out of it is concerned. The owner is already in the ignore list, so standing near your own
	// planting spot does not turn the ghost red.
	// LIFTED CLEAR OF THE FLOOR, and this was a real bug: centring the sphere exactly
	// ClearanceRadius above the ground puts its bottom face ON the ground, so the floor the gate is
	// standing on counts as an obstruction and every spot in a flat arena reads red. The lift has to
	// exceed the radius, not equal it.
	const FVector Centre = OutLocation + FVector(0.f, 0.f, ClearanceRadius + ClearanceGroundLift);
	const FCollisionShape Mouth = FCollisionShape::MakeSphere(ClearanceRadius);
	static const ECollisionChannel BlockingChannels[] = { ECC_WorldStatic, ECC_WorldDynamic, ECC_Pawn };
	for (ECollisionChannel Channel : BlockingChannels)
	{
		if (World->OverlapAnyTestByChannel(Centre, FQuat::Identity, Channel, Mouth, Query))
		{
			SetBlockReason(EGSWarrenPlacementBlock::Obstructed);
			return false;
		}
	}

	SetBlockReason(EGSWarrenPlacementBlock::None);
	return true;
}

void UGSWarrenPlacementComponent::RefreshWards()
{
	Wards.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (UGSTopplableComponent* Topplable = It->FindComponentByClass<UGSTopplableComponent>())
		{
			// Store every topplable, warding or not - IsWarding() is asked at test time, so a statue
			// toppled while the key is held stops warding immediately rather than at the next press.
			Wards.Add(Topplable);
		}
	}
}

const UGSTopplableComponent* UGSWarrenPlacementComponent::FindWardBlocking(const FVector& Spot) const
{
	for (const TWeakObjectPtr<UGSTopplableComponent>& Weak : Wards)
	{
		const UGSTopplableComponent* Ward = Weak.Get();
		if (!Ward || !Ward->IsWarding())
		{
			continue;
		}

		// The monument answers for its own reach - a global ward reaches everywhere, a radius ward
		// measures. Asked rather than computed here so the two cannot drift apart.
		if (Ward->WardReaches(Spot))
		{
			return Ward;
		}
	}
	return nullptr;
}

void UGSWarrenPlacementComponent::SetBlockReason(EGSWarrenPlacementBlock NewReason)
{
	if (BlockReason == NewReason)
	{
		return;
	}
	BlockReason = NewReason;

	// Logged on change, because there was no way to find out WHY the ghost was red without
	// guessing - and the first guess cost a round trip. Cheap: it fires when the state flips, not
	// per frame.
	static const TCHAR* Names[] = { TEXT("placeable"), TEXT("no ground"), TEXT("ground too steep"),
									TEXT("obstructed"), TEXT("on cooldown") };
	const int32 Index = static_cast<int32>(BlockReason);
	UE_LOG(LogGSWarrenPlacement, Log, TEXT("[GoblinSiege] Warren ghost: %s."),
		Names[FMath::Clamp(Index, 0, 4)]);

	// Only on CHANGE. A HUD prompt bound to this must not be rebuilt every frame.
	OnPlacementChanged.Broadcast(bPlacing, BlockReason);
}

// ====================================================================== the ghost

void UGSWarrenPlacementComponent::EnsureGhost()
{
	AActor* Owner = GetOwner();
	if (GhostComponent || !Owner)
	{
		return;
	}

	UStaticMesh* Mesh = GhostMesh.LoadSynchronous();
	if (!Mesh)
	{
		return;   // already complained at BeginPlay; placement still works blind
	}

	GhostComponent = NewObject<UStaticMeshComponent>(Owner, TEXT("WarrenGhost"));
	if (!GhostComponent)
	{
		return;
	}
	GhostComponent->SetStaticMesh(Mesh);
	GhostComponent->SetupAttachment(Owner->GetRootComponent());

	// Absolute, because the ghost stands on the ground where it will be planted and must NOT ride
	// the player who is previewing it.
	GhostComponent->SetUsingAbsoluteLocation(true);
	GhostComponent->SetUsingAbsoluteRotation(true);

	GhostComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GhostComponent->SetCastShadow(false);
	GhostComponent->SetCanEverAffectNavigation(false);
	GhostComponent->SetVisibility(false);
	GhostComponent->RegisterComponent();

	if (UMaterialInterface* Valid = GhostMaterialValid.LoadSynchronous())
	{
		GhostMIDValid = UMaterialInstanceDynamic::Create(Valid, this);
	}
	if (UMaterialInterface* Invalid = GhostMaterialInvalid.LoadSynchronous())
	{
		GhostMIDInvalid = UMaterialInstanceDynamic::Create(Invalid, this);
	}

	// Force the first ApplyGhostMaterial to actually assign, whichever state it lands in.
	bGhostMaterialIsValid = false;
	ApplyGhostMaterial(true);
}

void UGSWarrenPlacementComponent::ShowGhost(bool bVisible)
{
	if (GhostComponent)
	{
		GhostComponent->SetVisibility(bVisible, true);
	}
}

void UGSWarrenPlacementComponent::ApplyGhostMaterial(bool bValid)
{
	if (!GhostComponent || bValid == bGhostMaterialIsValid)
	{
		return;   // only on change - SetMaterial every frame is pure churn
	}
	bGhostMaterialIsValid = bValid;

	UMaterialInstanceDynamic* MID = bValid ? GhostMIDValid : GhostMIDInvalid;
	if (!MID)
	{
		return;
	}
	const int32 Slots = GhostComponent->GetNumMaterials();
	for (int32 i = 0; i < Slots; ++i)
	{
		GhostComponent->SetMaterial(i, MID);
	}
}
