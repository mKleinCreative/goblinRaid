#include "Destruction/GSTopplableComponent.h"

#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Raid/GSScoreSubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTopple, Log, All);

UGSTopplableComponent::UGSTopplableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGSTopplableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGSTopplableComponent, bToppled);
}

FText UGSTopplableComponent::GetObjectiveDisplayName() const
{
	if (!ObjectiveDisplayName.IsEmpty())
	{
		return ObjectiveDisplayName;
	}

	// Derived from the tag leaf, the same way the HUD pluralises burn types: a fifth monument type
	// should cost a tag and nothing else. Objective.Topple.Statue -> "Statue".
	if (ObjectiveTypeTag.IsValid())
	{
		FString Leaf = ObjectiveTypeTag.ToString();
		int32 Dot = INDEX_NONE;
		if (Leaf.FindLastChar(TEXT('.'), Dot))
		{
			Leaf = Leaf.RightChop(Dot + 1);
		}
		return FText::FromString(Leaf);
	}

	return NSLOCTEXT("GoblinSiege", "MonumentObjectiveDefault", "Monument");
}

bool UGSTopplableComponent::WardReaches(const FVector& Spot) const
{
	if (bWardsEntireLevel)
	{
		return true;
	}

	const AActor* Monument = GetOwner();
	if (!Monument || WardRadius <= 0.f)
	{
		return false;
	}

	// 2D, deliberately. A monument in a graveyard below the hamlet it guards would otherwise let the
	// player climb the slope and plant a gate 900 uu away on the map but "outside" the ward purely on
	// height.
	return FVector::DistSquared2D(Monument->GetActorLocation(), Spot) <= FMath::Square(WardRadius);
}

UGeometryCollectionComponent* UGSTopplableComponent::ResolveCollection() const
{
	AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UGeometryCollectionComponent>() : nullptr;
}

