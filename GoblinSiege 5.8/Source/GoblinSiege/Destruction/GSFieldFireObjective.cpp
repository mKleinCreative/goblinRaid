#include "Destruction/GSFieldFireObjective.h"
#include "Destruction/GSBurnMaskSubsystem.h"
#include "Destruction/GSFlammableComponent.h"
#include "Destruction/GSFireVolume.h"
#include "Core/GSGameState.h"
#include "Characters/GSCharacterBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PhysicsVolume.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

// Same file-local pattern as LogGSCrumble in GSCrumbleComponent.cpp. This file logged through
// LogTemp until 2026-09-09, which is unfilterable in a log where every other burn system has its
// own category.
DEFINE_LOG_CATEGORY_STATIC(LogGSField, Log, All);

namespace GSFieldFire
{
	/**
	 * Ignition threshold, deliberately a constant rather than a UPROPERTY (2026-07-31).
	 *
	 * Fixing it at 1.0 is what makes HeatPerSecondFromNeighbour readable as a unit - "fraction of
	 * an ignition delivered per second by one fully involved orthogonal neighbour" - so that
	 * 1 / HeatPerSecondFromNeighbour is literally seconds-to-ignite. Expose this and every tuning
	 * conversation needs two numbers and a division instead of one number.
	 */
	static constexpr float IgnitionHeat = 1.f;

	/**
	 * Floor on the wind scale, so heat still crosses to upwind neighbours. See WindHeatBias: a
	 * hard 0 upwind leaves a dead-straight edge behind the ignition point.
	 */
	static constexpr float MinWindScale = 0.05f;

	// The material parameter names moved to UGSBurnMaskSubsystem on 2026-07-31 and were renamed
	// there (GS_BurnMask / GS_BurnMaskOrigin / GS_BurnMaskSize). They were never field-scoped in
	// spirit - a world position is a world position - and keeping a second copy of the contract
	// here is exactly how the two would drift.
}

AGSFieldFireObjective::AGSFieldFireObjective()
{
	PrimaryActorTick.bCanEverTick = false;
	ObjectiveType = EGSBurnObjectiveType::Field;

	// "River", not "Water" - see WaterActorNameFilters for the enumeration that settled this.
	WaterActorNameFilters.Add(FName("River"));
	CompletionThreshold01 = 0.7f; // Q-03 (ruled 2026-07-21) / spec §3.2, placeholder

	// Soft path to an asset that does not exist yet (2026-07-31). Soft rather than hard precisely
	// because of that: the class must load, the wisps must be created and placed, and the field
	// must play correctly today with nothing on the other end of this path.
	SmokeWispSystem = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(TEXT("/Game/VFX/NS_GS_Smolder.NS_GS_Smolder")));

	// Michael's own Niagara Fluids prototype, NOT NS_Fire_Big (2026-08-30, second live look). The
	// sprite system read as "little puffs, hard to tell where it actually is" once scaled up to
	// span a whole burning front - a fixed-count sprite emitter spread over a bigger area is the
	// same number of particles covering more ground, so it thins out exactly where this needs to
	// read as solid. HANDOFF-2026-08-30.md's "still rows, even under Niagara Fluids" verdict on this
	// asset was measured under the OLD per-volume architecture (many small separate instances) -
	// that says nothing about how a fluid sim behaves as ONE instance spanning the whole front,
	// which is a materially different case and worth trying now that the architecture has changed.
	// Domain-resize cost at runtime against ConsolidatedFireCurrentScale is untested - the open risk
	// the handoff itself flagged.
	ConsolidatedFireSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(
		TEXT("/Game/VFX/NG_GS_SurfaceFireLiquid.NG_GS_SurfaceFireLiquid")));
}

void AGSFieldFireObjective::BeginPlay()
{
	Super::BeginPlay();

	EnsureGridAllocated();

	if (HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(CellTickHandle, this,
				&AGSFieldFireObjective::CellTick, CellTickInterval, true);
		}
	}

	// ---- cosmetic layer (2026-07-31) --------------------------------------------------------
	// Everything below is presentation and therefore runs on the SERVER AND ON CLIENTS. This is
	// the subtle half of the burn mask: the simulation is authority-only, but the mark it leaves
	// has to appear on every machine, and the only cell information a client has is the replicated
	// state array. So the splats are driven off OnFieldCellChanged - which SetCellState broadcasts
	// on the server and OnRep_CellStates broadcasts on clients - never off CellTick, which a client
	// never runs. Bind to our own delegate rather than splatting from both places, so there is
	// exactly one path in and it is impossible for the two to drift.
	//
	// Skipped entirely on a dedicated server: no renderer, so cosmetic work there is pure waste.
	// (UGSBurnMaskSubsystem does not even exist on a dedicated server, so every SplatBurn call
	// site is null-guarded anyway - this is the cheaper outer guard, not the only one.)
	//
	// NOTE what is NOT here any more: no EnsureBurnMask, no BindCropMaterials, no initial redraw.
	// The subsystem creates its own render target lazily and binds the world's crop once at world
	// begin-play, so three fields no longer do the same sweep three times, and the "publish the
	// initial all-black state" step is redundant against a target that is cleared on creation.
	if (!IsNetMode(NM_DedicatedServer))
	{
		OnFieldCellChanged.AddDynamic(this, &AGSFieldFireObjective::HandleCellChangedForFX);

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(CosmeticTimerHandle, this,
				&AGSFieldFireObjective::CosmeticTick,
				FMath::Max(0.05f, CosmeticTickInterval), true);
		}
	}
}

void AGSFieldFireObjective::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CellTickHandle);
		World->GetTimerManager().ClearTimer(CosmeticTimerHandle);
	}

	OnFieldCellChanged.RemoveDynamic(this, &AGSFieldFireObjective::HandleCellChangedForFX);

	// Pooled volumes are ours - don't leave them burning pawns after the field is gone.
	for (TObjectPtr<AGSFireVolume>& Volume : DamageVolumes)
	{
		if (Volume)
		{
			Volume->Destroy();
		}
	}
	DamageVolumes.Reset();
	VolumeCellIndices.Reset();

	// The wisps are components on this actor, so they would go with it anyway - but a wisp is
	// meant to outlive the fire, and a thing designed to persist is exactly the thing that ends up
	// leaked when the actor is streamed out rather than destroyed. Destroy them explicitly.
	for (TObjectPtr<UNiagaraComponent>& Wisp : SmokeWisps)
	{
		if (Wisp)
		{
			Wisp->Deactivate();
			Wisp->DestroyComponent();
		}
	}
	SmokeWisps.Reset();
	WispCellIndices.Reset();
	WispAges.Reset();

	// The consolidated fire visual is a component on this actor too, and unlike a wisp it is NOT
	// meant to persist past the field - destroy it explicitly rather than trust streaming-out.
	if (ConsolidatedFireFX)
	{
		ConsolidatedFireFX->Deactivate();
		ConsolidatedFireFX->DestroyComponent();
		ConsolidatedFireFX = nullptr;
	}

	// NO render target and NO MIDs to clean up here any more (2026-07-31). Both belong to
	// UGSBurnMaskSubsystem, whose Deinitialize nulls the mask out of every bound MID and releases
	// the target - and which outlives this actor by design, because a field streaming out must not
	// take the world's burn marks with it. That is the other half of the argument for the move: a
	// mark on the ground should survive the actor that made it, and it now does.
	//
	// The marks this field already made are deliberately LEFT ON THE MASK. A razed field that is
	// streamed out and back in should still be black.

	Super::EndPlay(EndPlayReason);
}

void AGSFieldFireObjective::EnsureGridAllocated()
{
	Rows = FMath::Max(1, Rows);
	Columns = FMath::Max(1, Columns);

	// On a NON-AUTHORITY instance the replicated state array is the more trustworthy size
	// (2026-07-31). Rows and Columns now replicate COND_InitialOnly, but property and array
	// ordering within an initial bunch is not something to rely on, and BeginPlay can run before
	// either has arrived. Sizing from Rows*Columns in that window would allocate a default-shaped
	// grid and - worse - could throw away the array OnRep_CellStates had already populated.
	// ReplicatedCellStates is authoritative about how many cells there are by construction.
	const int32 Needed = (!HasAuthority() && ReplicatedCellStates.Num() > 0)
		? ReplicatedCellStates.Num()
		: Rows * Columns;

	if (Cells.Num() == Needed)
	{
		return;
	}

	Cells.Empty(Needed);
	Cells.AddDefaulted(Needed);

	// ---- the one and only fuel roll (2026-07-31) --------------------------------------------
	// FuelScale is assigned HERE, once, and never again. That is the design point of the heat
	// model, not an optimisation: organic variation has to come from stable per-cell fuel, not
	// from luck re-rolled every tick. Stable fuel gives a front that is ragged in the SAME PLACES
	// every time - which the eye reads as the field having texture. Re-rolled luck gives a front
	// whose shape means nothing from one second to the next, which is the chaos this change is
	// removing. If you ever find yourself tempted to jitter spread per tick, this comment is why
	// not.
	const float Variance = FMath::Clamp(FuelVariance, 0.f, 0.9f);
	for (FGSFieldCell& Cell : Cells)
	{
		Cell.FuelScale = FMath::FRandRange(1.f - Variance, 1.f + Variance);
	}

	if (HasAuthority())
	{
		ReplicatedCellStates.Init(static_cast<uint8>(EGSFieldCellState::Unburnt), Needed);
	}

	// Ground type, once, for the same reason FuelScale is rolled once: it cannot change during a
	// raid. Must run BEFORE RecountCellStates, which is what publishes BurnableCellCount.
	ProbeWaterCells();

	RecountCellStates();
}

// ====================================================================== water

void AGSFieldFireObjective::ProbeWaterCells()
{
	BurnableCellCount = Cells.Num();

	if (!bBlockFireOnWater)
	{
		for (FGSFieldCell& Cell : Cells)
		{
			Cell.bIsWater = false;
		}
		return;
	}

	int32 WaterCells = 0;
	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		Cells[i].bIsWater = IsWaterAtLocation(GetCellWorldLocation(i));
		if (Cells[i].bIsWater)
		{
			++WaterCells;
		}
	}

	BurnableCellCount = Cells.Num() - WaterCells;

	// Log unconditionally, not Verbose: this number decides both what can burn and what counts as
	// completion, and a silent wrong answer here reads in play as "the field will not finish" with
	// nothing to point at. One line per field per raid is not spam.
	UE_LOG(LogGSField, Log,
		TEXT("[GoblinSiege] '%s' water probe: %d of %d cell(s) are water - %d burnable."),
		*GetName(), WaterCells, Cells.Num(), BurnableCellCount);

	if (BurnableCellCount <= 0)
	{
		UE_LOG(LogGSField, Warning,
			TEXT("[GoblinSiege] '%s' has NO burnable cells - every cell probed as water. The field ")
			TEXT("can never be completed. Check WaterActorNameFilters against what is actually under it."),
			*GetName());
	}
}

