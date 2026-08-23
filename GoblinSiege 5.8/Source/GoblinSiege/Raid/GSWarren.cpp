#include "Raid/GSWarren.h"
#include "Raid/GSRaidLibrary.h"
#include "Raid/GSLootBankComponent.h"
#include "Raid/GSScoreSubsystem.h"
#include "Interaction/GSCarryComponent.h"
#include "Interaction/GSInteractableComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSWarren, Log, All);

AGSWarren::AGSWarren()
{
	PrimaryActorTick.bCanEverTick = false;
	// Replicated for the same reason AGSRunicSite is: a co-op partner must see the same rune in the
	// same place. There is no replicated STATE yet - "placed, not planted" left this actor with no
	// open/closed flag to replicate - and that is a deliberate absence, not an oversight.
	bReplicates = true;
	SetReplicateMovement(false);

	WarrenRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WarrenRoot"));
	SetRootComponent(WarrenRoot);

	BankingSphere = CreateDefaultSubobject<USphereComponent>(TEXT("BankingSphere"));
	BankingSphere->SetupAttachment(WarrenRoot);
	BankingSphere->SetSphereRadius(BankingRadius);
	// QueryOnly, and overlapping BOTH pawns and world-dynamic bodies. The runic site only ever
	// needed pawns; this circle also has to notice a sack a courier put down, which is not a pawn.
	BankingSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BankingSphere->SetCollisionObjectType(ECC_WorldDynamic);
	BankingSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	BankingSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	BankingSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	BankingSphere->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	BankingSphere->SetGenerateOverlapEvents(true);

	MouthMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MouthMesh"));
	MouthMesh->SetupAttachment(WarrenRoot);
	// Scenery. Goblins climb OUT of this and the player walks INTO it; collision here would mean a
	// mouth nobody can enter.
	MouthMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	LootBank = CreateDefaultSubobject<UGSLootBankComponent>(TEXT("LootBank"));
	LootBank->BankLabel = TEXT("warren");

	WarrenFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("WarrenFX"));
	WarrenFX->SetupAttachment(WarrenRoot);
	// ON, unlike AGSRunicSite::PortalFX. The portal must stay dark until it opens, because a blazing
	// portal tells the player the exit is ready when it is not. The Warren has no closed state to
	// misreport, and an unlit one is a landmark the player cannot find.
	WarrenFX->bAutoActivate = true;
}

void AGSWarren::BeginPlay()
{
	Super::BeginPlay();

	BankingSphere->SetSphereRadius(BankingRadius);

	// Re-broadcast, so anything already bound to AGSWarren::OnLootBanked keeps working. The
	// migration must be invisible from outside this class.
	if (LootBank)
	{
		LootBank->OnLootBanked.AddDynamic(this, &AGSWarren::HandleLootBanked);
	}

	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogGSWarren, Log,
		TEXT("[GoblinSiege] Warren '%s' open at (%.0f, %.0f, %.0f). Arrival mouth: %s. Respawn: %s. Banks loot: %s."),
		*GetName(), GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z,
		bIsArrivalMouth ? TEXT("yes") : TEXT("no"),
		bIsRespawnPoint ? TEXT("yes") : TEXT("no"),
		bBanksLoot ? TEXT("yes") : TEXT("no"));

	// Bank anything already lying in the mouth at level start. Not defensive padding: a designer
	// dressing the Warren with a sack, or a courier delivering on the frame the level streams in,
	// both produce cargo that never fires a BeginOverlap.
	if (bBanksLoot)
	{
		TArray<AActor*> Overlapping;
		BankingSphere->GetOverlappingActors(Overlapping);
		for (AActor* Actor : Overlapping)
		{
			BankLooseActor(Actor);
		}
	}
}

// ====================================================================== lookup

namespace
{
	/** Shared body of the three FindNearest flavours. Filter is applied before distance, so a
	 *  banking-only Warren can never win an arrival query by being closer. */
	template <typename TPredicate>
	AGSWarren* FindNearestMatching(const UObject* WorldContextObject, const FVector& From, TPredicate&& Predicate)
	{
		const UWorld* World = GEngine
			? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
			: nullptr;
		if (!World)
		{
			return nullptr;
		}

		AGSWarren* Best = nullptr;
		float BestDistSq = TNumericLimits<float>::Max();
		for (TActorIterator<AGSWarren> It(const_cast<UWorld*>(World)); It; ++It)
		{
			AGSWarren* Warren = *It;
			if (!IsValid(Warren) || !Predicate(Warren))
			{
				continue;
			}

			const float DistSq = FVector::DistSquared(Warren->GetActorLocation(), From);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Warren;
			}
		}
		return Best;
	}
}

