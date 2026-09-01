#include "Destruction/GSBreakableComponent.h"
#include "Destruction/GSBuildingObjective.h"
#include "Components/ACFDestructableComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Game/ACFDamageType.h"
#include "Interaction/GSInteractableComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"
#include "World/GSCorruptionSubsystem.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSBreakable, Log, All);

UGSBreakableComponent::UGSBreakableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGSBreakableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGSBreakableComponent, bBroken);
}

void UGSBreakableComponent::BeginPlay()
{
	Super::BeginPlay();

	HitPointsRemaining = FMath::Max(SmashHitPoints, 1);

	if (!IntactMeshComponent)
	{
		IntactMeshComponent = ResolveIntactMesh();
	}

	// bBroken can arrive already true - a map saved mid-test, or a replicated actor whose initial
	// state already has it set - and neither case is "replication changing a value", which is the
	// only thing that fires OnRep_Broken. Without this, a pre-broken actor plays with its intact
	// mesh still showing and, worse, never unlocks its interactable: Break()'s SetAvailable(true)
	// is a one-time side effect of the imperative call, not something re-derived from bBroken, so
	// an actor that starts broken is permanently stuck neither looking nor working broken.
	if (bBroken)
	{
		RetireIntactMesh();

		if (bUnlockInteractableOnBreak)
		{
			if (AActor* Owner = GetOwner())
			{
				if (UGSInteractableComponent* Interactable = Owner->FindComponentByClass<UGSInteractableComponent>())
				{
					Interactable->SetAvailable(true);
				}
			}
		}
	}
}

UStaticMeshComponent* UGSBreakableComponent::ResolveIntactMesh() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	// 1. Named explicitly. Always wins.
	if (!IntactMeshComponentName.IsNone())
	{
		for (UStaticMeshComponent* Mesh : TInlineComponentArray<UStaticMeshComponent*>(Owner))
		{
			if (Mesh && Mesh->GetFName() == IntactMeshComponentName)
			{
				return Mesh;
			}
		}
		UE_LOG(LogGSBreakable, Warning,
			TEXT("[GoblinSiege] '%s' names IntactMeshComponent '%s', which does not exist on it. ")
			TEXT("Falling through to automatic resolution."),
			*Owner->GetName(), *IntactMeshComponentName.ToString());
	}

	// 2. A StaticMeshActor has exactly one mesh and it is the point of the actor. This is the window
	//    case - all 113 placed windows - so the behaviour there is unchanged by this whole rework.
	if (const AStaticMeshActor* AsSMA = Cast<AStaticMeshActor>(Owner))
	{
		return AsSMA->GetStaticMeshComponent();
	}

	// 3. Exactly one mesh on the actor: no ambiguity to resolve.
	TInlineComponentArray<UStaticMeshComponent*> Meshes(Owner);
	if (Meshes.Num() == 1)
	{
		return Meshes[0];
	}

	// 4. Several meshes and nobody said which. Hide NOTHING and say so loudly.
	//
	//    The old code hid all of them here. A prop that fails to disappear is a visible bug someone
	//    fixes in a minute; a prop actor that vanishes wholesale gets blamed on Chaos and costs an
	//    afternoon. Refusing to guess is the cheaper failure.
	if (Meshes.Num() > 1)
	{
		UE_LOG(LogGSBreakable, Warning,
			TEXT("[GoblinSiege] '%s' has %d static meshes and no IntactMeshComponentName - breaking it ")
			TEXT("will hide NOTHING. Set IntactMeshComponentName (or call SetIntactMeshComponent) to ")
			TEXT("nominate the one that should disappear."),
			*Owner->GetName(), Meshes.Num());
	}
	return nullptr;
}