bool UGSTopplableComponent::Topple(AActor* Toppler, const FVector& PullDirection, const FVector& AnchorPoint)
{
	// A monument comes down once. Not defensive - the haul ticks every frame and would otherwise
	// re-push a statue that is already mid-fall, which reads as it being swatted.
	if (bToppled)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	UGeometryCollectionComponent* Collection = ResolveCollection();
	if (!Collection || !Collection->GetRestCollection())
	{
		// Survivable and worth saying: a topplable with no collection is a statue that can be hauled
		// at forever and never fall, which from the player's side is indistinguishable from the rope
		// not working.
		UE_LOG(LogGSTopple, Warning,
			TEXT("[GoblinSiege] '%s' has a UGSTopplableComponent but no geometry collection with a rest ")
			TEXT("collection - hauling it over will do nothing. Assign one on the prop."),
			*Owner->GetName());
		return false;
	}

	bToppled = true;

	// ---- the swap ------------------------------------------------------------------------------
	// Retire the intact statue and bring the collection on in its place. Order matters only in that
	// both happen in the same frame, so the player never sees two statues or none.
	bool bSwapped = false;
	for (UStaticMeshComponent* Mesh : TInlineComponentArray<UStaticMeshComponent*>(Owner))
	{
		if (Mesh && Mesh->GetFName() == IntactMeshComponentName)
		{
			Mesh->SetHiddenInGame(true);
			// Collision off as well as hidden: an invisible statue you still bump into reads as the
			// monument having failed to fall, and the debris would land on a wall that is not there.
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			bSwapped = true;
			break;
		}
	}

	UE_LOG(LogGSTopple, Log, TEXT("[GoblinSiege] '%s' SWAP: intact mesh '%s' %s."),
		*Owner->GetName(), *IntactMeshComponentName.ToString(),
		bSwapped ? TEXT("hidden - collection revealed")
				 : TEXT("NOT FOUND, so nothing was hidden and the statue is still standing over the debris"));

	Collection->SetHiddenInGame(false);

	// BOTH of these, and the order matters. Measured live in PIE on 2026-08-19 after three topples
	// that logged success and moved nothing:
	//
	//   SetSimulatePhysics(true) does NOT change ObjectType. The component then reports
	//   is_simulating_physics == true while remaining Chaos_Object_Kinematic - and a kinematic body
	//   ignores impulses by definition. Every impulse was applied to something that could not respond,
	//   silently, with a log line claiming the statue had been toppled.
	//
	// Setting ObjectType to Dynamic first and THEN enabling simulation produced immediate linear
	// (18.7, 64.9, 84.1) and angular (32.6, 5.7, 59.5) velocity from the identical impulse.
	//
	// The monument rests Kinematic - it has a physics proxy so it can be woken, but it does not fall
	// under gravity while standing. Chaos_Object_Static would leave nothing to wake at all.
	Collection->ObjectType = EObjectStateTypeEnum::Chaos_Object_Dynamic;
	Collection->SetSimulatePhysics(true);

	// Push along the pull, with a little lift so it goes OVER rather than skidding. Applied at the
	// rope's anchor - high up on a statue - so the force has a moment arm and the thing rotates about
	// its base. Applied at the centre of mass it would simply slide.
	FVector Direction = PullDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = Owner->GetActorForwardVector();
	}
	Direction = (Direction + FVector::UpVector * ToppleLift).GetSafeNormal();

	// ONE FRAME LATER, and this is the whole difference between a statue that falls and one that
	// stands there reporting success.
	//
	// Measured 2026-08-19: promoting to Dynamic and applying the impulse in the SAME frame produced
	// "OUTCOME: DID NOT MOVE - linear 0 uu/s, angular 0 deg/s, state 4, simulating 1" - the component
	// already reporting itself dynamic and simulating, with the impulse simply gone. Chaos does not
	// switch the proxy's object state until the next physics tick, so an impulse dispatched before
	// that lands on a body which is still kinematic, and a kinematic body discards it silently.
	//
	// The earlier hand test that DID move the statue only worked because the promotion and the impulse
	// happened in two separate Python calls - a frame apart - which is exactly the accident this
	// reproduces deliberately.
	const FVector Shove = Direction * ToppleImpulse;
	const FVector ShoveAt = AnchorPoint;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [this, Shove, ShoveAt]()
			{
				if (UGeometryCollectionComponent* Late = ResolveCollection())
				{
					// THE ONE THAT ACTUALLY MATTERED, found by measurement on 2026-08-19 after four
					// wrong guesses (Static, Kinematic, MaxSimulatedLevel, a one-frame defer).
					//
					// The collection's particles are ANCHORED. An anchored geometry collection is
					// pinned in place no matter what else is true of it - it reported
					// is_active=true, object_type=Dynamic, simulating=true, root_broken=false, and
					// still would not move or even fall under gravity. Removing the anchors and
					// applying the SAME impulse produced linear 185.9 uu/s and angular 61.9 deg/s
					// immediately.
					//
					// Every property flipped before this was already correct. Nothing was ever going
					// to move an anchored collection, which is why each fix "worked" and changed
					// nothing.
					Late->RemoveAllAnchors();
					Late->AddImpulseAtLocation(Shove, ShoveAt);
				}
			}));
	}
	else
	{
		Collection->AddImpulseAtLocation(Shove, ShoveAt);
	}

	UE_LOG(LogGSTopple, Log,
		TEXT("[GoblinSiege] '%s' toppled by '%s' - impulse %.0f along (%.2f, %.2f, %.2f) at the rope anchor."),
		*Owner->GetName(),
		Toppler ? *Toppler->GetName() : TEXT("<none>"),
		ToppleImpulse, Direction.X, Direction.Y, Direction.Z);

	// Half a second later, say whether any of that actually did anything.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ToppleOutcomeTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]() { ReportToppleOutcome(); }), 0.5f, false);
	}

	// ---- score it ------------------------------------------------------------------------------
	// Awarded HERE rather than from a listener on OnToppled, for the reason the idempotence guard at
	// the top of this function exists: the topple is already first-wins, so scoring inside it cannot
	// double-count. A subscriber could be bound twice and silently award twice, and a wrong score is
	// the kind of bug nobody notices until an end-of-raid screen looks odd.
	if (UWorld* World = GetWorld())
	{
		if (UGSScoreSubsystem* Score = World->GetSubsystem<UGSScoreSubsystem>())
		{
			// First of its type scores full, later ones the duplicate rate - the same demotion the
			// burn objectives use, so a hamlet with three idols does not out-score the whole raid.
			const bool bFirstOfType = !DeedTypeTag.IsValid() || Score->GetDeedsForType(DeedTypeTag) <= 0;
			const int32 Points = bFirstOfType ? ToppleDeeds : DuplicateToppleDeeds;

			Score->AddDeeds(DeedTypeTag, Points);

			UE_LOG(LogGSTopple, Log,
				TEXT("[GoblinSiege] '%s' scored +%d deeds (%s %s) -> %d total."),
				*Owner->GetName(), Points,
				bFirstOfType ? TEXT("first") : TEXT("duplicate"),
				DeedTypeTag.IsValid() ? *DeedTypeTag.ToString() : TEXT("untagged"),
				Score->GetTotal());
		}
	}

	// Deliberately NOT calling Break(). The statue is not broken yet - it is falling. It shatters when
	// it lands, through bEnableDamageFromCollision on the collection. See the header.
	OnToppled.Broadcast(Toppler);
	return true;
}