AGSWarren* AGSWarren::FindNearest(const UObject* WorldContextObject, FVector From)
{
	return FindNearestMatching(WorldContextObject, From, [](const AGSWarren*) { return true; });
}

AGSWarren* AGSWarren::FindNearestArrivalMouth(const UObject* WorldContextObject, FVector From)
{
	return FindNearestMatching(WorldContextObject, From,
		[](const AGSWarren* W) { return W->IsArrivalMouth(); });
}

AGSWarren* AGSWarren::FindNearestRespawnPoint(const UObject* WorldContextObject, FVector From)
{
	return FindNearestMatching(WorldContextObject, From,
		[](const AGSWarren* W) { return W->IsRespawnPoint(); });
}

// ====================================================================== transforms

FTransform AGSWarren::GetArrivalTransform() const
{
	const FRotator Facing(0.f, GetActorRotation().Yaw, 0.f);

	// UGSRaidLibrary::FindStandableSpotNear, not a third copy of the trace. Its header says why it
	// was extracted out of AGSRunicSite in the first place, and #054 already recorded the mistake of
	// restating it inline. Deliberately NOT navmesh projection - GEN_NavBounds_Village is 4000x4000uu
	// and projection fails across most of the map.
	FVector Spot;
	if (UGSRaidLibrary::FindStandableSpotNear(this, GetActorLocation(), Spot, this))
	{
		return FTransform(Facing, Spot);
	}

	UE_LOG(LogGSWarren, Warning,
		TEXT("[GoblinSiege] Warren '%s' found no standable ground for an arrival. Goblins will climb ")
		TEXT("out on the actor itself - if that is inside geometry, the Warren is placed badly."),
		*GetName());
	return FTransform(Facing, GetActorLocation());
}

FTransform AGSWarren::GetSpawnTransform() const
{
	// Same search as the arrival. They are separate functions because they are separate DECISIONS -
	// a Warren that wanted respawns pushed clear of the mouth would change one and not the other -
	// and collapsing them now would make that change a signature edit instead of a body edit.
	return GetArrivalTransform();
}

// ====================================================================== banking

int32 AGSWarren::GetPointsBankedHere() const
{
	return LootBank ? LootBank->GetPointsBankedHere() : 0;
}

void AGSWarren::HandleLootBanked(AActor* Source, int32 Points)
{
	// Straight re-broadcast. The component owns the rule; this actor owns the delegate Blueprints
	// were already bound to, and that contract must survive the implementation moving.
	OnLootBanked.Broadcast(Source, Points);
}

void AGSWarren::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (!HasAuthority() || !bBanksLoot || !IsValid(OtherActor))
	{
		return;
	}

	// A pawn banks what it is CARRYING, not itself. Checked first because a courier goblin carrying
	// a pig is both a pawn and, in principle, a loose actor - and swallowing the courier would be a
	// spectacular way to lose the horde.
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		BankCarriedLoot(Pawn);
		return;
	}

	BankLooseActor(OtherActor);
}

int32 AGSWarren::BankCarriedLoot(APawn* Pawn)
{
	// The bBanksLoot switch stays HERE rather than moving into the component: it is a property of
	// this Warren ("a decorative rune that takes nothing"), not of banking as an idea. The runic
	// site has no equivalent and should not inherit one.
	if (!bBanksLoot || !LootBank)
	{
		return 0;
	}
	return LootBank->BankCarriedLoot(Pawn);
}

int32 AGSWarren::BankLooseActor(AActor* Actor)
{
	if (!bBanksLoot || !LootBank)
	{
		return 0;
	}
	return LootBank->BankLooseActor(Actor);
}

FString AGSWarren::DescribeStatus() const
{
	return FString::Printf(
		TEXT("%s at (%.0f, %.0f, %.0f) - arrival:%s respawn:%s banks:%s - %d loot from %d item(s)"),
		*GetName(), GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z,
		bIsArrivalMouth ? TEXT("Y") : TEXT("N"),
		bIsRespawnPoint ? TEXT("Y") : TEXT("N"),
		bBanksLoot ? TEXT("Y") : TEXT("N"),
		GetPointsBankedHere(), LootBank ? LootBank->GetItemsBankedHere() : 0);
}