bool AGSFieldFireObjective::IsWaterAtLocation(const FVector& WorldLocation) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// 1. Physics volumes first, because they are the project's existing answer to "this is water"
	//    (AGSWaterVolume, World/GSWaterVolume.h - an APhysicsVolume with bWaterVolume, which is
	//    what the sea already is). No trace needed and no naming convention to get wrong.
	for (TActorIterator<APhysicsVolume> It(const_cast<UWorld*>(World)); It; ++It)
	{
		const APhysicsVolume* Volume = *It;
		if (Volume && Volume->bWaterVolume && Volume->EncompassesPoint(WorldLocation))
		{
			return true;
		}
	}

	// 2. Then what is actually under the cell. The river on this map is Dreamscape's
	//    BP_RiverSpline - a blueprint with spline meshes, not a volume and not an Epic water body
	//    (the Water plugin is not enabled in this project at all). It does block a Visibility
	//    trace, which is what makes this detectable: measured 2026-09-09, 37 of GS_MillField's
	//    1089 cells land on one.
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(GSFieldWaterProbe), false, this);
	for (const TObjectPtr<AGSFireVolume>& Existing : DamageVolumes)
	{
		if (Existing)
		{
			TraceParams.AddIgnoredActor(Existing);
		}
	}

	FHitResult Hit;
	const FVector TraceStart = WorldLocation + FVector(0.f, 0.f, WaterProbeTraceHeight);
	const FVector TraceEnd   = WorldLocation - FVector(0.f, 0.f, WaterProbeTraceHeight);

	if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, TraceParams))
	{
		return false;
	}

	const AActor* HitActor = Hit.GetActor();
	if (!HitActor)
	{
		return false;
	}

	if (!WaterActorTag.IsNone() && HitActor->ActorHasTag(WaterActorTag))
	{
		return true;
	}

	const FString ClassName = HitActor->GetClass()->GetName();
	const FString ActorName = HitActor->GetName();
	for (const FName& Filter : WaterActorNameFilters)
	{
		if (Filter.IsNone())
		{
			continue;
		}
		const FString Needle = Filter.ToString();
		if (ClassName.Contains(Needle, ESearchCase::IgnoreCase)
			|| ActorName.Contains(Needle, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

// ====================================================================== geometry

FVector AGSFieldFireObjective::GetCellWorldLocation(int32 CellIndex) const
{
	if (!Cells.IsValidIndex(CellIndex))
	{
		return GetActorLocation();
	}

	const int32 Row = CellIndex / Columns;
	const int32 Col = CellIndex % Columns;

	// Grid is centred on the actor and rotated with it, so a field can sit at any angle.
	const float HalfW = (Columns - 1) * CellSize * 0.5f;
	const float HalfH = (Rows - 1) * CellSize * 0.5f;
	const FVector Local(Row * CellSize - HalfH, Col * CellSize - HalfW, 0.f);

	return GetActorTransform().TransformPosition(Local);
}

int32 AGSFieldFireObjective::FindNearestCell(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);

	const float HalfW = (Columns - 1) * CellSize * 0.5f;
	const float HalfH = (Rows - 1) * CellSize * 0.5f;

	// RoundToInt32, not RoundToInt: Local is an FVector, which is double under LWC, so RoundToInt
	// deduces a 64-bit result and assigning it to an int32 is a narrowing conversion warning.
	const int32 Row = FMath::Clamp(FMath::RoundToInt32((Local.X + HalfH) / CellSize), 0, Rows - 1);
	const int32 Col = FMath::Clamp(FMath::RoundToInt32((Local.Y + HalfW) / CellSize), 0, Columns - 1);

	return Row * Columns + Col;
}

bool AGSFieldFireObjective::IsEdgeCell(int32 CellIndex) const
{
	if (!Cells.IsValidIndex(CellIndex))
	{
		return false;
	}
	const int32 Row = CellIndex / Columns;
	const int32 Col = CellIndex % Columns;
	return Row == 0 || Col == 0 || Row == Rows - 1 || Col == Columns - 1;
}

bool AGSFieldFireObjective::ContainsWorldLocation(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);

	// Half-extents run to the OUTER edge of the outermost cells, not their centres.
	const float HalfW = (Columns - 1) * CellSize * 0.5f + CellSize * 0.5f;
	const float HalfH = (Rows - 1) * CellSize * 0.5f + CellSize * 0.5f;

	return FMath::Abs(Local.X) <= HalfH
		&& FMath::Abs(Local.Y) <= HalfW
		&& FMath::Abs(Local.Z) <= VerticalTolerance;
}

FVector AGSFieldFireObjective::GetBurningCentroid() const
{
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;

	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		if (Cells[i].State == EGSFieldCellState::Burning)
		{
			Sum += GetCellWorldLocation(i);
			++Count;
		}
	}

	return Count > 0 ? Sum / static_cast<float>(Count) : GetActorLocation();
}

FBox AGSFieldFireObjective::GetBurningLocalBounds() const
{
	FBox LocalBounds(ForceInit);
	const FTransform& Xform = GetActorTransform();
	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		if (Cells[i].State == EGSFieldCellState::Burning)
		{
			LocalBounds += Xform.InverseTransformPosition(GetCellWorldLocation(i));
		}
	}
	return LocalBounds;
}

// GetFieldOrigin / GetFieldWorldSize were REMOVED on 2026-07-31 with the move to a world mask.
//
// They existed to hand the crop material a rectangle to sample against, and the whole point of
// that rectangle was that it was the FIELD's. A world mask has one rectangle for the level
// (UGSBurnMaskSubsystem::GetOrigin / GetSize), so publishing a per-field one would give a material
// two conflicting answers to the same question.
//
// It also quietly removes a limitation worth recording: the old pair described an AXIS-ALIGNED
// rectangle, so a yawed field's mask sheared. The subsystem's rectangle is axis-aligned in world
// space and the splats are placed by world position, so a rotated field now marks correctly with
// no extra material parameters and no basis vectors.

float AGSFieldFireObjective::GetCellIntensity01(int32 CellIndex) const
{
	if (!Cells.IsValidIndex(CellIndex))
	{
		return 0.f;
	}

	const FGSFieldCell& Cell = Cells[CellIndex];

	// Only burning cells radiate, glow, or drive a flame. Ash and wet crop are 0 by definition,
	// and callers rely on that - the heat sweep uses it to skip, the mask uses it to leave G clear.
	if (Cell.State != EGSFieldCellState::Burning)
	{
		return 0.f;
	}

	if (CellBurnSeconds <= 0.f)
	{
		return 1.f;
	}

	const float T = FMath::Clamp(Cell.BurnSeconds / CellBurnSeconds, 0.f, 1.f);

	float In  = FMath::Clamp(IntensityRampInFraction, 0.f, 1.f);
	float Out = FMath::Clamp(IntensityRampOutFraction, 0.f, 1.f);

	// If a designer sets the two ramps to more than the whole burn, scale both down in proportion
	// rather than letting the trapezoid invert. The result degrades to a triangle - peak intensity
	// at one instant instead of a plateau - which is a sensible reading of "ramp for longer than
	// the burn lasts", and crucially it can never return a negative or >1 value.
	if (In + Out > 1.f)
	{
		const float Renormalise = 1.f / (In + Out);
		In  *= Renormalise;
		Out *= Renormalise;
	}

	if (In > 0.f && T < In)
	{
		return T / In;
	}
	if (Out > 0.f && T > 1.f - Out)
	{
		return (1.f - T) / Out;
	}
	return 1.f;
}

FVector AGSFieldFireObjective::SnapPointToGround(const FVector& InLocation) const
{
	const UWorld* World = GetWorld();
	if (!World || !bSnapVolumesToGround || GroundSnapTraceHeight <= 0.f)
	{
		return InLocation;
	}

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(GSFieldVolumeSnap), false, this);

	// Ignore our own fire volumes, or a volume already standing there catches the trace and the
	// next thing we place stacks on top of it, climbing a little further off the ground each time.
	for (const TObjectPtr<AGSFireVolume>& Existing : DamageVolumes)
	{
		if (Existing)
		{
			TraceParams.AddIgnoredActor(Existing);
		}
	}

	// The wisps need no ignore entry: they are components on THIS actor, already ignored above.

	FHitResult Hit;
	const FVector TraceStart = InLocation + FVector(0.f, 0.f, GroundSnapTraceHeight);
	const FVector TraceEnd   = InLocation - FVector(0.f, 0.f, GroundSnapTraceHeight * 2.f);

	if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, TraceParams))
	{
		return Hit.ImpactPoint;
	}
	return InLocation;
}

// ====================================================================== ignition

void AGSFieldFireObjective::IgniteAtLocation(const FVector& WorldLocation)
{
	if (!HasAuthority() || IsComplete())
	{
		return;
	}

	// FindNearestCell CLAMPS to the grid, so without this guard a torch landing 500m away would
	// still light an edge cell. The torch path is already gated by ContainsWorldLocation, but
	// Blueprint callers are not - bounds-check here rather than trusting the caller.
	if (!ContainsWorldLocation(WorldLocation))
	{
		return;
	}

	// Direct ignition: a torch relights doused firebreaks (decision 26 / spec §3.3).
	IgniteCell(FindNearestCell(WorldLocation), /*bDirectIgnition=*/true);
}

void AGSFieldFireObjective::IgniteCell(int32 CellIndex, bool bDirectIgnition)
{
	if (!Cells.IsValidIndex(CellIndex))
	{
		return;
	}

	// WATER NEVER BURNS, by any route. This is above the state checks on purpose: it is a property
	// of the ground, not of what has happened to the cell, so a torch thrown directly into the
	// river must fail exactly as spread across it does. bDirectIgnition is not an override here -
	// that flag exists to let a torch beat a DOUSED firebreak, which is a wet crop that can dry
	// out, and a river is not that.
	if (Cells[CellIndex].bIsWater)
	{
		return;
	}

	const EGSFieldCellState State = Cells[CellIndex].State;

	// Ash never relights, and an already-burning cell doesn't restack.
	if (State == EGSFieldCellState::Burnt || State == EGSFieldCellState::Burning)
	{
		return;
	}

	// THE FIREBREAK. Spread cannot cross a doused cell; a torch can relight it. Collapsing these
	// two cases is what made dousing feel like it did nothing.
	if (State == EGSFieldCellState::Doused && !bDirectIgnition)
	{
		return;
	}

	FGSFieldCell& Cell = Cells[CellIndex];

	// ---- where this cell starts on its burn arc (2026-07-31) --------------------------------
	//
	// RE-IGNITION CLAMP first. BurnSeconds is deliberately preserved across a douse so that a
	// part-burnt crop finishes faster than fresh crop - that intent is kept, and it is why this
	// clamps rather than resets. But unclamped it had a perverse consequence: a cell doused at
	// 20 of 22 seconds relights inside its ramp-OUT, which means it comes back at ~0.26 intensity,
	// burns out two seconds later and radiates almost nothing into its neighbours. Re-torching the
	// firebreak the brigade just made was therefore close to useless - the one counter-play the
	// player has against dousing. Landing it at the START of the plateau keeps the head start
	// (it still finishes in ramp-out time rather than a full 22 s) while guaranteeing it gets a
	// full-strength stretch to push the front back across the break.
	const float PlateauStart =
		CellBurnSeconds * FMath::Clamp(1.f - IntensityRampOutFraction, 0.f, 1.f);
	Cell.BurnSeconds = FMath::Min(Cell.BurnSeconds, PlateauStart);

	// FRESH-CELL FLOOR second. A cell lit this tick used to sit at BurnSeconds == 0, so
	// GetCellIntensity01 returned exactly 0, so pass 2 skipped it entirely: a newly-caught cell
	// contributed NO heat at all on its first tick and near-zero through its whole ramp-in. Every
	// cell in the chain paid that, which compounds directly into the spread rate the H_peak
	// relation solves for. Starting one tick in is the smallest correction that removes the hard
	// zero without touching the ramp SHAPE - which stays as authored, because the slow start is
	// the "grows as it goes" read and is wanted.
	Cell.BurnSeconds = FMath::Max(Cell.BurnSeconds, FMath::Min(CellTickInterval, CellBurnSeconds));

	SetCellState(CellIndex, EGSFieldCellState::Burning);

	// First crop alight this raid: arm the unseen-fire fuse. UGSFlammableComponent::Ignite() does
	// this for ordinary structures, but the field's cells are plain data and never touch it.
	if (!bHasReportedFireStarted)
	{
		bHasReportedFireStarted = true;
		if (UWorld* World = GetWorld())
		{
			if (AGSGameState* GS = World->GetGameState<AGSGameState>())
			{
				GS->ReportFireStarted();
			}
		}
	}
}

