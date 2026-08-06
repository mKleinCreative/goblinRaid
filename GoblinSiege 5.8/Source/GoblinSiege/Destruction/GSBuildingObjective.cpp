#include "Destruction/GSBuildingObjective.h"
#include "Destruction/GSFlammableComponent.h"
#include "Destruction/GSBurnFXComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Core/GSGameState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

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
		UE_LOG(LogGSBuilding, Log,
			TEXT("[GoblinSiege] Building '%s' adopted %d piece(s) within %.0f uu; needs %d burnt (%.0f%%)."),
			*GetName(), InitialPieceCount, AdoptRadius,
			FMath::CeilToInt(InitialPieceCount * CompletionThreshold01), CompletionThreshold01 * 100.f);
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
		FVector PieceOrigin, PieceExtent;
		Other->GetActorBounds(false, PieceOrigin, PieceExtent);
		const float EdgeDistance = FVector::Dist(PieceOrigin, Origin) - PieceExtent.Size();
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

		if (bMatches)
		{
			Pieces.Add(Other);
		}
	}

	InitialPieceCount = Pieces.Num();
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

bool AGSBuildingObjective::ContainsWorldLocation(const FVector& /*WorldLocation*/) const
{
	// Never. A building owns no ground, so the torch's FindObjectiveAtLocation sweep cannot light a
	// house by splashing its outside wall. Getting in is the window's job and the roof's job, and
	// routing it through those keeps the rule in one place instead of two.
	return false;
}

void AGSBuildingObjective::IgniteAtLocation(const FVector& /*WorldLocation*/)
{
	// Intentionally empty - see the header. Exterior fire is refused.
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