bool UGSBreakableComponent::ApplySmash(int32 Damage, const FVector& ImpactPoint,
	const FVector& ImpactVelocity, AActor* Instigator)
{
	if (bBroken)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || Damage <= 0)
	{
		return false;
	}

	// BeginPlay seeds this, but a component added at runtime by MakeActorBreakable can take a hit
	// before its BeginPlay has run.
	if (HitPointsRemaining <= 0)
	{
		HitPointsRemaining = FMath::Max(SmashHitPoints, 1);
	}

	HitPointsRemaining -= Damage;

	if (HitPointsRemaining > 0)
	{
		OnSmashHit.Broadcast(HitPointsRemaining, FMath::Max(SmashHitPoints, 1));
		return false;
	}

	Break(ImpactPoint, ImpactVelocity);
	return true;
}

void UGSBreakableComponent::Break(const FVector& ImpactPoint, const FVector& ImpactVelocity)
{
	// Idempotent, and the guard is load-bearing rather than defensive: a torch sticks where it lands
	// and a second one can easily hit the same hole. Without this the window would re-shatter and,
	// worse, re-report an opening to the building.
	if (bBroken)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	// World corruption (ruling 40). Called directly, not bound to the delegate above, for the
	// reason GSTopplableComponent.cpp already documents for scoring: this call site is
	// first-wins guarded so it cannot double-count, whereas a subscriber can silently be
	// bound twice - and a direct call survives an actor spawned after BeginPlay.
	if (UGSCorruptionSubsystem* Corruption = UGSCorruptionSubsystem::Get(this))
	{
		Corruption->ReportStructureDestroyed(Owner);
	}

	bBroken = true;
	HitPointsRemaining = 0;
	OnRep_Broken();

	// ---- hand Chaos to ACF, if this prop has a fracture -------------------------------------
	bool bFractured = false;
	if (UACFDestructableComponent* Destructable = Owner->FindComponentByClass<UACFDestructableComponent>())
	{
		// THE LINE THAT LOOKS REDUNDANT AND IS NOT. ACF applies strain but never enables simulation;
		// a dormant collection absorbs it and does nothing at all. See the header.
		if (UGeometryCollectionComponent* GC = Owner->FindComponentByClass<UGeometryCollectionComponent>())
		{
			if (GC->GetRestCollection())
			{
				// ObjectType FIRST, then simulation - and both are required. Proven in PIE 2026-08-19
				// (#193/#195): SetSimulatePhysics leaves ObjectType alone, so a collection authored
				// Kinematic or Static reports is_simulating_physics == true while still ignoring every
				// impulse and every strain, silently. The statue logged three successful topples and
				// never moved because of exactly this.
				//
				// No prop on the smash path carries a collection yet, so this has never fired here -
				// which is precisely why it is worth fixing now rather than after it costs someone a day.
				GC->ObjectType = EObjectStateTypeEnum::Chaos_Object_Dynamic;
				GC->SetSimulatePhysics(true);

				FACFDamageEvent Event;
				Event.DamageDealer = Owner;          // no dealer is tracked through Break(); the prop
				Event.DamageReceiver = Owner;        // is its own context and ACF only reads these for FX
				Event.hitResult.ImpactPoint = ImpactPoint;
				// ACF convention (UACMCollisionManagerComponent::ApplyPointDamage): hitDirection points
				// from the victim back TOWARD the damager, and scatter is its negation. So to throw
				// pieces along the hit's travel, hand it the reverse of the travel direction.
				Event.hitDirection = -ImpactVelocity.GetSafeNormal();

				Destructable->ForceDestruction(Event);
				bFractured = true;
			}
		}

		if (!bFractured)
		{
			UE_LOG(LogGSBreakable, Warning,
				TEXT("[GoblinSiege] '%s' has a UACFDestructableComponent but no geometry collection with ")
				TEXT("a rest collection assigned - falling back to hide + FX. Assign one on the prop."),
				*Owner->GetName());
		}
	}

	UE_LOG(LogGSBreakable, Log, TEXT("[GoblinSiege] '%s' broken (%s)."),
		*Owner->GetName(),
		bFractured ? TEXT("ACF fracture") : TEXT("hidden + FX"));

	// The point of the whole feature: a broken window is a way in.
	if (bOpensBuilding)
	{
		if (AGSBuildingObjective* Building = AGSBuildingObjective::FindBuildingOwning(this, Owner))
		{
			Building->IgniteInterior(EGSBuildingIgnitionSource::Window);
		}
		else
		{
			// Not fatal - a window in a wall that belongs to no building simply opens onto nothing.
			// Worth saying, though: the usual cause is a building whose AdoptRadius did not reach
			// this piece, which is the same silent-shortfall class of bug the market had.
			UE_LOG(LogGSBreakable, Warning,
				TEXT("[GoblinSiege] '%s' broke but belongs to no building - nothing caught fire. ")
				TEXT("Is it inside a AGSBuildingObjective's AdoptRadius?"), *Owner->GetName());
		}
	}

	// Smashing a container is how you OPEN it. The lid comes off and the loot becomes reachable in
	// one motion, rather than "break it" and "loot it" being two unrelated verbs on one actor.
	if (bUnlockInteractableOnBreak)
	{
		if (UGSInteractableComponent* Interactable = Owner->FindComponentByClass<UGSInteractableComponent>())
		{
			// SetAvailable is server-only and replicates through OnRep_IsAvailable, which is the same
			// path a looted chest uses to turn itself off - so this needs no client work.
			Interactable->SetAvailable(true);

			UE_LOG(LogGSBreakable, Log,
				TEXT("[GoblinSiege] '%s' broken open - its interactable is now available."),
				*Owner->GetName());
		}
		else
		{
			UE_LOG(LogGSBreakable, Warning,
				TEXT("[GoblinSiege] '%s' has bUnlockInteractableOnBreak but no UGSInteractableComponent ")
				TEXT("to unlock. Breaking it opens nothing."), *Owner->GetName());
		}
	}

	OnBroken.Broadcast();
}