void AGSFieldFireObjective::SetCellState(int32 CellIndex, EGSFieldCellState NewState)
{
	if (!Cells.IsValidIndex(CellIndex))
	{
		return;
	}

	FGSFieldCell& Cell = Cells[CellIndex];
	if (Cell.State == NewState)
	{
		return;
	}

	// Maintain counters incrementally - recounting hundreds of cells every tick is the kind of
	// thing that quietly eats a frame budget.
	switch (Cell.State)
	{
	case EGSFieldCellState::Burning: --BurningCellCount; break;
	case EGSFieldCellState::Burnt:   --BurntCellCount;   break;
	case EGSFieldCellState::Doused:  --DousedCellCount;  break;
	default: break;
	}

	Cell.State = NewState;

	switch (NewState)
	{
	case EGSFieldCellState::Burning: ++BurningCellCount; break;
	case EGSFieldCellState::Burnt:   ++BurntCellCount;   break;
	case EGSFieldCellState::Doused:  ++DousedCellCount;  break;
	default: break;
	}

	if (NewState == EGSFieldCellState::Burnt)
	{
		// Stamp WHEN this cell became ash, for the smoke wake's "most recently burnt" search
		// (2026-07-31). Burnt is terminal, so without this every razed cell is indistinguishable
		// afterwards and the wisp pool can only ever sit on whichever cells finished first.
		Cell.BurntSequence = ++BurntSequenceCounter;

		// Ash is fully charred by definition. See FGSFieldCell::CharHigh - the mask's R channel is
		// contractually monotonic, so it is pinned here rather than left to a BurnSeconds ratio
		// that a re-torch could walk backwards.
		Cell.CharHigh = 1.f;
	}

	if (HasAuthority() && ReplicatedCellStates.IsValidIndex(CellIndex))
	{
		ReplicatedCellStates[CellIndex] = static_cast<uint8>(NewState);
	}

	// Server broadcasts locally; clients get theirs from OnRep_CellStates.
	OnFieldCellChanged.Broadcast(CellIndex, NewState);
}

void AGSFieldFireObjective::OnRep_CellStates()
{
	// OnRep can land before BeginPlay on a client, so make sure the grid exists first.
	if (Cells.Num() != ReplicatedCellStates.Num())
	{
		Cells.Empty(ReplicatedCellStates.Num());
		Cells.AddDefaulted(ReplicatedCellStates.Num());
	}

	// Diff against local state and fire the cosmetic hook only for cells that actually moved. This
	// is what lets the FX layer rebuild its Niagara position array on change instead of per tick.
	for (int32 i = 0; i < ReplicatedCellStates.Num(); ++i)
	{
		const EGSFieldCellState NewState = static_cast<EGSFieldCellState>(ReplicatedCellStates[i]);
		if (Cells[i].State != NewState)
		{
			// Restart the LOCAL cosmetic burn clock as a cell catches (2026-07-31). BurnSeconds is
			// server business and is never replicated - but the burn mask's ember channel and any
			// client-side flame arc are both driven by GetCellIntensity01, which reads it, so on a
			// client it has to come from somewhere. CosmeticTick advances it locally; this is
			// where it is zeroed.
			//
			// This deliberately does NOT reproduce the server's "doused crop keeps its BurnSeconds
			// so re-torching finishes faster" rule. That rule is about SIMULATION timing, which the
			// client does not run; here the only consumer is a 0..1 glow curve, and a re-lit cell
			// glowing from the start of its arc is the correct read either way. OnRep only ever
			// runs on clients, so the authority's value can never be trampled by this - the
			// HasAuthority guard is belt-and-braces for a future listen-server manual OnRep call.
			if (!HasAuthority() && NewState == EGSFieldCellState::Burning)
			{
				Cells[i].BurnSeconds = 0.f;
			}

			if (NewState == EGSFieldCellState::Burnt)
			{
				// The client's own copies of the two things SetCellState stamps on the server, and
				// they have to be maintained here because OnRep writes State directly rather than
				// going through SetCellState (2026-07-31).
				//
				// BurntSequence: the smoke wake runs on clients now, so it needs the same recency
				// ordering. The counters are per-machine and the ABSOLUTE values will differ from
				// the server's - that is fine and is why this is a counter rather than a
				// timestamp, since only the comparison is ever used, and the order cells arrive in
				// is the order they burnt.
				//
				// CharHigh: pinning it here is what stops a re-torched cell un-charring on a
				// client. The zeroing of BurnSeconds just above is exactly the write that used to
				// walk the mask's R channel backwards.
				Cells[i].BurntSequence = ++BurntSequenceCounter;
				Cells[i].CharHigh = 1.f;
			}

			Cells[i].State = NewState;
			OnFieldCellChanged.Broadcast(i, NewState);
		}
	}

	RecountCellStates();
}

void AGSFieldFireObjective::RecountCellStates()
{
	BurningCellCount = 0;
	BurntCellCount = 0;
	DousedCellCount = 0;

	for (const FGSFieldCell& Cell : Cells)
	{
		switch (Cell.State)
		{
		case EGSFieldCellState::Burning: ++BurningCellCount; break;
		case EGSFieldCellState::Burnt:   ++BurntCellCount;   break;
		case EGSFieldCellState::Doused:  ++DousedCellCount;  break;
		default: break;
		}
	}
}

// ====================================================================== the burn

