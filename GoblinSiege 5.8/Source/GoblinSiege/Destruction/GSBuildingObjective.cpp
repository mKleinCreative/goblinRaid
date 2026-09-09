#include "Destruction/GSBuildingObjective.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Destruction/GSFlammableComponent.h"
#include "Destruction/GSCrumbleComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "Kismet/GameplayStatics.h"
#include "Destruction/GSBurnFXComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Core/GSGameState.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSBuilding, Log, All);

AGSBuildingObjective::AGSBuildingObjective()
{
	PrimaryActorTick.bCanEverTick = false;

	// A house is mostly gone once two thirds of it has burned; holding out for every last floor
	// plank would leave a blackened shell reading as "not done". Same reasoning as the market's
	// 0.75, one notch more forgiving because a building has far more small pieces than a market
	// has stalls, and the awkward ones are floor planks under furniture.
	CompletionThreshold01 = 0.66f;

	// Name-based classification, because the kit carries no metadata to ask. Both lists are
	// EditAnywhere so a designer can correct a misfiled piece without a recompile.
	EntryNameFilters.Add(TEXT("Window"));
	EntryNameFilters.Add(TEXT("Roof"));

	RoofNameFilters.Add(TEXT("Roof"));

	// A piece that IS a whole building - walls, roof and windows baked into one mesh. This map's
	// ordinary houses are all of these (SM_MERGED_House_*), and they own no roof piece to aim at, so
	// their roof is a REGION of their own bounds instead. Name-based like every other filter here,
	// because the kit carries no metadata to ask.
	MonolithicNameFilters.Add(TEXT("MERGED"));

	// See InteriorNameFilters in the header. "Beam" is in here because the roof beams are structural
	// interior geometry - they are named Roof_Beam and would otherwise be counted as roof.
	InteriorNameFilters.Add(TEXT("Interior"));
	InteriorNameFilters.Add(TEXT("Floor"));
	InteriorNameFilters.Add(TEXT("Stair"));
	InteriorNameFilters.Add(TEXT("Ceiling"));
	InteriorNameFilters.Add(TEXT("Beam"));

	// Soft paths: these are real assets today (Content/VFX), but soft-loading keeps a level holding
	// eleven buildings from pulling in Niagara nobody has lit yet.
	FireSystem = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(TEXT("/Game/VFX/NS_GS_SurfaceFire.NS_GS_SurfaceFire")));
	SmokeColumnSystem = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(TEXT("/Game/VFX/NS_GS_SmokeColumn.NS_GS_SmokeColumn")));

	// Generic collapse fallback (#390-adjacent, 2026-08-31) for the 41 of 42 `SM_MERGED_House_*`
	// meshes with no matching GC_ fracture asset - see SpawnGenericRubbleFallback. Reused rather than
	// authored: N_PebbleDust is the project's existing generic debris-burst system
	// (EnvironmentVFX/VFX/EnvironmentDust), and the debris cue is the existing rock-impact sound bank.
	GenericRubbleFXAsset = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(
		TEXT("/Game/EnvironmentVFX/VFX/EnvironmentDust/Systems/N_PebbleDust.N_PebbleDust")));
	GenericRubbleSoundAsset = TSoftObjectPtr<USoundBase>(FSoftObjectPath(
		TEXT("/Game/NaPH_RPG_Fantasy_Sounds_Bunle/Environment/cue/SC_Rock_Large_Debris_2-1_Cue."
			 "SC_Rock_Large_Debris_2-1_Cue")));

	PieceNameFilters.Add(TEXT("House"));
	PieceNameFilters.Add(TEXT("Roof"));
	PieceNameFilters.Add(TEXT("Wall"));
	PieceNameFilters.Add(TEXT("Window"));
	PieceNameFilters.Add(TEXT("Door"));
	PieceNameFilters.Add(TEXT("Foundation"));
	PieceNameFilters.Add(TEXT("Barn"));
	PieceNameFilters.Add(TEXT("Tavern"));
}

void AGSBuildingObjective::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGSBuildingObjective, bAlight);
}