void UGSBreakableComponent::OnRep_Broken()
{
	if (!bBroken)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Hide the intact mesh whether or not ACF fractured. With a fracture the collection is the thing
	// you should be looking at; without one, hiding IS the break.
	RetireIntactMesh();

	if (UNiagaraSystem* FX = BreakFX.LoadSynchronous())
	{
		// At the mesh, not at the actor pivot. On anything larger than a window the pivot can be
		// metres from the surface that was actually hit, and the puff appears in mid-air.
		const FVector FXLocation = IntactMeshComponent
			? IntactMeshComponent->Bounds.Origin
			: Owner->GetActorLocation();

		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FX, FXLocation, Owner->GetActorRotation());
	}
}

void UGSBreakableComponent::RetireIntactMesh()
{
	// Resolved at BeginPlay; re-resolve defensively for components added at runtime.
	if (!IntactMeshComponent)
	{
		IntactMeshComponent = ResolveIntactMesh();
	}

	if (!IntactMeshComponent)
	{
		return; // ResolveIntactMesh already warned; do not hide the whole actor as a guess.
	}

	// A broken variant, where the art exists, beats disappearing. The prop stays in the world as
	// evidence the player did something, which is the same argument GSDestructibleObjective makes for
	// leaving a wreck behind: "the raid has no memory" otherwise.
	if (UStaticMesh* Broken = BrokenMesh.LoadSynchronous())
	{
		IntactMeshComponent->SetStaticMesh(Broken);

		// Collision deliberately left alone. A smashed crate is still a crate you can walk into, and
		// the swap pairs share bounds closely enough that the existing collision still fits. Only the
		// hide path needs to drop collision, and for a different reason - see below.
		return;
	}

	// Collision off as well as hidden. Hiding alone would leave an invisible pane the next torch
	// bounces off, which reads as the window having repaired itself.
	IntactMeshComponent->SetHiddenInGame(true);
	IntactMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