void AGSFieldFireObjective::CellTick()
{
	if (!HasAuthority())
	{
		return;
	}

	// Idle cheaply only when there is genuinely nothing to advance. Doused cells still need ticking
	// even with no fire left, or wet crop would stay wet forever and the objective could be
	// permanently deleted by the defenders.
	//
	// ...unless firebreaks are PERMANENT (DousedDryOutSeconds == 0), in which case doused cells
	// have no dry-out clock to run and there is nothing to advance after all (2026-07-31). Without
	// that second half, one permanently-doused cell on an otherwise dead field kept the whole
	// five-pass sweep - and its two TArray allocations - running at 4 Hz for the rest of the raid,
	// to do nothing at all.
	if (BurningCellCount == 0 && (DousedCellCount == 0 || DousedDryOutSeconds <= 0.f))
	{
		// ...but heat still has to bleed off (2026-07-31). With the fire out, cells that were one
		// tick short of catching would otherwise FREEZE at 0.9-something for the rest of the raid,
		// and the next torch anywhere on the field would flash them over instantly - the field
		// would "remember" a fire that died ten minutes ago. Decay-only sweep, no allocations, no
		// neighbour work; it costs one pass over a few dozen floats.
		//
		// It does NOT stop once the grid is cold, whatever an earlier version of this comment
		// claimed - there is no break and the timer is never cleared, so this pass keeps running
		// at CellTickInterval for as long as the field exists. That is deliberate (the field must
		// still respond to a torch at any moment) and it is cheap, but the comment said otherwise
		// and someone budgeting frame time deserves the truth.
		for (FGSFieldCell& Cell : Cells)
		{
			if (Cell.Heat > 0.f)
			{
				Cell.Heat = FMath::Max(0.f, Cell.Heat - HeatDecayPerSecond * CellTickInterval);
			}
		}
		return;
	}

	// ---- HEAT DIFFUSION (2026-07-31, objective 2) --------------------------------------------
	// Five ordered passes. The order is load-bearing, not stylistic - see each pass.
	//
	// What this replaced: an every-tick FRand() coin flip against four orthogonal neighbours.
	// See HeatPerSecondFromNeighbour in the header for why percolation could never produce a
	// coherent front no matter how it was tuned.

	const int32 NumCells = Cells.Num();

	// Wind in grid space: X = column axis, Y = row axis, matching WindDirectionDeg's documented
	// convention (0 degrees = +column).
	const float WindRadians = FMath::DegreesToRadians(WindDirectionDeg);
	const FVector2D WindDirection(FMath::Cos(WindRadians), FMath::Sin(WindRadians));

	// ---- pass 1: dry-out, and heat decay on everything not currently alight ------------------
	// Decay runs BEFORE any heat is added this tick, so a cell that is still being fed nets out
	// positive while one that has lost its neighbour genuinely cools. Do it the other way round
	// and a cell right on the ignition boundary flickers across the threshold.
	//
	// Also collects the burning set, because pass 2 iterates BURNING cells rather than all cells -
	// burning is much the smaller set, and it is the one doing the work.
	TArray<int32> BurningCells;
	BurningCells.Reserve(BurningCellCount);

	for (int32 i = 0; i < NumCells; ++i)
	{
		FGSFieldCell& Cell = Cells[i];

		switch (Cell.State)
		{
		case EGSFieldCellState::Doused:
			// A FIREBREAK MUST ACTUALLY BREAK THE FRONT. Wet crop never accumulates heat and is
			// held at zero, so the brigade's work is not quietly undone by a cell that kept the
			// heat it had banked before the water landed and ignites the instant it dries.
			Cell.Heat = 0.f;

			// Wet crop drying back to dry crop. Keeps BurnSeconds, so a cell that was part-burnt
			// before the brigade saved it still finishes faster once it catches again.
			if (DousedDryOutSeconds > 0.f)
			{
				Cell.DousedSeconds += CellTickInterval;
				if (Cell.DousedSeconds >= DousedDryOutSeconds)
				{
					Cell.DousedSeconds = 0.f;
					SetCellState(i, EGSFieldCellState::Unburnt);
					// Leaves the cell Unburnt with Heat already 0 - freshly dried crop starts cold
					// rather than resuming from wherever it was when the water hit.
				}
			}
			break;

		case EGSFieldCellState::Unburnt:
			Cell.Heat = FMath::Max(0.f, Cell.Heat - HeatDecayPerSecond * CellTickInterval);
			break;

		case EGSFieldCellState::Burning:
			// A burning cell has no use for stored heat; zero it so a cell that is doused and
			// later dries does not restart with a full tank.
			Cell.Heat = 0.f;
			BurningCells.Add(i);
			break;

		case EGSFieldCellState::Burnt:
			// Ash never re-accumulates. Nothing writes into it in pass 2 either, but zeroing here
			// means the value can never be resurrected by a future code path.
			Cell.Heat = 0.f;
			break;

		default:
			break;
		}
	}

	// ---- pass 2: push heat outward from burning cells ----------------------------------------
	// Accumulated into a scratch array and applied afterwards, NOT written straight into the
	// cells. That is what makes the result independent of iteration order: if cell 12 wrote
	// directly, cell 13 would be reading a grid that cell 12 had already advanced, and the fire
	// would spread measurably faster toward increasing indices than toward decreasing ones - a
	// directional bias with no design behind it, purely an artefact of a for loop.
	TArray<float> HeatDelta;
	HeatDelta.Init(0.f, NumCells);

	for (const int32 SourceIndex : BurningCells)
	{
		// The emitting cell's own burn arc. THIS is what delivers "grows as it goes": a cell that
		// caught a second ago radiates almost nothing, a fully involved one radiates at full
		// strength. A broad established front therefore pushes far harder into the crop ahead of
		// it than a single lit cell does, so the fire accelerates as it grows - with no
		// special-case code, no ramp timer, and nothing to keep in sync.
		const float SourceIntensity = GetCellIntensity01(SourceIndex);
		if (SourceIntensity <= 0.f)
		{
			continue;
		}

		const int32 Row = SourceIndex / Columns;
		const int32 Col = SourceIndex % Columns;

		// MOORE neighbourhood (8), not von Neumann (4). Four-neighbour spread can only ever grow a
		// diamond; eight with 1/sqrt(2) on the diagonals rounds the front off.
		for (int32 DeltaRow = -1; DeltaRow <= 1; ++DeltaRow)
		{
			for (int32 DeltaCol = -1; DeltaCol <= 1; ++DeltaCol)
			{
				if (DeltaRow == 0 && DeltaCol == 0)
				{
					continue;
				}

				const int32 NeighbourRow = Row + DeltaRow;
				const int32 NeighbourCol = Col + DeltaCol;
				if (NeighbourRow < 0 || NeighbourCol < 0 || NeighbourRow >= Rows || NeighbourCol >= Columns)
				{
					continue;
				}

				const int32 NeighbourIndex = NeighbourRow * Columns + NeighbourCol;

				// Only dry crop takes heat. Doused cells are firebreaks, Burnt is ash, Burning is
				// already alight - all three are skipped, which is the firebreak mechanic intact.
				if (Cells[NeighbourIndex].State != EGSFieldCellState::Unburnt)
				{
					continue;
				}

				const bool bDiagonal = (DeltaRow != 0 && DeltaCol != 0);
				const float DistanceWeight = bDiagonal ? DiagonalHeatScale : 1.f;

				// Wind: continuous around the compass rather than a flat multiplier on two
				// neighbours, floored so upwind crop still creeps. Grid space (X = column axis).
				const FVector2D Offset(static_cast<float>(DeltaCol), static_cast<float>(DeltaRow));
				const FVector2D OffsetDirection = Offset.GetSafeNormal();
				// The cast is not cosmetic: FVector2D is double-precision under LWC, so
				// DotProduct returns a double and the whole expression would widen to double and
				// then narrow on assignment - a warning that is an error under this project's
				// settings. Cast the dot product, keep the arithmetic in float.
				const float WindScale = FMath::Max(
					GSFieldFire::MinWindScale,
					1.f + WindHeatBias * static_cast<float>(
						FVector2D::DotProduct(OffsetDirection, WindDirection)));

				// FuelScale is the RECEIVER's - how readily this patch of crop takes heat - and it
				// was rolled once at grid allocation and never re-rolled. See FuelVariance.
				HeatDelta[NeighbourIndex] +=
					HeatPerSecondFromNeighbour
					* DistanceWeight
					* WindScale
					* SourceIntensity
					* Cells[NeighbourIndex].FuelScale
					* CellTickInterval;
			}
		}
	}

	// ---- pass 3: apply the accumulated heat and collect ignitions ----------------------------
	// Ignitions are applied after every pass, so a cell lit this tick does not immediately spread
	// in the same tick (which would flash the field over in one frame).
	TArray<int32> PendingIgnitions;

	for (int32 i = 0; i < NumCells; ++i)
	{
		if (HeatDelta[i] <= 0.f)
		{
			continue;
		}

		FGSFieldCell& Cell = Cells[i];

		// The state re-check that used to sit here has been deleted (2026-07-31). It could not
		// fire: pass 1 runs entirely before pass 2, so any Doused-to-Unburnt dry-out has already
		// happened by the time pass 2 decides who receives heat, and pass 2 only ever writes into
		// cells it has just confirmed are Unburnt. Nothing between the two passes changes state.
		// Its comment claimed the opposite pass ordering, which is the more dangerous half of the
		// problem - it would have justified a real reordering later on.
		Cell.Heat += HeatDelta[i];
		if (Cell.Heat >= GSFieldFire::IgnitionHeat)
		{
			PendingIgnitions.Add(i);
		}
	}

	// ---- pass 4: advance the burn and retire finished cells -----------------------------------
	// Iterates the pass-1 snapshot, so cells ignited this tick start their burn next tick with a
	// full CellBurnSeconds ahead of them - and so the intensity used for heat above was the value
	// at the START of the tick for every cell, uniformly.
	for (const int32 i : BurningCells)
	{
		FGSFieldCell& Cell = Cells[i];
		if (Cell.State != EGSFieldCellState::Burning)
		{
			continue;
		}

		Cell.BurnSeconds += CellTickInterval;

		// Ratchet the char high-water mark alongside it (2026-07-31). CharHigh exists because
		// BurnSeconds is NOT monotonic - it restarts on a client every time a cell re-enters
		// Burning - while the mask's R channel is contractually monotonic. See FGSFieldCell.
		if (CellBurnSeconds > 0.f)
		{
			Cell.CharHigh = FMath::Max(Cell.CharHigh,
				FMath::Clamp(Cell.BurnSeconds / CellBurnSeconds, 0.f, 1.f));
		}

		if (Cell.BurnSeconds < CellBurnSeconds)
		{
			continue;
		}

		SetCellState(i, EGSFieldCellState::Burnt);

		if (bCanJumpToAdjacentFlammables && !Cell.bHasJumped && IsEdgeCell(i))
		{
			Cell.bHasJumped = true;
			TryJumpToAdjacentFlammables(i);
		}
	}

	// ---- pass 5: ignite -----------------------------------------------------------------------
	for (const int32 Idx : PendingIgnitions)
	{
		// Spread, not a torch: IgniteCell's bDirectIgnition=false is what stops it crossing a
		// firebreak. Belt-and-braces given pass 2 already refuses to heat a Doused cell.
		IgniteCell(Idx, /*bDirectIgnition=*/false);
	}

	UpdateDamageVolumes();

	// UpdateSmokeWisps is deliberately NOT called from here any more (2026-07-31). The wisps are
	// bare runtime UNiagaraComponents and runtime-created components do not replicate, so anything
	// this authority-only function creates exists on the authority alone - which on a dedicated
	// server means nobody sees the smoke and the one machine with no renderer runs all the ground
	// traces. It runs from CosmeticTick now, alongside the rest of the cosmetic layer. Everything
	// it reads (Burnt state) is replicated.

	SetCompletion01(GetBurntFraction());
}

// ====================================================================== damage volumes