void AGSBuildingObjective::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	AdoptPieces();
	EnsurePiecesFlammable();

	if (InitialPieceCount == 0)
	{
		// The same failure the market had, and it is silent everywhere else: a building that adopted
		// nothing can never complete, and looks identical to one that simply has not been lit.
		UE_LOG(LogGSBuilding, Error,
			TEXT("[GoblinSiege] Building '%s' adopted ZERO pieces within %.0f uu. It can never burn ")
			TEXT("or complete. Check AdoptRadius and PieceNameFilters against the kit meshes here."),
			*GetName(), AdoptRadius);
	}
	else
	{
		// Report against the SHELL, because that is what RecomputeCompletion divides by. This line
		// used to say CeilToInt(InitialPieceCount * Threshold), which on a kit that is ~28% interior
		// over-reported the requirement by about 40% on every building - the one line a designer
		// reads to sanity-check a house was describing a denominator nothing uses.
		const int32 ShellCount = CountShellPieces();
		UE_LOG(LogGSBuilding, Log,
			TEXT("[GoblinSiege] Building '%s' adopted %d piece(s) within %.0f uu, %d of them shell; ")
			TEXT("needs %d shell piece(s) burnt (%.0f%%)."),
			*GetName(), InitialPieceCount, AdoptRadius, ShellCount,
			FMath::CeilToInt(ShellCount * CompletionThreshold01), CompletionThreshold01 * 100.f);

		if (ShellCount == 0)
		{
			// Reachable today: InteriorNameFilters contains "Beam", so a detached roof-beam cluster
			// adopts pieces, counts more than zero, and scores nothing. Without this the building
			// just freezes at its last completion value forever with no line in the log - the same
			// silent-dead-objective class as the InitialPieceCount == 0 case above, which has an
			// Error precisely because it has burned this project before.
			UE_LOG(LogGSBuilding, Error,
				TEXT("[GoblinSiege] Building '%s' adopted %d piece(s) but NONE of them are shell - ")
				TEXT("every one matched InteriorNameFilters. Its completion can never move. Check ")
				TEXT("InteriorNameFilters (note it contains \"Beam\") against this structure's meshes."),
				*GetName(), InitialPieceCount);
		}
	}
}

// ====================================================================== adoption

