#include "Destruction/GSFlammableComponent.h"
#include "Core/GSGameState.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"

UGSFlammableComponent::UGSFlammableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Already here before 2026-07-31 and doing nothing on its own: this only registers the
	// component with its owner's replication list. Until Q-36 added GetLifetimeReplicatedProps
	// below, the component replicated ZERO properties, so a remote client never saw a fire start
	// and UGSBurnFXComponent - which does all its work from this component's delegates - only ever
	// charred anything on the host.
	SetIsReplicatedByDefault(true);
}

void UGSFlammableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 2026-07-31 (Q-36) - THE CHEAP CO-OP ROOT. Two bools, unconditional.
	//
	// Scope is deliberately exactly this: BurnedSeconds, bEnableSmoke and FireIntensity01 are
	// explicitly NOT here, deferred until co-op is scheduled. See BurnedSeconds in the header for
	// why the continuous values are a different, larger decision than the transitions.
	DOREPLIFETIME(UGSFlammableComponent, bIsBurning);
	DOREPLIFETIME(UGSFlammableComponent, bBurnedDown);
}

void UGSFlammableComponent::OnRep_BurningState()
{
	// CLIENTS ONLY. The server already broadcast this locally inside Ignite()/Extinguish(), and a
	// LISTEN SERVER runs both paths in the same process - the authority code AND, without this
	// guard, its own RepNotify. That would double-fire OnIgnited on the host and only the host,
	// which is the nastiest shape of co-op bug: it works in a client build, works on a dedicated
	// server, and misbehaves exactly where a designer playtests. Downstream that means two smolder
	// systems on one prop, and AGSSpawnerActor's OnBurnedDown handler running twice.
	//
	// GetOwnerRole() rather than a world netmode check: it is the property that actually decides
	// whether this instance is the authority for this object, so it stays correct for anything
	// exotic (a replay, a client-authoritative prop) that a netmode test would get wrong.
	if (GetOwnerRole() == ROLE_Authority)
	{
		return;
	}

	// One bool, two transitions. Ash cannot re-ignite, so a late burst that delivers bIsBurning
	// true after bBurnedDown has already latched must not announce a fresh fire - the OnRep order
	// between two properties in the same bunch is not something to rely on.
	if (bIsBurning)
	{
		if (!bBurnedDown)
		{
			OnIgnited.Broadcast();
		}
		return;
	}

	// bIsBurning went false. That is either an Extinguish or the burn completing; only the former
	// is this delegate's business, and OnRep_BurnedDown handles the latter - both bools change in
	// BurnTick's burn-down path, so without this test a completed burn would fire OnExtinguished
	// AND OnBurnedDown on every client, and UGSBurnFXComponent would take the freeze-the-char path
	// on the way past.
	if (!bBurnedDown)
	{
		OnExtinguished.Broadcast();
	}
}

void UGSFlammableComponent::OnRep_BurnedDown()
{
	// Clients only - see OnRep_BurningState. BurnTick() already broadcast this on the authority.
	if (GetOwnerRole() == ROLE_Authority)
	{
		return;
	}

	// One-way latch on the server, so this can only ever mean "it finished".
	if (bBurnedDown)
	{
		OnBurnedDown.Broadcast();
	}
}

void UGSFlammableComponent::Ignite()
{
	// FireResistance >= 1 means "doesn't burn" (stone barracks path). A thing that already
	// finished burning is ash and stays ash.
	if (bIsBurning || bBurnedDown || FireResistance >= 1.f)
	{
		return;
	}

	bIsBurning = true;
	SecondsSinceSpreadAttempt = 0.f;
	OnIgnited.Broadcast();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(BurnTimerHandle, this,
		&UGSFlammableComponent::BurnTick, BurnTickInterval, true);

	// Arm the town's unseen-fire fuse (decision 19). Idempotent on the GameState side - the
	// first fire of the raid arms it and later fires don't restart it.
	if (AGSGameState* GS = World->GetGameState<AGSGameState>())
	{
		GS->ReportFireStarted();
	}
	// Fire FX attachment is a Blueprint concern bound to OnIgnited - keeps this component
	// presentation-free.
}

void UGSFlammableComponent::Extinguish()
{
	if (!bIsBurning)
	{
		return;
	}

	bIsBurning = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BurnTimerHandle);
	}
	// Progress deliberately NOT reset: a re-ignited half-burned objective finishes faster -
	// rewards re-torching what the defenders saved (decision 26, doused cells are re-ignitable).
	OnExtinguished.Broadcast();
}

void UGSFlammableComponent::BurnTick()
{
	BurnedSeconds += BurnTickInterval * (1.f - FireResistance);

	if (BurnedSeconds >= BurnDurationSeconds)
	{
		bIsBurning = false;
		bBurnedDown = true;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BurnTimerHandle);
		}
		OnBurnedDown.Broadcast();
		return;
	}

	if (!bCanSpread)
	{
		return;
	}

	// Only a well-established fire jumps. Keeps a glancing torch from chaining the village.
	if (GetBurnProgress01() < SpreadAtProgress01)
	{
		return;
	}

	SecondsSinceSpreadAttempt += BurnTickInterval;
	if (SecondsSinceSpreadAttempt >= SpreadAttemptInterval)
	{
		SecondsSinceSpreadAttempt = 0.f;
		TrySpread();
	}
}

void UGSFlammableComponent::SetSpreadRadius(float NewRadius)
{
	if (NewRadius > 0.f)
	{
		SpreadRadius = NewRadius;
	}
}

void UGSFlammableComponent::TrySpread()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || SpreadRadius <= 0.f)
	{
		return;
	}

	// Server owns fire propagation; clients see it through the component's replicated state and
	// the cosmetic OnIgnited hook.
	if (!Owner->HasAuthority())
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSFireSpread), false, Owner);
	Params.bReturnPhysicalMaterial = false;

	const bool bAnyHit = World->OverlapMultiByObjectType(
		Overlaps,
		Owner->GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
		FCollisionShape::MakeSphere(SpreadRadius),
		Params);

	if (!bAnyHit)
	{
		return;
	}

	// Dedupe: one actor can return several overlapping primitives.
	TSet<AActor*> Considered;
	Considered.Reserve(Overlaps.Num());

	for (const FOverlapResult& Result : Overlaps)
	{
		AActor* Other = Result.GetActor();
		if (!Other || Other == Owner)
		{
			continue;
		}

		bool bAlreadySeen = false;
		Considered.Add(Other, &bAlreadySeen);
		if (bAlreadySeen)
		{
			continue;
		}

		UGSFlammableComponent* Neighbour = Other->FindComponentByClass<UGSFlammableComponent>();
		if (!Neighbour || Neighbour == this)
		{
			continue;
		}

		if (!Neighbour->bCanBeLitBySpread || Neighbour->IsBurning() || Neighbour->HasBurnedDown())
		{
			continue;
		}

		// Hard material gate first - stone never catches no matter how the dice land.
		if (Neighbour->GetFireResistance() >= MaxNeighbourResistanceToCatch)
		{
			continue;
		}

		// Then the roll, biased by how flammable the neighbour is: thatch catches far more
		// eagerly than damp timber.
		const float Chance = SpreadChance * (1.f - Neighbour->GetFireResistance());
		if (FMath::FRand() <= Chance)
		{
			Neighbour->Ignite();
		}
	}
}

void UGSFlammableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BurnTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}