void AGSFieldFireObjective::UpdateDamageVolumes()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}

	// Collect the burning front.
	TArray<int32> Burning;
	Burning.Reserve(BurningCellCount);
	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		if (Cells[i].State == EGSFieldCellState::Burning)
		{
			Burning.Add(i);
		}
	}

	// Fire out: retire the pool so nothing keeps burning pawns on a dead field.
	if (!bSpawnDamageVolumes || Burning.Num() == 0)
	{
		for (TObjectPtr<AGSFireVolume>& Volume : DamageVolumes)
		{
			if (Volume)
			{
				Volume->Destroy();
			}
		}
		DamageVolumes.Reset();
		VolumeCellIndices.Reset();
		return;
	}

	// ---- sticky slot -> cell assignment (2026-07-31) ----------------------------------------
	// A volume keeps its cell while that cell is still burning, and only moves when it goes out -
	// and then only to the NEAREST unclaimed burning cell. See VolumeCellIndices in the header for
	// why: the old even-slice sampling re-derived every slot from a re-sorted array each tick, so
	// flames teleported across the field instead of creeping.
	// How many volumes the current front justifies. Computed BEFORE the slot array is grown
	// (2026-07-31) because it is now the size that array is held to in both directions - see the
	// trim below.
	//
	// Clamped rather than trusted: the ClampMin="1" meta on MaxFireVolumes only constrains the
	// details panel, not a value coming from a config .ini or a Blueprint class default, and a
	// negative here would make Wanted negative and the SeedPick divide-by-2*Wanted blow up.
	const int32 Wanted = FMath::Clamp(FMath::Min(MaxFireVolumes, Burning.Num()), 0, Burning.Num());

	// Grow explicitly rather than SetNum - a default-constructed int32 is 0, which would silently
	// claim cell 0 instead of reading as unassigned.
	//
	// The grow target is Wanted, NOT Max(DamageVolumes.Num(), Wanted) as it was until 2026-07-31.
	// Growing to the live volume count kept slots alive past the point the retire loop below had
	// already destroyed their volumes, and those stale slots still held cell claims - see the trim.
	while (VolumeCellIndices.Num() < Wanted)
	{
		VolumeCellIndices.Add(INDEX_NONE);
	}

	// Trim on the way IN as well as on the way out. The retire loop at the end of this function
	// already trims to Wanted, but that leaves a one-update stale window: if the front shrinks
	// twice in a row, the Claimed set built below would still read the entries left over from the
	// PREVIOUS Wanted, hold their cells as claimed with no volume standing on them, and starve a
	// live slot into the SeedPick fallback - a single-frame teleport, the exact artefact this
	// whole mechanism exists to remove. Trimming here costs nothing and closes it.
	if (VolumeCellIndices.Num() > Wanted)
	{
		VolumeCellIndices.SetNum(Wanted);
	}

	for (int32& Assigned : VolumeCellIndices)
	{
		// Drop any assignment whose cell stopped burning; everything else stays put.
		if (Assigned != INDEX_NONE && !Burning.Contains(Assigned))
		{
			Assigned = INDEX_NONE;
		}
	}

	TSet<int32> Claimed;
	for (int32 Assigned : VolumeCellIndices)
	{
		if (Assigned != INDEX_NONE)
		{
			Claimed.Add(Assigned);
		}
	}

	UClass* VolumeClass = FireVolumeClass ? FireVolumeClass.Get() : AGSFireVolume::StaticClass();

	for (int32 Slot = 0; Slot < Wanted; ++Slot)
	{
		// Sticky: keep the cell we're already on. Only when it stops burning do we move, and then
		// to the nearest unclaimed burning cell - so a volume creeps to adjacent ground instead of
		// jumping to wherever an even slice of a re-sorted array happens to point.
		if (!VolumeCellIndices.IsValidIndex(Slot))
		{
			VolumeCellIndices.Add(INDEX_NONE);
		}

		if (VolumeCellIndices[Slot] == INDEX_NONE)
		{
			// Anchor the search at where this volume already stands; for a brand-new slot fall back
			// to the old even-slice pick so the initial pool still straddles the front.
			const int32 SeedPick = (Burning.Num() * (2 * Slot + 1)) / (2 * Wanted);
			const FVector SearchOrigin = DamageVolumes.IsValidIndex(Slot) && DamageVolumes[Slot]
				? DamageVolumes[Slot]->GetActorLocation()
				: GetCellWorldLocation(Burning[FMath::Clamp(SeedPick, 0, Burning.Num() - 1)]);

			int32 BestCell = INDEX_NONE;
			float BestDistSq = TNumericLimits<float>::Max();
			for (const int32 CandidateCell : Burning)
			{
				if (Claimed.Contains(CandidateCell))
				{
					continue;
				}

				const float DistSq = FVector::DistSquared(GetCellWorldLocation(CandidateCell), SearchOrigin);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					BestCell = CandidateCell;
				}
			}

			// Every burning cell already spoken for (more volumes than fire): double up on the
			// seed rather than leaving a slot dark.
			if (BestCell == INDEX_NONE)
			{
				BestCell = Burning[FMath::Clamp(SeedPick, 0, Burning.Num() - 1)];
			}

			VolumeCellIndices[Slot] = BestCell;
			Claimed.Add(BestCell);
		}

		// Drop the volume onto whatever is actually underneath it. The cell grid is one flat
		// plane; real terrain is not. Factored into SnapPointToGround on 2026-07-31 so the smoke
		// wisps land on the same ground by the same trace.
		const FVector Target = SnapPointToGround(GetCellWorldLocation(VolumeCellIndices[Slot]));

		if (!DamageVolumes.IsValidIndex(Slot) || !DamageVolumes[Slot])
		{
			// Deferred spawn so pooled mode is set BEFORE BeginPlay starts the lifespan/spread.
			AGSFireVolume* Volume = World->SpawnActorDeferred<AGSFireVolume>(
				VolumeClass, FTransform(Target), this, nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

			if (!Volume)
			{
				continue;
			}

			// bEnableSmoke=false when the field owns its own smoke (2026-07-31). The wisps behind
			// the front are the plume now; leaving it on here as well would double it up on the
			// one part of the field that already reads. Must be set before FinishSpawning, since
			// ApplyFireFX runs off BeginPlay.
			// bInEnableLight=(Slot < MaxLitVolumes) (2026-07-31, Q-33). The volume count and the
			// LIGHT count are decoupled here: 10 volumes exist for the flame read (raised from 3 on
			// 2026-07-30 - "3 big volumes read as three tidy bonfires"), but burn-types spec §3.4
			// caps the Blaze stage at 3 pooled shadowless point lights, and AGSFireVolume builds a
			// UPointLightComponent unconditionally, so before this the two numbers were the same by
			// accident. With three fields on Tutorial_Island (T-09) that was up to 30 dynamic
			// lights. Passed here so the unlit volumes never start a flicker timer at all.
			Volume->ConfigurePooled(FireVolumeRadius,
				/*bInEnableSmoke=*/!bFieldOwnsSmoke,
				/*bInEnableLight=*/Slot < MaxLitVolumes,
				/*bInEnableFireFX=*/!bConsolidateFireVisual);
			Volume->FinishSpawning(FTransform(Target));

			if (DamageVolumes.IsValidIndex(Slot))
			{
				DamageVolumes[Slot] = Volume;
			}
			else
			{
				DamageVolumes.Add(Volume);
			}
		}
		else
		{
			DamageVolumes[Slot]->SetActorLocation(Target);
		}

		// Drive the fire's ARC. Michael's reference note was that the big meteor-scale look is the
		// PEAK, so it has to be earned.
		//
		// REPLACED on 2026-07-31 (was: a count of burning cells within FireVolumeRadius*1.6,
		// saturating at IntensitySaturationCells). Replaced rather than reconciled, because the
		// two answers were measuring different things and disagreeing about when the fire peaked:
		// the density term peaked when the volume happened to sit in a crowd, the cell's own burn
		// peaked partway through its 22 seconds, and the heat model has a third opinion. Reading
		// the arc off the cell the volume is PARKED ON makes all three literally the same number -
		// the flames grow exactly as fast as the cell radiates, and the mask's ember channel
		// glows on the same curve. That is what the shared helper exists for.
		//
		// The small-flame-for-a-new-cell read the density term used to provide is not lost: a cell
		// that just caught returns near 0 here, and AGSFireVolume::MinIntensityScale floors the
		// visual at 0.4 rather than nothing.
		if (DamageVolumes.IsValidIndex(Slot) && DamageVolumes[Slot])
		{
			DamageVolumes[Slot]->SetFireIntensity(GetCellIntensity01(VolumeCellIndices[Slot]));

			// Re-assert the light cap EVERY update, not just at spawn (2026-07-31, Q-33).
			//
			// The pooled volumes are REPOSITIONED, NOT RESPAWNED - a volume is spawned once and
			// then walks the front for the rest of the raid - so the ConfigurePooled flag above
			// only ever describes the pool as it was at the moment each actor happened to be
			// created. Slot COUNT moves with the front (Wanted = min(MaxFireVolumes, burning
			// cells)), so a slot that spawned as slot 7 while the whole field was alight is still
			// slot 7 when the front has shrunk to four volumes - but it is now a surplus slot being
			// retired, and conversely MaxLitVolumes can be changed live from the details panel
			// during a tuning pass. Without this line the cap would be silently wrong in both
			// directions.
			//
			// SetLightEnabled is idempotent and early-outs on no change, so calling it on ten
			// volumes four times a second costs ten bool compares.
			DamageVolumes[Slot]->SetLightEnabled(Slot < MaxLitVolumes);
		}
	}

	// Retire any surplus if the front shrank.
	for (int32 Slot = DamageVolumes.Num() - 1; Slot >= Wanted; --Slot)
	{
		if (DamageVolumes[Slot])
		{
			DamageVolumes[Slot]->Destroy();
		}
		DamageVolumes.RemoveAt(Slot);
	}

	// ...and trim the parallel assignment array with it (2026-07-31). Missing this is what made
	// sticky assignment fail silently rather than loudly: the two arrays are parallel, so a
	// VolumeCellIndices entry above Wanted describes a volume that no longer exists - but it was
	// still read into the Claimed set at the top of the next update. With a shrinking front those
	// ghost claims cover every remaining burning cell, the nearest-unclaimed search finds nothing,
	// and every live slot falls through to the even-slice SeedPick - which is exactly the
	// teleporting flame that sticky assignment was written to eliminate. The symptom only appears
	// AFTER the fire peaks, which is why it survived a first pass.
	VolumeCellIndices.SetNum(Wanted);
}

// ====================================================================== smoke wake (objective 3)