void AGSBuildingObjective::AdoptPieces()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin = GetActorLocation();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Other = *It;
		if (!Other || Other == this || Other->IsA<AGSBurnObjectiveBase>())
		{
			continue;
		}

		// Measure to the piece's GEOMETRY, not its pivot.
		//
		// This kit offsets its meshes from their actor origin by a median of 287 uu and up to 671
		// (measured over 1,000 house pieces on L_Tutorial_Island). Testing GetActorLocation() was
		// therefore asking "is this pivot near me", which is a different question from "is this wall
		// part of my house" - and the answer diverged badly: 76 of 113 windows ended up owned by no
		// building, so breaking them did nothing at all.
		//
		// Subtracting the piece's own bounding radius means a large wall counts as adopted if ANY of
		// it is inside the footprint, which is what "part of this building" actually means.
		//
		// That is the intent; the arithmetic used to overshoot it. `Dist(centre, centre) -
		// PieceExtent.Size()` subtracts the box DIAGONAL - sqrt(x^2+y^2+z^2) - which is the radius of
		// the sphere the box is inscribed in, not the box's reach toward this building. On a roof
		// piece with extent (700, 700, 200) that is 1005 rather than 700, so the piece was treated as
		// starting ~300 uu closer than it does, in EVERY direction at once. The class header names an
		// over-large adopt radius as a route to an unwinnable objective, and it also inflates the
		// completion denominator with pieces from next door.
		//
		// This is the exact distance from the building centre to the nearest point of the piece's
		// box, which is zero when the centre is inside it.
		FVector PieceOrigin, PieceExtent;
		Other->GetActorBounds(false, PieceOrigin, PieceExtent);
		const FVector Beyond = (Origin - PieceOrigin).GetAbs() - PieceExtent;
		const float EdgeDistance = FVector(FMath::Max(Beyond.X, 0.f),
										   FMath::Max(Beyond.Y, 0.f),
										   FMath::Max(Beyond.Z, 0.f)).Size();
		if (EdgeDistance > AdoptRadius)
		{
			continue;
		}

		// Match on the MESH name, not the actor label. Labels are whatever the level author typed;
		// the mesh is what the art actually is, and it is the same signal the market's stall filter
		// and the burn mask's crop filter already rely on.
		UStaticMeshComponent* MeshComp = Other->FindComponentByClass<UStaticMeshComponent>();
		if (!MeshComp || !MeshComp->GetStaticMesh())
		{
			continue;
		}

		const FString MeshName = MeshComp->GetStaticMesh()->GetName();
		bool bMatches = PieceNameFilters.Num() == 0;
		for (const FString& Filter : PieceNameFilters)
		{
			if (!Filter.IsEmpty() && MeshName.Contains(Filter))
			{
				bMatches = true;
				break;
			}
		}

		if (!bMatches)
		{
			continue;
		}

		// FOLIAGE IS NEVER PART OF A BUILDING, WHATEVER THE FILTERS SAY.
		//
		// This is the vanishing-trees bug (2026-09-09), and it deleted the entire island's foliage.
		// Michael: "it's almost always after the Inn burns down that the trees disappear."
		//
		// AInstancedFoliageActor holds UFoliageInstancedStaticMeshComponents, which derive from
		// UStaticMeshComponent - so the check above finds one and GetStaticMesh() happily returns
		// SM_Tree_Apple_01. Its bounds span the whole map (measured: extent 63076 x 64242 x 13051 on
		// L_Tutorial_Island), so the edge-distance test returns 0 for every building and it sits
		// inside ANY AdoptRadius. With PieceNameFilters EMPTY - which means "adopt everything", and
		// which exactly one building uses (GSBuildingObjective_293, the Inn, cleared in #395 so it
		// would adopt its own furniture) - it matched, and the Inn adopted all 280,153 foliage
		// instances as a single "piece". CrumblePieces then calls Piece->Destroy() on it.
		//
		// One actor, one Destroy(), every tree and every stalk of wheat on the island gone at once.
		// That is why it was always the Inn, and why nothing about the trees themselves ever looked
		// wrong: instance counts, visibility, cull distances and materials were all still correct
		// right up until the actor holding them stopped existing.
		//
		// A hard exclusion rather than a name filter, because "do not eat the world's foliage" is not
		// a per-building tuning decision.
		if (MeshComp->IsA<UInstancedStaticMeshComponent>())
		{
			continue;
		}

		// The same mistake generalised: anything dramatically bigger than this building cannot be a
		// part of it. A candidate whose own bounds out-reach the adopt radius by this much is a
		// landscape, a sky sphere, a river spline or another world-sized actor that happens to carry
		// a mesh - and adopting one costs the whole level rather than one wrong wall.
		//
		// Checked AFTER the name filter on purpose: before it, this warned about every world-sized
		// actor near every building, including the ~67 that PieceNameFilters was already rejecting
		// on its own. A warning that fires where there was never a bug teaches people to ignore it.
		FVector GuardOrigin, GuardExtent;
		Other->GetActorBounds(false, GuardOrigin, GuardExtent);
		const float PieceReach = static_cast<float>(GuardExtent.GetMax());
		if (AdoptRadius > 0.f && PieceReach > AdoptRadius * OversizePieceRejectRatio)
		{
			UE_LOG(LogGSBuilding, Warning,
				TEXT("[GoblinSiege] '%s' refused to adopt '%s': its extent (%.0f) is more than %.0fx ")
				TEXT("this building's AdoptRadius (%.0f). A piece that size is the world, not a wall."),
				*GetName(), *Other->GetName(), PieceReach, OversizePieceRejectRatio, AdoptRadius);
			continue;
		}

		Pieces.Add(Other);
	}

	InitialPieceCount = Pieces.Num();

	// Which KIND of building is this? It decides how a torch gets in, and it is answered by the
	// pieces themselves rather than by a flag someone has to remember to set.
	//
	// Kitbashed -> it owns roof actors, so the roof is a thing you can hit.
	// Merged     -> it does not, so its roof is the top of its own bounds. See ContainsWorldLocation.
	// How can a torch get into this building? Answered from the pieces themselves rather than a flag
	// someone has to remember to set - and reported, because a building with NEITHER kind of entry is
	// an objective the player cannot complete and nothing else would say so.
	int32 RoofPieceCount = 0;
	int32 MonolithicCount = 0;
	for (const TWeakObjectPtr<AActor>& Weak : Pieces)
	{
		const AActor* Piece = Weak.Get();
		if (IsRoofPiece(Piece))
		{
			++RoofPieceCount;
		}
		if (IsMonolithicPiece(Piece))
		{
			++MonolithicCount;
		}
	}

	if (RoofPieceCount == 0 && MonolithicCount == 0)
	{
		UE_LOG(LogGSBuilding, Warning,
			TEXT("[GoblinSiege] Building '%s' has NO WAY IN - %d piece(s), no roof piece and no "
				 "monolithic mesh. A torch cannot light it. Check MonolithicNameFilters and "
				 "RoofNameFilters against the kit meshes here."),
			*GetName(), Pieces.Num());
	}
	else
	{
		UE_LOG(LogGSBuilding, Log,
			TEXT("[GoblinSiege] Building '%s': %d piece(s), %d roof piece(s), %d monolithic mesh(es)."),
			*GetName(), Pieces.Num(), RoofPieceCount, MonolithicCount);
	}
}