void UGSTopplableComponent::ReportToppleOutcome()
{
	UGeometryCollectionComponent* Collection = ResolveCollection();
	AActor* Owner = GetOwner();
	if (!Collection || !Owner)
	{
		return;
	}

	const FVector Linear = Collection->GetPhysicsLinearVelocity();
	const FVector Angular = Collection->GetPhysicsAngularVelocityInDegrees();
	const bool bMoved = Linear.SizeSquared() > 1.f || Angular.SizeSquared() > 1.f;

	UE_LOG(LogGSTopple, Log,
		TEXT("[GoblinSiege] '%s' OUTCOME: %s - linear %.0f uu/s, angular %.0f deg/s, state %d, simulating %d."),
		*Owner->GetName(),
		bMoved ? TEXT("MOVED") : TEXT("DID NOT MOVE"),
		Linear.Size(), Angular.Size(),
		static_cast<int32>(Collection->ObjectType),
		Collection->IsSimulatingPhysics() ? 1 : 0);
}

void UGSTopplableComponent::ResetTopple()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	bToppled = false;

	if (UGeometryCollectionComponent* Collection = ResolveCollection())
	{
		// Back to inert-and-hidden, in the reverse order Topple() brought it on.
		Collection->SetSimulatePhysics(false);
		Collection->ObjectType = EObjectStateTypeEnum::Chaos_Object_Kinematic;
		Collection->SetHiddenInGame(true);
	}

	for (UStaticMeshComponent* Mesh : TInlineComponentArray<UStaticMeshComponent*>(Owner))
	{
		if (Mesh && Mesh->GetFName() == IntactMeshComponentName)
		{
			Mesh->SetHiddenInGame(false);
			Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			break;
		}
	}

	UE_LOG(LogGSTopple, Log, TEXT("[GoblinSiege] '%s' stood back up."), *Owner->GetName());
}

// Console: stand every monument in the play world back up, so the haul can be retried without
// restarting PIE. Cheat-only - this exists to make the feature testable, not playable.
static FAutoConsoleCommandWithWorld GSToppleResetAllCmd(
	TEXT("GS.Topple.ResetAll"),
	TEXT("Stand every toppled monument back up (testing)."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		int32 Count = 0;
		for (TObjectIterator<UGSTopplableComponent> It; It; ++It)
		{
			UGSTopplableComponent* Topplable = *It;
			// World-filtered: without this an editor-world CDO or another PIE instance gets reset too.
			if (IsValid(Topplable) && Topplable->GetWorld() == World)
			{
				Topplable->ResetTopple();
				++Count;
			}
		}
		UE_LOG(LogGSTopple, Log, TEXT("[GoblinSiege] GS.Topple.ResetAll stood up %d monument(s)."), Count);
	}));