void AGSFieldFireObjective::UpdateSmokeWisps()
{
	UWorld* World = GetWorld();
	if (!World || MaxSmokeWisps <= 0)
	{
		return;
	}

	// NOT HasAuthority (2026-07-31). This used to be authority-gated, which is the correct instinct
	// for anything that touches simulation and exactly wrong for this: wisps are plain runtime
	// UNiagaraComponents, runtime-created components do not replicate, and so an authority-only
	// wisp is one that no client ever sees. On a dedicated server that meant the objective-3 smoke
	// existed on precisely the one machine with no renderer, and nowhere else.
	//
	// The gate that IS right is the renderless one. Everything this function reads - which cells
	// are Burnt, and in what order - a client has from ReplicatedCellStates.
	if (IsNetMode(NM_DedicatedServer))
	{
		return;
	}

	// Collect the WAKE, not the front. That one line is the entire architectural difference from
	// UpdateDamageVolumes, and it is why smoke can now stay behind: the volumes track Burning
	// cells and move as those change, these track Burnt cells, which never change again.
	TArray<int32> BurntCells;
	BurntCells.Reserve(BurntCellCount);
	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		if (Cells[i].State == EGSFieldCellState::Burnt)
		{
			BurntCells.Add(i);
		}
	}

	if (BurntCells.Num() == 0)
	{
		return;
	}

	// Note the absence of a RETIRE path, in deliberate contrast to UpdateDamageVolumes. A fire
	// volume must be destroyed when its cell stops burning - it is doing damage. A wisp must NOT
	// be, because persistence is its whole job: the razed field should still be smoking when the
	// raid ends, which is what makes it read from across the valley (GDD §6.3).
	//
	// A wisp can be RE-SEEDED, though, which is a different thing and was missing until
	// 2026-07-31 - see the reseed pass below and WispReseedSeconds in the header. Nothing is ever
	// destroyed here; a wisp only ever changes which razed cell it stands on.
	const int32 Wanted = FMath::Min(MaxSmokeWisps, BurntCells.Num());

	// RESOLVE ONCE (2026-07-31). This used to call LoadSynchronous() on every update - 4 Hz, for
	// the whole raid - against a default path that points at an asset which does not exist yet.
	// LoadSynchronous does not cache a failure, so that was a failed package load plus an engine
	// warning four times a second; the bWarnedMissingSmokeSystem latch only ever suppressed OUR
	// log line, not the work behind it. Once the failure latch below is set the load is skipped
	// entirely - see ResolvedSmokeSystem in the header.
	if (!ResolvedSmokeSystem && !bSmokeSystemResolveFailed)
	{
		ResolvedSmokeSystem = SmokeWispSystem.IsNull() ? nullptr : SmokeWispSystem.LoadSynchronous();
		bSmokeSystemResolveFailed = (ResolvedSmokeSystem == nullptr);
	}

	UNiagaraSystem* System = ResolvedSmokeSystem;
	if (!System && !bWarnedMissingSmokeSystem)
	{
		bWarnedMissingSmokeSystem = true;
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] %s has no smoke-wisp Niagara system (%s) - burnt crop will not "
				 "smoulder. The wisp components are still created and positioned, so authoring the "
				 "asset at that path is the only step remaining."),
			*GetName(), *SmokeWispSystem.ToString());
	}

	// Grow explicitly rather than SetNum - a default-constructed int32 is 0, which would silently
	// claim cell 0 instead of reading as unassigned. Same reasoning as VolumeCellIndices.
	while (WispCellIndices.Num() < Wanted)
	{
		WispCellIndices.Add(INDEX_NONE);
	}
	while (WispAges.Num() < WispCellIndices.Num())
	{
		WispAges.Add(0.f);
	}

	TSet<int32> Claimed;
	for (int32& Assigned : WispCellIndices)
	{
		// Burnt is terminal, so this never drops an assignment on its own - which is precisely the
		// problem the reseed pass below exists to solve. Kept anyway so the invariant is enforced
		// by the code rather than assumed by it.
		if (Assigned != INDEX_NONE
			&& Cells.IsValidIndex(Assigned)
			&& Cells[Assigned].State == EGSFieldCellState::Burnt)
		{
			Claimed.Add(Assigned);
		}
		else
		{
			Assigned = INDEX_NONE;
		}
	}

	// ---- reseed pass: let the wake MIGRATE with the front (2026-07-31) -----------------------
	// Sticky assignment plus a terminal Burnt state is a trap, and the first version of this pool
	// walked straight into it: nothing ever released a wisp, so the first MaxSmokeWisps cells to
	// finish burning held all twelve columns for the rest of the raid and the remaining 28 cells
	// of a 5x8 field never smoked at all. The smoke marked where the fire STARTED. Objective 3 is
	// "smoke stays behind after the fire moves on" - the wake has to follow the front, not pin to
	// its origin.
	//
	// An old-enough wisp therefore moves to the MOST RECENTLY BURNT unclaimed cell. Most-recent,
	// not nearest: nearest is the right rule for the damage volumes, which are chasing a front
	// that is already adjacent to them, and the wrong one here, where the entire point is to reach
	// ground the fire has only just left. WispReseedSeconds = 0 disables this entirely.
	if (WispReseedSeconds > 0.f)
	{
		for (int32 Slot = 0; Slot < Wanted; ++Slot)
		{
			if (WispCellIndices[Slot] == INDEX_NONE || WispAges[Slot] < WispReseedSeconds)
			{
				continue;
			}

			// Search BEFORE releasing, so "somewhere newer to stand" is a real precondition rather
			// than something the release itself manufactures - release first and the wisp's own
			// cell becomes an unclaimed candidate and it would happily re-pick itself.
			int32 BestCell = INDEX_NONE;
			int32 BestSequence = -1;
			for (const int32 Candidate : BurntCells)
			{
				if (Claimed.Contains(Candidate))
				{
					continue;
				}
				if (Cells[Candidate].BurntSequence > BestSequence)
				{
					BestSequence = Cells[Candidate].BurntSequence;
					BestCell = Candidate;
				}
			}

			if (BestCell == INDEX_NONE)
			{
				continue; // every razed cell already has a column on it; nothing to move to
			}

			Claimed.Remove(WispCellIndices[Slot]);
			WispCellIndices[Slot] = BestCell;
			WispAges[Slot] = 0.f;
			Claimed.Add(BestCell);
		}
	}

	for (int32 Slot = 0; Slot < Wanted; ++Slot)
	{
		if (WispCellIndices[Slot] == INDEX_NONE)
		{
			// Seed at an evenly-spaced slice of the burnt set, then take the NEAREST UNCLAIMED
			// burnt cell to that seed. The nearest-unclaimed half is the same sticky rule the fire
			// volumes use; the even-slice seed is what spreads the columns across the razed region
			// instead of stacking a dozen of them on whichever cells finished first, which would
			// read as one bonfire rather than as a burnt field.
			const int32 SeedPick = FMath::Clamp(
				(BurntCells.Num() * (2 * Slot + 1)) / (2 * Wanted), 0, BurntCells.Num() - 1);
			const FVector SearchOrigin = GetCellWorldLocation(BurntCells[SeedPick]);

			int32 BestCell = INDEX_NONE;
			float BestDistSq = TNumericLimits<float>::Max();
			for (const int32 Candidate : BurntCells)
			{
				if (Claimed.Contains(Candidate))
				{
					continue;
				}
				const float DistSq = FVector::DistSquared(GetCellWorldLocation(Candidate), SearchOrigin);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					BestCell = Candidate;
				}
			}

			// Unreachable: claimed cells are always fewer than Wanted, and Wanted <= BurntCells.
			// BREAK rather than continue if it ever happens anyway - SmokeWisps and
			// WispCellIndices are parallel and appended in slot order, so skipping a slot would
			// desync them permanently.
			if (BestCell == INDEX_NONE)
			{
				break;
			}

			WispCellIndices[Slot] = BestCell;
			WispAges[Slot] = 0.f;
			Claimed.Add(BestCell);
		}

		const FVector Target = SnapPointToGround(GetCellWorldLocation(WispCellIndices[Slot]));

		if (!SmokeWisps.IsValidIndex(Slot) || !SmokeWisps[Slot])
		{
			// A PLAIN UNiagaraComponent - not an AGSFireVolume. No damage sphere, no point light,
			// no GAS effect. Burnt ground should not keep cooking anything that walks across it,
			// and a dozen more unshadowed lights standing over ash with no flame in it would be
			// both wrong to look at and expensive.
			UNiagaraComponent* Wisp = NewObject<UNiagaraComponent>(this);
			if (!Wisp)
			{
				break;
			}

			// Attach before registering (the runtime-created-component order), so the wisp moves
			// with the field if the actor is ever moved, and dies with it.
			Wisp->SetAutoActivate(false);
			if (USceneComponent* Root = GetRootComponent())
			{
				Wisp->SetupAttachment(Root);
			}
			Wisp->RegisterComponent();
			Wisp->SetWorldLocation(Target);

			// Null-safe: with no asset the component still exists and is still positioned, so the
			// field plays correctly today and assigning NS_GS_Smolder later needs no code change.
			if (System)
			{
				Wisp->SetAsset(System);
				Wisp->Activate(true);
			}

			if (SmokeWisps.IsValidIndex(Slot))
			{
				SmokeWisps[Slot] = Wisp;
			}
			else
			{
				SmokeWisps.Add(Wisp);
			}
		}
		else
		{
			// This is how a re-seeded wisp actually relocates. It is a hard cut rather than a
			// drift, which is acceptable for something this slow and this diffuse - and it happens
			// at most once per WispReseedSeconds per wisp, on ground the player has usually
			// already walked away from. A crossfade would need two components per slot.
			SmokeWisps[Slot]->SetWorldLocation(Target);
		}
	}
}

// ====================================================================== consolidated fire visual

void AGSFieldFireObjective::UpdateConsolidatedFireVisual()
{
	if (!bConsolidateFireVisual)
	{
		return;
	}

	// Same gate as UpdateSmokeWisps, same reason: a bare runtime UNiagaraComponent does not
	// replicate, so it has to be built and driven on every machine that draws, and there is nothing
	// to draw on a dedicated server.
	if (IsNetMode(NM_DedicatedServer))
	{
		return;
	}

	const FBox LocalBounds = GetBurningLocalBounds();
	if (!LocalBounds.IsValid)
	{
		// Nothing burning: fade out and idle rather than destroy. The front can reignite, and
		// destroying/recreating on every flare-up would re-pay the asset load and re-roll the seed
		// for no benefit - same reasoning ConfigurePooled's pooling exists for in the first place.
		if (ConsolidatedFireFX && ConsolidatedFireFX->IsActive())
		{
			ConsolidatedFireFX->Deactivate();
		}
		return;
	}

	// Local space, not world (2026-08-30): GetBurningLocalBounds is already in the field's own
	// rotated frame, and ConsolidatedFireFX is attached to the field's root, so its RelativeScale3D
	// axes ARE the field's local axes - a yawed field gets a box that follows its actual burn front
	// instead of one that's axis-aligned to the world and doesn't line up with it.
	const FVector LocalCenter = LocalBounds.GetCenter();
	const FVector LocalExtent = LocalBounds.GetExtent();
	const FVector TargetWorldLocation =
		SnapPointToGround(GetActorTransform().TransformPosition(LocalCenter));

	// FireVolumeRadius padding: the box is built from CELL CENTRES (GetCellWorldLocation), but the
	// burning ground each cell actually radiates extends FireVolumeRadius past its own centre in
	// every direction - the same padding a single pooled volume's own DamageRadius already
	// contributes at the front's edge. Without it the visual's edge would sit exactly on the
	// outermost burning cells' centres and read as narrower than the fire actually is.
	const float HalfX = FMath::Max(ConsolidatedFireMinHalfExtent, LocalExtent.X + FireVolumeRadius);
	const float HalfY = FMath::Max(ConsolidatedFireMinHalfExtent, LocalExtent.Y + FireVolumeRadius);
	const FVector TargetScale(
		HalfX / ConsolidatedFireAuthoredHalfExtent,
		HalfY / ConsolidatedFireAuthoredHalfExtent,
		FMath::Max(HalfX, HalfY) / ConsolidatedFireAuthoredHalfExtent);

	if (!ConsolidatedFireFX)
	{
		UNiagaraComponent* FireFX = NewObject<UNiagaraComponent>(this);
		if (!FireFX)
		{
			return;
		}

		FireFX->SetAutoActivate(false);
		if (USceneComponent* Root = GetRootComponent())
		{
			FireFX->SetupAttachment(Root);
		}
		FireFX->RegisterComponent();
		ConsolidatedFireFX = FireFX;

		// First frame snaps straight to target instead of chasing from the origin - see
		// ConsolidatedFireCurrentLocation's comment in the header.
		ConsolidatedFireCurrentLocation = TargetWorldLocation;
		ConsolidatedFireCurrentScale = TargetScale;
	}

	const float Alpha = FMath::Clamp(ConsolidatedFireInterpSpeed
		* FMath::Max(0.05f, CosmeticTickInterval), 0.f, 1.f);
	ConsolidatedFireCurrentLocation =
		FMath::Lerp(ConsolidatedFireCurrentLocation, TargetWorldLocation, Alpha);
	ConsolidatedFireCurrentScale = FMath::Lerp(ConsolidatedFireCurrentScale, TargetScale, Alpha);

	ConsolidatedFireFX->SetWorldLocation(ConsolidatedFireCurrentLocation);
	ConsolidatedFireFX->SetRelativeScale3D(ConsolidatedFireCurrentScale);

	if (!ConsolidatedFireFX->IsActive())
	{
		// RESOLVE ONCE, same pattern as ResolvedSmokeSystem - see its comment on why LoadSynchronous
		// must not be re-tried every update against a path that keeps failing.
		if (!ResolvedConsolidatedFireSystem && !bConsolidatedFireSystemResolveFailed)
		{
			ResolvedConsolidatedFireSystem =
				ConsolidatedFireSystem.IsNull() ? nullptr : ConsolidatedFireSystem.LoadSynchronous();
			bConsolidatedFireSystemResolveFailed = (ResolvedConsolidatedFireSystem == nullptr);
		}

		if (!ResolvedConsolidatedFireSystem)
		{
			if (!bWarnedMissingConsolidatedFireSystem)
			{
				bWarnedMissingConsolidatedFireSystem = true;
				UE_LOG(LogTemp, Warning,
					TEXT("[GoblinSiege] %s has no consolidated fire Niagara system (%s) - the field ")
					TEXT("will burn with no flame visual at all now that its pooled volumes' own ")
					TEXT("FireFX is suppressed (bConsolidateFireVisual)."),
					*GetName(), *ConsolidatedFireSystem.ToString());
			}
			return;
		}

		if (ConsolidatedFireFX->GetAsset() != ResolvedConsolidatedFireSystem)
		{
			ConsolidatedFireFX->SetAsset(ResolvedConsolidatedFireSystem);
		}
		ConsolidatedFireFX->Activate(true);
	}
}