void AGSBuildingObjective::EnsurePiecesFlammable()
{
	for (const TWeakObjectPtr<AActor>& Weak : Pieces)
	{
		AActor* Piece = Weak.Get();
		if (!Piece)
		{
			continue;
		}

		UGSFlammableComponent* Flam = Piece->FindComponentByClass<UGSFlammableComponent>();
		if (!Flam)
		{
			// Kit pieces are plain StaticMeshActors and cannot catch on their own. AddInstanceComponent
			// is what makes this survive a level save; without it the house looks dressed until the
			// next reload. Same lesson as UGSRaidLibrary::MakeActorFlammable.
			Flam = NewObject<UGSFlammableComponent>(Piece, UGSFlammableComponent::StaticClass(),
				TEXT("GSFlammable_Building"), RF_Transactional);
			if (Flam)
			{
				Piece->AddInstanceComponent(Flam);
				Flam->RegisterComponent();
			}
		}

		if (Flam)
		{
			// Give the piece this building's reach. Applied to pieces that ALREADY had a flammable
			// component too, not just ones we just created - a stall or cart standing inside the
			// adopt radius is part of this house as far as fire is concerned.
			if (PieceSpreadRadius > 0.f)
			{
				Flam->SetSpreadRadius(PieceSpreadRadius);
			}

			Flam->OnBurnedDown.AddDynamic(this, &AGSBuildingObjective::HandlePieceBurnedDown);
			Flam->OnIgnited.AddDynamic(this, &AGSBuildingObjective::HandlePieceIgnited);
			PieceFlammables.Add(Flam);
		}

		// Char and smoulder. UGSBurnFXComponent finds the sibling flammable itself and binds to its
		// ignite/extinguish/burned-down delegates, so this is the whole of "the house blackens" -
		// which is exactly why the first pass showed nothing: the pieces got a flammable component
		// and no FX component, so they burned correctly and looked untouched.
		if (!Piece->FindComponentByClass<UGSBurnFXComponent>())
		{
			UGSBurnFXComponent* FX = NewObject<UGSBurnFXComponent>(Piece, UGSBurnFXComponent::StaticClass(),
				TEXT("GSBurnFX_Building"), RF_Transactional);
			if (FX)
			{
				Piece->AddInstanceComponent(FX);
				FX->RegisterComponent();
			}
		}
	}
}

// ====================================================================== ignition

namespace
{
	/** Does this actor's static mesh name match any of these substrings? */
	bool MeshNameMatches(const AActor* Piece, const TArray<FString>& Filters)
	{
		if (!Piece)
		{
			return false;
		}

		const UStaticMeshComponent* MeshComp = Piece->FindComponentByClass<UStaticMeshComponent>();
		if (!MeshComp || !MeshComp->GetStaticMesh())
		{
			return false;
		}

		const FString MeshName = MeshComp->GetStaticMesh()->GetName();
		for (const FString& Filter : Filters)
		{
			if (!Filter.IsEmpty() && MeshName.Contains(Filter))
			{
				return true;
			}
		}
		return false;
	}
}

bool AGSBuildingObjective::IsEntryPiece(const AActor* Piece) const
{
	return MeshNameMatches(Piece, EntryNameFilters);
}

bool AGSBuildingObjective::IsRoofPiece(const AActor* Piece) const
{
	// A roof BEAM is interior structure, not roof you can throw a torch onto.
	return MeshNameMatches(Piece, RoofNameFilters) && !IsInteriorPiece(Piece);
}

bool AGSBuildingObjective::IsInteriorPiece(const AActor* Piece) const
{
	return MeshNameMatches(Piece, InteriorNameFilters);
}

bool AGSBuildingObjective::IsMonolithicPiece(const AActor* Piece) const
{
	return MeshNameMatches(Piece, MonolithicNameFilters);
}

bool AGSBuildingObjective::ContainsWorldLocation(const FVector& WorldLocation) const
{
	// A MERGED piece has no roof ACTOR to throw a torch at (2026-08-06). This map's ordinary houses
	// are single SM_MERGED_House_* meshes - walls, roof and windows baked into one actor - so
	// IsRoofPiece can never be true for them and there is no window actor to break either. Refusing
	// here would leave nine buildings in ten unlightable, which is the exact bug this feature exists
	// to fix.
	//
	// For those the roof is not an actor, it is a REGION: the top of the mesh's own bounds. That keeps
	// the ignition rule intact rather than weakening it - a torch into the wall still fails, because
	// the wall is the lower two thirds.
	//
	// Asked PER PIECE, not per building. Gating this on "the building owns no roof piece at all" was
	// wrong and PIE proved it: 14 of the 61 merged houses adopt a stray roof tile from a neighbouring
	// shed, which flipped them to kitbashed and left them with no way in - a torch on their own roof
	// did nothing. A building can hold both kinds, and each piece answers for itself.
	for (const TWeakObjectPtr<AActor>& Weak : Pieces)
	{
		const AActor* Piece = Weak.Get();
		if (!Piece || IsInteriorPiece(Piece) || !IsMonolithicPiece(Piece))
		{
			continue;
		}

		FVector Origin, Extent;
		Piece->GetActorBounds(false, Origin, Extent);
		if (Extent.IsNearlyZero())
		{
			continue;
		}

		const bool bInFootprint =
			FMath::Abs(WorldLocation.X - Origin.X) <= Extent.X &&
			FMath::Abs(WorldLocation.Y - Origin.Y) <= Extent.Y;
		const float RoofFloorZ =
			Origin.Z + Extent.Z * (1.f - 2.f * FMath::Clamp(RoofZoneFraction, 0.05f, 0.9f));

		if (bInFootprint && WorldLocation.Z >= RoofFloorZ && WorldLocation.Z <= Origin.Z + Extent.Z)
		{
			return true;
		}
	}

	return false;
}

void AGSBuildingObjective::IgniteAtLocation(const FVector& WorldLocation)
{
	// Only ever reached for a merged building whose roof region contains this point - see above. A
	// kitbashed building returns false from ContainsWorldLocation and never gets here, so exterior
	// fire on a wall is still refused.
	if (ContainsWorldLocation(WorldLocation))
	{
		IgniteInterior(EGSBuildingIgnitionSource::Roof);
	}
}

void AGSBuildingObjective::IgniteInterior(EGSBuildingIgnitionSource Source)
{
	if (!HasAuthority() || bAlight || IsComplete())
	{
		return;
	}

	bAlight = true;
	IgnitionSource = Source;
	OnRep_Alight();

	// Light the entry pieces themselves and let UGSFlammableComponent's spread carry it through the
	// rest of the house. Lighting every piece at once would be cheaper to write and completely wrong
	// to watch: a house should go up from where the torch landed, not everywhere simultaneously.
	int32 Seeded = 0;
	for (const TWeakObjectPtr<UGSFlammableComponent>& Weak : PieceFlammables)
	{
		UGSFlammableComponent* Flam = Weak.Get();
		if (!Flam)
		{
			continue;
		}

		if (IsEntryPiece(Flam->GetOwner()))
		{
			Flam->Ignite();
			++Seeded;
		}
	}

	// A building with no roof or window pieces still has to be lightable once something has decided
	// it should burn - otherwise a torch through the window achieves nothing and says nothing.
	if (Seeded == 0)
	{
		for (const TWeakObjectPtr<UGSFlammableComponent>& Weak : PieceFlammables)
		{
			if (UGSFlammableComponent* Flam = Weak.Get())
			{
				Flam->Ignite();
				++Seeded;
				break;
			}
		}
	}

	UE_LOG(LogGSBuilding, Log, TEXT("[GoblinSiege] Building '%s' is alight (source %d), seeded %d piece(s)."),
		*GetName(), static_cast<int32>(Source), Seeded);

	// The distant tell. One column per building, at its centre, so a burning house is findable from
	// the treeline - GDD 2.1 wants a fire to read from a mile off, and per-piece flames do not carry
	// past about a hundred metres.
	if (!SmokeColumn)
	{
		if (UNiagaraSystem* Smoke = SmokeColumnSystem.LoadSynchronous())
		{
			SmokeColumn = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Smoke,
				GetActorLocation(), FRotator::ZeroRotator);
		}
		else
		{
			UE_LOG(LogGSBuilding, Warning,
				TEXT("[GoblinSiege] '%s' has no smoke column asset - a burning house will be hard to "
					 "spot from range."), *GetName());
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (AGSGameState* GS = World->GetGameState<AGSGameState>())
		{
			GS->AddAlarm(AlarmOnIgnite, EGSAlarmSource::FireDamage);
			GS->ReportFireStarted();
		}
	}

	OnBuildingIgnited.Broadcast();
}

void AGSBuildingObjective::HandlePieceIgnited()
{
	// Which piece? The delegate carries no sender, so light whichever burning piece is not yet
	// showing flames. Cheap - the cap keeps this loop short and it only runs on ignition events.
	for (const TWeakObjectPtr<UGSFlammableComponent>& Weak : PieceFlammables)
	{
		if (ActiveFireFX.Num() >= MaxFireFX)
		{
			return;
		}

		UGSFlammableComponent* Flam = Weak.Get();
		if (Flam && Flam->IsBurning())
		{
			SpawnFireFXOn(Flam->GetOwner());
		}
	}
}

void AGSBuildingObjective::SpawnFireFXOn(AActor* Piece)
{
	if (!Piece || ActiveFireFX.Num() >= MaxFireFX)
	{
		return;
	}

	// Already burning visibly? Do not stack a second system on the same piece.
	for (const TObjectPtr<UNiagaraComponent>& Existing : ActiveFireFX)
	{
		if (Existing && Existing->GetOwner() == Piece)
		{
			return;
		}
	}

	UNiagaraSystem* Fire = FireSystem.LoadSynchronous();
	if (!Fire)
	{
		return;
	}

	// Attached, so the flame follows the piece and dies with it rather than hanging in the air
	// after the wall it belonged to has gone.
	UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAttached(
		Fire, Piece->GetRootComponent(), NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget, true);
	if (Comp)
	{
		ActiveFireFX.Add(Comp);
	}
}

void AGSBuildingObjective::OnRep_Alight()
{
	// Presentation hook only; the Blueprint decides what a burning house looks like.
}