// ====================================================================== burn mask (objective 1)
//
// EnsureBurnMask, RedrawBurnMask, BindCropMaterials, RefreshCropMaterialBindings and GetBurnMaskRT
// were all REMOVED on 2026-07-31. The field owns no render target and creates no MIDs; it reports
// cell state to UGSBurnMaskSubsystem and that is the whole of its relationship with the mask.
//
// What is left below is the two hooks that decide WHAT gets splatted and WHEN:
//   HandleCellChangedForFX - one splat per state change, on server and clients alike;
//   CosmeticTick           - the client clock, the char ratchet, the smoke wake, and a re-splat
//                            of every live cell so the transient channels survive the subsystem's
//                            per-flush decay.

void AGSFieldFireObjective::HandleCellChangedForFX(int32 CellIndex, EGSFieldCellState NewState)
{
	// THIS RUNS ON THE SERVER AND ON CLIENTS, and that is load-bearing rather than incidental.
	// OnFieldCellChanged is broadcast by SetCellState on the authority and by OnRep_CellStates on
	// every client, so binding the mask to the delegate - rather than calling it from CellTick,
	// which only the authority runs - is the entire reason a co-op client sees charred crop
	// instead of an untouched golden field with fire standing on it. Easy to break while
	// refactoring; check it first if clients stop marking.
	if (!Cells.IsValidIndex(CellIndex))
	{
		return;
	}

	UGSBurnMaskSubsystem* Mask = UGSBurnMaskSubsystem::Get(this);
	if (!Mask)
	{
		return; // dedicated server, or a world with no mask. Never an error.
	}

	FGSFieldCell& Cell = Cells[CellIndex];

	// R is a DEPOSIT, not a set (see UGSBurnMaskSubsystem::SplatBurn), and that is exactly why
	// this path deposits on ONE transition rather than on all of them (corrected 2026-07-31).
	//
	// What was here: an unconditional splat of CharHigh on every state change - Burning, Burnt,
	// Doused, and the dry-out back to Unburnt - stacked on top of the slices CosmeticTick was
	// already depositing every 0.2 s. Because the mask ADDS, a cell that caught, was doused,
	// dried and was re-torched paid its whole char over again on each of those four edges, so R
	// saturated to full black while the cell was still visibly golden crop with flame on it, and
	// the ramp the slices exist to draw never appeared. The comment claiming the extra deposits
	// "saturate harmlessly" was reasoning about a SET, on a channel that accumulates.
	//
	// What it does now: only the transition to Burnt deposits, and it deposits the REMAINDER
	// needed to take this cell's lifetime contribution to exactly 1.0 - see FGSFieldCell's
	// CharDeposited. That keeps the one behaviour this path was actually needed for (ash blackens
	// the ground the instant the cell finishes, instead of waiting several cosmetic ticks for the
	// slices to catch up) and drops the three that were double-counting. Doused and dry-out
	// deposit no char at all, which is correct: neither event chars anything.
	const bool bDepositChar = (NewState == EGSFieldCellState::Burnt);

	float CharDeposit = 0.f;
	if (bDepositChar)
	{
		CharDeposit = FMath::Clamp(1.f - Cell.CharDeposited, 0.f, 1.f);
		Cell.CharDeposited = FMath::Min(1.f, Cell.CharDeposited + CharDeposit);
	}

	// Both the char and the ember are scaled by the subsystem's overlap compensation, because the
	// splat quad is wider than a cell AND a texel is wider than a cell, so several cells deposit
	// into the same texel every tick - see UGSBurnMaskSubsystem::GetOverlapCompensation for the
	// arithmetic. Uncompensated, one cell's worth of char landed as several.
	//
	// CellSize is passed IN because the cell pitch belongs to the field and the mask has no
	// business owning one, while the rest of the arithmetic - how wide the quad is drawn, how many
	// unreal units a texel is worth - belongs to the mask. Passing it keeps the derivation in one
	// place instead of splitting it across two files.
	//
	// B is deliberately NOT scaled: doused is a flag, not an accumulator, and it is re-splatted
	// from scratch every cosmetic tick.
	const float Compensation = Mask->GetOverlapCompensation(CellSize);

	// No coalescing flag any more: the subsystem QUEUES splats and draws the whole level's worth in
	// one canvas pair, so ten cells catching in the same tick costs ten array appends and still
	// exactly one canvas open. That is strictly better than the old per-field dirty bit, which
	// coalesced only within one field.
	Mask->SplatBurn(
		GetCellWorldLocation(CellIndex),
		CellSize * Mask->GetSplatRadiusCells(),
		CharDeposit * Compensation,
		GetCellIntensity01(CellIndex) * Compensation,
		NewState == EGSFieldCellState::Doused ? 1.f : 0.f);
}

void AGSFieldFireObjective::CosmeticTick()
{
	const float Step = FMath::Max(0.05f, CosmeticTickInterval);

	// A client never runs CellTick, so its BurnSeconds would sit at 0 forever and the ember (G)
	// channel would be permanently flat - burning crop that never glows. BurnSeconds is server
	// business and rightly not replicated, so on a client it is advanced here as a purely local
	// cosmetic clock, zeroed by OnRep_CellStates as each cell catches and clamped so it can never
	// run past the end of the arc while waiting for the Burnt state to replicate in.
	//
	// This is an ESTIMATE, and it is allowed to be: it drives a 0..1 glow curve and nothing else.
	// The authority's simulation is untouched by it.
	if (!HasAuthority())
	{
		for (FGSFieldCell& Cell : Cells)
		{
			if (Cell.State == EGSFieldCellState::Burning)
			{
				Cell.BurnSeconds = FMath::Min(Cell.BurnSeconds + Step, CellBurnSeconds);

				// Same ratchet as the server's pass 4. This is the side of the split that actually
				// motivated CharHigh: OnRep_CellStates zeroes BurnSeconds above every time a cell
				// re-enters Burning, so a doused-then-re-torched cell used to visibly un-char here.
				if (CellBurnSeconds > 0.f)
				{
					Cell.CharHigh = FMath::Max(Cell.CharHigh,
						FMath::Clamp(Cell.BurnSeconds / CellBurnSeconds, 0.f, 1.f));
				}
			}
		}
	}

	// The smoke wake rides this timer rather than CellTick (2026-07-31). See UpdateSmokeWisps in
	// the header: the wisps are unreplicated runtime components, so they have to be built on every
	// machine that draws them, and this is the timer that runs on every machine that draws. It is
	// also already skipped on a dedicated server, which is where the wisps' ground traces and
	// asset loads were pure waste. Called BEFORE the early-out below, because a razed field with
	// nothing left burning is exactly when the wake matters most.
	//
	// Age the wisps by the same Step for the same reason it advances BurnSeconds here: this
	// function is the only clock the cosmetic layer has.
	for (float& Age : WispAges)
	{
		Age += Step;
	}
	UpdateSmokeWisps();

	// Same machine set as UpdateSmokeWisps and for the same reason - see UpdateConsolidatedFireVisual.
	UpdateConsolidatedFireVisual();

	// ---- re-publish the mask's LIVE cells (2026-07-31, world-mask re-architecture) -------------
	//
	// A field with nothing burning and nothing wet costs two integer compares. This replaced
	// "redraw on the dirty flag, and steadily while anything is alight" and keeps the same two
	// jobs, by a different mechanism:
	//
	//   G and B are TRANSIENT on the world mask. The subsystem cannot erase a channel on an
	//   additive canvas, so it fades G and B a little on every flush; anything that wants a live
	//   ember or a live wet patch has to keep saying so. That is what this loop is - the ember arc
	//   still animates for exactly the reason it did before (GetCellIntensity01 changes as the
	//   cell burns), it is just pushed rather than re-rendered.
	//
	//   R is accumulated in SLICES. Depositing Step / CellBurnSeconds per tick means a cell that
	//   burns for its full CellBurnSeconds deposits exactly 1.0 in total, so the char ramps over
	//   the burn instead of snapping - and because deposits only ever add, the monotonic guarantee
	//   is preserved by construction. A cell that is doused part way keeps what it deposited,
	//   which is the same "the brigade saves the crop, it does not un-scorch it" rule the old
	//   full redraw expressed through CharHigh.
	if (BurningCellCount == 0 && DousedCellCount == 0)
	{
		return;
	}

	UGSBurnMaskSubsystem* Mask = UGSBurnMaskSubsystem::Get(this);
	if (!Mask)
	{
		return;
	}

	const float SplatRadius = CellSize * Mask->GetSplatRadiusCells();
	const float CharSlice = CellBurnSeconds > 0.f ? Step / CellBurnSeconds : 1.f;

	// THE OVERLAP DIVISOR (2026-07-31), and it is the difference between a 22 s char ramp and a
	// 3 s one. Several cells stamp the same texel every tick and every one of them was depositing
	// a FULL cell's slice into it, so R integrated to that many instead of to 1 - at the old 1.35
	// cell radius, 7.3x, so a cell hit full black in the first three seconds of a 22 s burn and
	// the ramp was never visible at all. The same over-deposit pinned the ember channel to 1.0
	// across the whole burning band, flattening GetCellIntensity01's trapezoid into a slab and
	// hiding the arc it exists to draw.
	//
	// TWO causes, both divided out by the one helper: the splat quad is wider than a cell, and a
	// texel is wider than a cell (270 uu cells under ~390 uu texels at 1024^2 over Tutorial_Island,
	// so ~2 cells share a texel no matter how small the quad gets). Correcting only the first left
	// R still integrating to ~2 and the char still ramping in half the burn.
	//
	// Derived once by the subsystem, which is the only thing that knows how wide it draws the quad
	// and how many unreal units a texel is worth; CellSize is ours, so it goes in as an argument.
	// See UGSBurnMaskSubsystem::GetOverlapCompensation for the full arithmetic and worked numbers.
	const float Compensation = Mask->GetOverlapCompensation(CellSize);

	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		FGSFieldCell& Cell = Cells[i];

		if (Cell.State == EGSFieldCellState::Burning)
		{
			// Deposit against the cell's running total rather than blindly - see CharDeposited.
			// Both paths into the mask draw from the same budget of exactly 1.0 per cell, so the
			// slices here and the top-up in HandleCellChangedForFX can never double-count each
			// other, however many times the cell is doused and re-torched.
			const float Slice = FMath::Clamp(FMath::Min(CharSlice, 1.f - Cell.CharDeposited), 0.f, 1.f);
			Cell.CharDeposited = FMath::Min(1.f, Cell.CharDeposited + Slice);

			Mask->SplatBurn(GetCellWorldLocation(i), SplatRadius,
				Slice * Compensation, GetCellIntensity01(i) * Compensation, 0.f);
		}
		else if (Cell.State == EGSFieldCellState::Doused)
		{
			// No char deposit: wet crop is not charring. B only, so the water reads on the field
			// and fades out on its own once the cell dries and stops re-splatting.
			Mask->SplatBurn(GetCellWorldLocation(i), SplatRadius, 0.f, 0.f, 1.f);
		}
	}
}

float AGSFieldFireObjective::GetBurntFraction() const
{
	// Only Burnt counts. Doused cells count as unburnt, per spec 3.3 - a firebreak denies the
	// player progress, which is the whole point of the defenders' verb.
	//
	// The denominator is BURNABLE cells, not every cell (2026-09-09). Water cells can never reach
	// Burnt by any route, so leaving them in the divisor would silently cap the achievable
	// fraction: GS_MillField is 3.4% river, and at a 70% completion threshold that is not yet
	// fatal - but it is the same class of bug as a field that crosses a wider stream and simply
	// never finishes however long the player burns it. Gating ignition without fixing this is
	// half a change.
	return BurnableCellCount > 0
		? static_cast<float>(BurntCellCount) / static_cast<float>(BurnableCellCount)
		: 0.f;
}