// ====================================================================== progress

AActor* AGSBuildingObjective::SpawnCollectionProxy(AActor* Piece) const
{
	// WHY A SPAWNED ACTOR AND NOT A COMPONENT ON THE PIECE (2026-08-26, after four builds).
	//
	// The first version grafted a UGeometryCollectionComponent onto the kit piece, which is a plain
	// StaticMeshActor. It failed four separate ways, each hiding the next, and every one of them
	// reported success in the log while doing nothing:
	//
	//   1. Static root mobility - SetSimulatePhysics(true) accepted, logged "simulating 0".
	//   2. Still attached       - driven by the parent transform, "simulating 1" and motionless.
	//   3. Collision profile    - defaults to Custom, so pieces fell through the landscape.
	//   4. All three fixed      - 31 of 33 pieces still ended 1.3 km away and below the world.
	//
	// Meanwhile the IDENTICAL asset folds correctly in L_CombatArena, where it sits on a plain
	// AGeometryCollectionActor placed in the editor. The difference is not the asset and not the
	// release sequence: Chaos builds a collection's proxy once, at registration, from the actor it
	// belongs to. A collection wants to BE an actor's root, not be bolted onto one.
	//
	// So spawn the shape that works and hide the original piece. Deferred spawn matters: the rest
	// collection must be assigned BEFORE FinishSpawningActor, or the component registers empty and
	// reports zero transforms until something re-registers it.
	if (!IsValid(Piece))
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UStaticMeshComponent* MeshComp = Piece->FindComponentByClass<UStaticMeshComponent>();
	UStaticMesh* Mesh = MeshComp ? MeshComp->GetStaticMesh() : nullptr;
	if (!Mesh)
	{
		return nullptr;
	}

	// SM_MERGED_House_Small_03 becomes GC_MERGED_House_Small_03. Anything not following the kit's
	// SM_ prefix simply gets GC_ prepended, so a one-off mesh can still be given a fracture.
	FString Base = Mesh->GetName();
	if (Base.StartsWith(TEXT("SM_")))
	{
		Base.RightChopInline(3);
	}
	const FString AssetName = FString::Printf(TEXT("GC_%s"), *Base);
	const FString FullPath = FString::Printf(TEXT("%s/%s.%s"), *CrumbleCollectionFolder, *AssetName, *AssetName);

	// Silent on miss. This fires for every unfractured piece of every building on the map, and a
	// warning here would bury the one line that matters in CrumblePieces.
	UGeometryCollection* Collection = LoadObject<UGeometryCollection>(nullptr, *FullPath);
	if (!Collection)
	{
		return nullptr;
	}

	// REFUSE AN EMPTY COLLECTION. GC_House_Window_C is 10 KB of scaffolding that nothing referenced.
	// Used anyway it produced the worst possible failure: the swap ran, the log said the collection
	// was revealed, the intact mesh was hidden, and the player got a hole where a window used to be.
	if (Collection->NumElements(FGeometryCollection::TransformGroup) <= 1)
	{
		UE_LOG(LogGSBuilding, Warning,
			TEXT("[GoblinSiege] %s skipped %s for piece %s: the asset has no geometry (%d transforms). ")
			TEXT("It is a stub, not a fracture."),
			*GetName(), *AssetName, *Piece->GetName(),
			Collection->NumElements(FGeometryCollection::TransformGroup));
		return nullptr;
	}

	const FTransform Xform = Piece->GetActorTransform();

	AGeometryCollectionActor* Proxy = World->SpawnActorDeferred<AGeometryCollectionActor>(
		AGeometryCollectionActor::StaticClass(), Xform,
		const_cast<AGSBuildingObjective*>(this), nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Proxy)
	{
		return nullptr;
	}

	if (UGeometryCollectionComponent* GCC = Proxy->GetGeometryCollectionComponent())
	{
		GCC->SetRestCollection(Collection);

		// Dormant until released. A collection left at its default Chaos_Object_Dynamic falls over and
		// shatters itself at level start - #192 recorded that as the feature working before Michael
		// pointed out he had never touched it.
		GCC->ObjectType = EObjectStateTypeEnum::Chaos_Object_Static;
		GCC->SetCollisionProfileName(TEXT("Destructible"));
		GCC->SetHiddenInGame(true);
	}

	// Replicated, unlike the StaticMeshActor it stands in for - which never replicated, and so could
	// never have shown a client anything at all.
	Proxy->SetReplicates(true);

	UGameplayStatics::FinishSpawningActor(Proxy, Xform);

	if (UGeometryCollectionComponent* GCC = Proxy->GetGeometryCollectionComponent())
	{
		GCC->SetSimulatePhysics(false);
	}

	UE_LOG(LogGSBuilding, Verbose,
		TEXT("[GoblinSiege] %s spawned a %s proxy for piece %s."),
		*GetName(), *AssetName, *Piece->GetName());

	return Proxy;
}

void AGSBuildingObjective::SpawnGenericRubbleFallback(AActor* Piece, bool bPlayFX) const
{
	if (!IsValid(Piece))
	{
		return;
	}

	// REVERTED TO NON-PHYSICS (2026-08-31, #393). The physics-topple version of this function is
	// gone, not tuned - it failed two different ways in two different live tests:
	//   1. On a MERGED house (one big mesh): a strong enough impulse to visibly rotate a house-scale
	//      body read as the entire building launching into the air, not collapsing. Michael: "why in
	//      gods green earth did you think the entire house popping up would be a good idea."
	//   2. On individual KITBASHED trim pieces (SM_House_Roof_01_*, SM_House_Wall_5x4_*, etc.): these
	//      ship with 'Use Complex Collision As Simple', which the physics engine categorically cannot
	//      simulate on - confirmed live, every single call logged "Trying to simulate physics on ...
	//      but it has ComplexAsSimple collision" and did nothing, leaving a piece that both never
	//      moved AND had no collision response (Michael: "it has no collision, so I can just walk
	//      through it").
	// #393's bulk fracture generation covers MERGED houses only, not kitbashed trim pieces - this
	// function is reached constantly for a kitbashed building's hundreds of pieces, not rarely. Every
	// piece is still hidden/removed here regardless of bPlayFX (see CrumblePieces' MaxRubbleFXPerBuilding
	// cap) - only the dust/sound burst is bounded, since THAT is what measured at 577 simultaneous
	// Niagara instances and ~70ms of game-thread time (2026-09-01, "it's really laggy right now").
	for (UStaticMeshComponent* M : TInlineComponentArray<UStaticMeshComponent*>(Piece))
	{
		if (M)
		{
			M->SetHiddenInGame(true);
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	if (bPlayFX)
	{
		const FTransform PieceTransform = Piece->GetActorTransform();

		if (UWorld* World = GetWorld())
		{
			if (!GenericRubbleFXAsset.IsNull())
			{
				if (UNiagaraSystem* Dust = GenericRubbleFXAsset.LoadSynchronous())
				{
					UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Dust,
						PieceTransform.GetLocation(), PieceTransform.Rotator());
				}
			}

			if (!GenericRubbleSoundAsset.IsNull())
			{
				if (USoundBase* Sound = GenericRubbleSoundAsset.LoadSynchronous())
				{
					UGameplayStatics::PlaySoundAtLocation(World, Sound, PieceTransform.GetLocation());
				}
			}
		}
	}

	UE_LOG(LogGSBuilding, Verbose,
		TEXT("[GoblinSiege] %s removed piece %s (no fracture asset for its mesh yet)."),
		*GetName(), *Piece->GetName());

	Piece->Destroy();
}

void AGSBuildingObjective::HandleCompleted()
{
	Super::HandleCompleted();

	// Server decides; the pieces' own replicated releases carry it to the clients.
	if (HasAuthority())
	{
		CrumblePieces();
	}
}

void AGSBuildingObjective::CrumblePieces()
{
	int32 Released = 0;
	int32 NoAsset = 0;
	int32 RubbleFXSpawned = 0;

	for (const TWeakObjectPtr<AActor>& Weak : Pieces)
	{
		AActor* Piece = Weak.Get();
		if (!IsValid(Piece))
		{
			continue;
		}

		AActor* Proxy = SpawnCollectionProxy(Piece);
		if (!Proxy)
		{
			++NoAsset;
			// Capped, not per-piece - see MaxRubbleFXPerBuilding's header comment. A kitbashed
			// building's hundreds of un-fractured trim pieces each spawning their own dust/sound cue
			// measured live at 577 simultaneous Niagara instances and ~70ms of game-thread time.
			const bool bPlayFX = RubbleFXSpawned < MaxRubbleFXPerBuilding;
			if (bPlayFX)
			{
				++RubbleFXSpawned;
			}
			SpawnGenericRubbleFallback(Piece, bPlayFX);
			continue;
		}

		// Retire the standing piece. Collision off as well as hidden: an invisible house you still bump
		// into reads as the collapse having failed, and the rubble would land on walls that are gone.
		for (UStaticMeshComponent* M : TInlineComponentArray<UStaticMeshComponent*>(Piece))
		{
			if (M)
			{
				M->SetHiddenInGame(true);
				M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}

		UGSCrumbleComponent* Crumble = UGSCrumbleComponent::FindOrAdd(Proxy);
		if (!Crumble)
		{
			continue;
		}

		// A building gives way; it is not shoved over. Hand the proxy the collapse shape, then pass a
		// ZERO impulse - the ring replaces it.
		Crumble->ClusterCrumblePasses = CollapseClusterPasses;
		Crumble->CollapseShoveCount = CollapseShoveCount;
		Crumble->CollapseShoveMagnitude = CollapseShoveMagnitude;
		Crumble->CollapseInwardRatio = CollapseInwardRatio;

		// A house that burned to the ground drops BLACK rubble. The proxy is a freshly spawned
		// collection with pristine materials - nothing ever burned it - so the burn state has to be
		// carried across the swap or the wreckage arrives clean. Michael caught this on the windmill
		// ("it didn't have the char on it") and it is the identical bug here.
		Crumble->CharAmountOnRelease = 1.f;

		// A collapsing roof/wall should be able to kill, same reasoning as the mill's falling cap
		// (2026-08-30, Michael: "can we have it so the flying geometry causes death?"). This ring's
		// own shove magnitude (CollapseShoveMagnitude, default 5,000,000) is well clear of
		// UGSCrumbleComponent's damage floor.
		Crumble->bEnableDamageFromCollision = true;

		if (Crumble->Crumble(FVector::ZeroVector, Proxy->GetActorLocation()))
		{
			++Released;
		}
	}

	// One line, and it has to be readable by somebody who has not read this file. "0 of 34" is the
	// answer to why a house did not fall down with a REAL fracture, and it points at the missing
	// asset, not at the code - the other 34 still visibly break, just via the generic rubble fallback
	// rather than a fitted Chaos collection.
	UE_LOG(LogGSBuilding, Log,
		TEXT("[GoblinSiege] %s burnt out: released %d of %d piece(s) with a real fracture; %d used the ")
		TEXT("generic rubble fallback (no GC_ asset for their mesh yet)."),
		*GetName(), Released, Pieces.Num(), NoAsset);
}

void AGSBuildingObjective::HandlePieceBurnedDown()
{
	RecomputeCompletion();
}

void AGSBuildingObjective::RecomputeCompletion()
{
	if (!HasAuthority() || InitialPieceCount <= 0)
	{
		return;
	}

	// Score the SHELL, not the interior. A player judges a burning house from outside, and the
	// interior is 28% of the kit - so counting it meant a third of the work was invisible, and a
	// house could sit at "not done" while visibly gutted. Interiors still burn; they just do not
	// gate the objective.
	int32 Burnt = 0, Shell = 0;
	for (const TWeakObjectPtr<UGSFlammableComponent>& Weak : PieceFlammables)
	{
		const UGSFlammableComponent* Flam = Weak.Get();
		if (!Flam || IsInteriorPiece(Flam->GetOwner()))
		{
			continue;
		}
		++Shell;
		if (Flam->HasBurnedDown())
		{
			++Burnt;
		}
	}

	BurntPieceCount = Burnt;
	if (Shell > 0)
	{
		SetCompletion01(static_cast<float>(Burnt) / static_cast<float>(Shell));
	}
	else if (!bWarnedNoShell)
	{
		// Not silent. Skipping SetCompletion01 freezes completion at whatever it last held, forever,
		// with nothing in the log to say why - a dead objective that reads exactly like one nobody
		// has lit yet. BeginPlay reports this too; this catches the case where the shell disappears
		// later (every shell piece destroyed and unregistered), which BeginPlay cannot see.
		bWarnedNoShell = true;
		UE_LOG(LogGSBuilding, Error,
			TEXT("[GoblinSiege] Building '%s' has no shell pieces left to score (%d adopted, all ")
			TEXT("interior or gone). Completion is frozen at %.2f and cannot advance."),
			*GetName(), InitialPieceCount, GetCompletion01());
	}
}

int32 AGSBuildingObjective::CountShellPieces() const
{
	int32 Shell = 0;
	for (const TWeakObjectPtr<UGSFlammableComponent>& Weak : PieceFlammables)
	{
		const UGSFlammableComponent* Flam = Weak.Get();
		if (Flam && !IsInteriorPiece(Flam->GetOwner()))
		{
			++Shell;
		}
	}
	return Shell;
}

// ====================================================================== lookup

AGSBuildingObjective* AGSBuildingObjective::FindBuildingOwning(const UObject* WorldContextObject, AActor* Piece)
{
	if (!WorldContextObject || !Piece)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Ask the buildings, rather than storing a back-pointer on 1,413 kit pieces. Buildings are tens
	// of actors per level; the same trade AGSBurnObjectiveBase::FindObjectiveAtLocation already made.
	for (TActorIterator<AGSBuildingObjective> It(World); It; ++It)
	{
		AGSBuildingObjective* Building = *It;
		if (!Building)
		{
			continue;
		}

		for (const TWeakObjectPtr<AActor>& Weak : Building->Pieces)
		{
			if (Weak.Get() == Piece)
			{
				return Building;
			}
		}
	}
	return nullptr;
}