int32 AGSFieldFireObjective::DryAllDousedCells()
{
	if (!HasAuthority())
	{
		return 0;
	}

	int32 Dried = 0;
	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		if (Cells[i].State == EGSFieldCellState::Doused)
		{
			Cells[i].DousedSeconds = 0.f;
			SetCellState(i, EGSFieldCellState::Unburnt);
			++Dried;
		}
	}
	return Dried;
}

float AGSFieldFireObjective::GetDousedFraction() const
{
	return Cells.Num() > 0 ? static_cast<float>(DousedCellCount) / static_cast<float>(Cells.Num()) : 0.f;
}

// ====================================================================== the jump

void AGSFieldFireObjective::TryJumpToAdjacentFlammables(int32 CellIndex)
{
	UWorld* World = GetWorld();
	if (!World || JumpRadius <= 0.f)
	{
		return;
	}

	if (FMath::FRand() > JumpChance)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSFieldFireJump), false, this);

	const bool bAnyHit = World->OverlapMultiByObjectType(
		Overlaps,
		GetCellWorldLocation(CellIndex),
		FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
		FCollisionShape::MakeSphere(JumpRadius),
		Params);

	if (!bAnyHit)
	{
		return;
	}

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
			// Ignite() enforces its own rules - resistance, already-burning, already-ash, and
			// crucially bCanBeLitBySpread, so a field fire can take a fence but cannot complete
			// the mill or the market on the player's behalf.
			Flammable->Ignite();
		}
	}
}

// ====================================================================== debug draw

void AGSFieldFireObjective::DrawDebugState() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Half = CellSize * 0.4f; // slight gap so the grid reads as cells, not a solid slab
	const FVector Extent(Half, Half, 20.f);

	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		// Water first, above state, because a water cell has no interesting state - it is always
		// Unburnt and always will be, and drawing it as dry crop is exactly the picture that would
		// hide a mis-probed river. Deep blue against Doused's lighter wet-crop blue.
		if (Cells[i].bIsWater)
		{
			DrawDebugSolidBox(World, GetCellWorldLocation(i), Extent, FColor(10, 40, 130), false, 0.3f, 0);
			continue;
		}

		FColor Colour;
		switch (Cells[i].State)
		{
		case EGSFieldCellState::Burning: Colour = FColor::Orange;      break;
		case EGSFieldCellState::Burnt:   Colour = FColor(40, 40, 40);  break;
		case EGSFieldCellState::Doused:  Colour = FColor(60, 130, 220); break; // wet = blue
		default:
			// Dry crop, tinted green -> yellow by accumulated Heat (2026-07-31). This is the only
			// way to actually SEE the new spread model while tuning it: the front's shape, its
			// depth, whether wind is biasing it and whether decay is holding it back are all
			// properties of the heat field, and none of them is visible in cell STATE alone. If
			// the fire looks wrong, look at the gradient ahead of it before touching a number.
			Colour = FLinearColor::LerpUsingHSV(
				FLinearColor(0.12f, 0.47f, 0.12f),
				FLinearColor(1.f, 0.95f, 0.f),
				FMath::Clamp(Cells[i].Heat, 0.f, 1.f)).ToFColor(false);
			break;
		}

		DrawDebugSolidBox(World, GetCellWorldLocation(i), Extent, Colour, false, 0.3f, 0);
	}

	// Headline numbers float above the middle of the field.
	const FString Readout = FString::Printf(
		TEXT("%s  %.0f%% burnt (need %.0f%%)  burning:%d  doused:%d  wisps:%d  mask:%s"),
		*GetName(), GetBurntFraction() * 100.f, CompletionThreshold01 * 100.f,
		BurningCellCount, DousedCellCount, SmokeWisps.Num(),
		// "mask:on" now means the WORLD mask exists (2026-07-31). It reads off on a dedicated
		// server, where the subsystem is deliberately never created, and on nothing else - so an
		// "off" here on a client or a listen server is a genuine finding, not a per-field setting.
		UGSBurnMaskSubsystem::Get(this) ? TEXT("on") : TEXT("off"));

	DrawDebugString(World, GetActorLocation() + FVector(0, 0, 400.f), Readout,
		nullptr, IsComplete() ? FColor::Green : FColor::White, 0.3f, true);

	// The pooled damage volumes - their actual damage radii, so you can see whether a pawn is
	// really inside one or just near the flames.
	for (const TObjectPtr<AGSFireVolume>& Volume : DamageVolumes)
	{
		if (Volume)
		{
			DrawDebugSphere(World, Volume->GetActorLocation(), Volume->GetDamageRadius(), 16,
				FColor(255, 80, 0), false, 0.3f, 0, 2.f);
		}
	}

	if (BurningCellCount > 0)
	{
		DrawDebugSphere(World, GetBurningCentroid(), 60.f, 12, FColor::Red, false, 0.3f, 0, 3.f);
	}

	// Live HP over every character in the level. Lives here because the field owns the burn overlay
	// and there is one field per test map; if a second field is ever added this will double-draw.
	for (TActorIterator<AGSCharacterBase> It(World); It; ++It)
	{
		AGSCharacterBase* Character = *It;
		if (!Character)
		{
			continue;
		}

		const float HP = Character->GetHealth();
		const float MaxHP = Character->GetMaxHealth();
		const bool bHurt = MaxHP > 0.f && HP < MaxHP;

		DrawDebugString(World, Character->GetActorLocation() + FVector(0, 0, 120.f),
			FString::Printf(TEXT("%s  %.0f/%.0f%s"), *Character->GetName(), HP, MaxHP,
				Character->IsAlive() ? TEXT("") : TEXT("  DEAD")),
			nullptr,
			!Character->IsAlive() ? FColor::Red : (bHurt ? FColor::Orange : FColor::White),
			0.3f, true);
	}
}

// ====================================================================== dousing

int32 AGSFieldFireObjective::DouseAtLocation(const FVector& WorldLocation, float Radius)
{
	if (!HasAuthority() || Radius <= 0.f)
	{
		return 0;
	}

	const float RadiusSq = Radius * Radius;
	int32 Doused = 0;

	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		// PRE-WETTING (2026-07-31, Q-35). Unburnt crop is now dousable as well as burning crop.
		//
		// Until today this function converted ONLY cells already in Burning, and that single
		// condition is what made decision 16's Option C bucket brigade decorative. A firebreak is
		// only worth anything if it spans a whole column - the Moore kernel reaches +-1, so a gap
		// of one cell lets the front straight through - and under heat diffusion the cells of a
		// column light at different times and burn for CellBurnSeconds each. Requiring all five
		// cells of a column to be simultaneously alight in order to wet them is not merely hard,
		// it is very nearly impossible: the front is typically four cells DEEP and one cell wide
		// per column, so the top of a column is usually ash by the time the bottom catches. The
		// defenders' one verb could therefore never actually produce the thing it exists to
		// produce.
		//
		// Letting the brigade wet crop AHEAD of the fire is what the verb was always meant to be -
		// a bucket line soaks the ground the fire is coming towards, it does not throw water into
		// the middle of a blaze. It also finally makes the brigade's positioning matter: Option C
		// says the brigade is effective at the EDGES, and edges are exactly where the crop is
		// still Unburnt.
		//
		// Burnt cells are still refused below. Ash never re-wets and, more to the point, Burnt is
		// the terminal state the completion fraction counts, so allowing it would let defenders
		// walk back progress the goblins have already banked - a different rule entirely, and not
		// one anybody has ruled on.
		//
		// THE EXPLOIT IS ALREADY BOUNDED, and by an existing knob rather than a new one:
		// DousedDryOutSeconds is 25 s, so a pre-wetted cell that the fire has not yet reached
		// dries back to Unburnt on its own. A brigade cannot pre-soak the whole field and park it -
		// it has to keep re-wetting the same ground, which is precisely the "stops THIS front, now"
		// tactical shape DousedDryOutSeconds was tuned for (see it for the arithmetic on why
		// permanent firebreaks can make the 70% completion rule unreachable).
		//
		// Reviewed 2026-07-31 and confirmed: a full-height doused column genuinely does stop the
		// front, because the Moore neighbourhood reaches only +-1 in each axis and pass 2 refuses
		// to deposit heat into a Doused cell at all. There is no diagonal that skips a column, so
		// a contiguous wet column is a real barrier for as long as it stays wet - which is what
		// makes the brigade worth doing now that building one is achievable.
		const EGSFieldCellState CellState = Cells[i].State;
		if (CellState != EGSFieldCellState::Burning && CellState != EGSFieldCellState::Unburnt)
		{
			continue;
		}

		if (FVector::DistSquared(GetCellWorldLocation(i), WorldLocation) <= RadiusSq)
		{
			// Doused, NOT Unburnt. The cell becomes a firebreak that spread cannot cross, while
			// remaining re-lightable by a direct torch. BurnSeconds is kept, so re-torching it
			// finishes faster than starting fresh.
			Cells[i].DousedSeconds = 0.f; // restart the dry-out clock on a fresh soaking
			SetCellState(i, EGSFieldCellState::Doused);
			++Doused;
		}
	}

	// Still the count of cells ACTUALLY CHANGED, not of cells in range - callers (the brigade BT,
	// the debug commands) use it to tell "the bucket did something" from "the bucket hit ash", and
	// widening what qualifies must not quietly turn it into a range count. Cells already Doused
	// are skipped by the state filter above, so re-soaking wet ground correctly reports 0.
	return Doused;
}

// ====================================================================== replication

void AGSFieldFireObjective::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGSFieldFireObjective, ReplicatedCellStates);

	// GRID SHAPE, COND_InitialOnly (2026-07-31).
	//
	// These three were unreplicated, on the reasonable-sounding grounds that they are level-design
	// data. They are also read on CLIENTS by everything the cosmetic layer does - since the
	// 2026-07-31 world-mask change that means every splat position (GetCellWorldLocation) and every
	// splat radius (CellSize) the field hands the subsystem - so for a field the hamlet generator
	// SPAWNS at runtime, rather than one placed in the level, a client held the class defaults
	// (5x8 @ 270) while the server ran the real shape. Clients then marked the WRONG GROUND, which
	// on a world-space mask is visible anywhere in the level rather than merely misaligned within
	// one field. Worse still, EnsureGridAllocated
	// sized Cells from the client's stale Rows*Columns and could wipe the array OnRep_CellStates
	// had just filled.
	//
	// InitialOnly rather than plain DOREPLIFETIME because grid shape genuinely never changes after
	// spawn: paying for three properties in every subsequent update of a field that is already
	// replicating a per-cell byte array would be paying for nothing.
	DOREPLIFETIME_CONDITION(AGSFieldFireObjective, Rows, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AGSFieldFireObjective, Columns, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AGSFieldFireObjective, CellSize, COND_InitialOnly);
}
