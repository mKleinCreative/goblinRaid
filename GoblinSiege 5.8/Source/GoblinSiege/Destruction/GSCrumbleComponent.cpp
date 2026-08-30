#include "Destruction/GSCrumbleComponent.h"

#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"
#include "Destruction/GSFlammableComponent.h"
#include "Combat/GSGE_WeaponDamage.h"
#include "Combat/GSGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSCrumble, Log, All);

UGSCrumbleComponent::UGSCrumbleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// C++ default, same reasoning as AGSFireVolume::FireDamageEffectClass - a hazard's damage GE is
	// not worth a content asset a designer could silently unassign, and a C++ default can't go
	// missing from a content folder. UGSGE_WeaponDamage bakes no Damage.* tag of its own by design
	// (see its header) - HandlePieceCollision supplies Damage.Blast, same as any other non-fire hit.
	DebrisDamageEffectClass = UGSGE_WeaponDamage::StaticClass();
}

void UGSCrumbleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGSCrumbleComponent, Release);
}

void UGSCrumbleComponent::BeginPlay()
{
	Super::BeginPlay();

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Say it out loud at start rather than letting it be discovered in co-op.
	//
	// AStaticMeshActor does NOT replicate by default - it only calls SetReplicates(true) when the
	// per-instance bStaticMeshReplicateMovement is ticked. A crumble component on a non-replicating
	// actor is a component whose replicated state has nowhere to go: the release lands on the server
	// and no client is ever told. Everything looks correct in single player and the monument stands
	// untouched for every other player, which is precisely the class of bug this component exists to
	// close, so it must not be reintroduced silently one actor at a time.
	if (Owner->HasAuthority() && !Owner->GetIsReplicated())
	{
		UE_LOG(LogGSCrumble, Warning,
			TEXT("[GoblinSiege] '%s' carries a UGSCrumbleComponent but the actor does not replicate - ")
			TEXT("it will crumble on the server and stand untouched on every client. Tick Replicates on ")
			TEXT("the actor (or bStaticMeshReplicateMovement on a StaticMeshActor)."),
			*Owner->GetName());
	}

	if (bAutoCrumbleOnBurnedDown)
	{
		if (UGSFlammableComponent* Flammable = Owner->FindComponentByClass<UGSFlammableComponent>())
		{
			Flammable->OnBurnedDown.AddDynamic(this, &UGSCrumbleComponent::HandleBurnedDown);
		}
		else
		{
			UE_LOG(LogGSCrumble, Warning,
				TEXT("[GoblinSiege] '%s' has bAutoCrumbleOnBurnedDown set but no UGSFlammableComponent ")
				TEXT("- nothing will ever call Crumble()."),
				*Owner->GetName());
		}
	}
}

void UGSCrumbleComponent::HandleBurnedDown()
{
	Crumble(FVector::ZeroVector, FVector::ZeroVector);
}

UGeometryCollectionComponent* UGSCrumbleComponent::ResolveCollection() const
{
	AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UGeometryCollectionComponent>() : nullptr;
}

UStaticMeshComponent* UGSCrumbleComponent::ResolveIntactMesh() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	TInlineComponentArray<UStaticMeshComponent*> Meshes(Owner);

	// The named one wins, always. This is the authored answer and the only one that is safe on a
	// multi-mesh prop.
	if (!IntactMeshComponentName.IsNone())
	{
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (Mesh && Mesh->GetFName() == IntactMeshComponentName)
			{
				return Mesh;
			}
		}
	}

	// Nothing named, so fall back ONLY where there is no guess to get wrong. A plain kit actor is one
	// StaticMeshActor with one mesh component called StaticMeshComponent0, and requiring somebody to
	// hand-name it would mean touching every placed house. Two or more candidates and we refuse:
	// hiding the wrong one is worse than hiding none, because the actor visibly half-disappears.
	if (Meshes.Num() == 1)
	{
		return Meshes[0];
	}

	return nullptr;
}

UGSCrumbleComponent* UGSCrumbleComponent::FindOrAdd(AActor* Actor)
{
	if (!IsValid(Actor) || !Actor->HasAuthority())
	{
		return nullptr;
	}

	if (UGSCrumbleComponent* Existing = Actor->FindComponentByClass<UGSCrumbleComponent>())
	{
		return Existing;
	}

	UGSCrumbleComponent* Added = NewObject<UGSCrumbleComponent>(Actor);
	if (!Added)
	{
		return nullptr;
	}

	// Both of these, and in this order. RegisterComponent is what makes it live; SetIsReplicated is
	// what makes it reach the clients, and a runtime-added component does not inherit the CDO's
	// SetIsReplicatedByDefault.
	Added->RegisterComponent();
	Added->SetIsReplicated(true);
	Actor->AddInstanceComponent(Added);

	return Added;
}

AActor* UGSCrumbleComponent::SpawnProxyFor(AActor* Source, const FString& Folder,
	FName AssetNameOverride)
{
	if (!IsValid(Source))
	{
		return nullptr;
	}
	UWorld* World = Source->GetWorld();
	if (!World || !Source->HasAuthority())
	{
		return nullptr;
	}

	UStaticMeshComponent* MeshComp = Source->FindComponentByClass<UStaticMeshComponent>();
	UStaticMesh* Mesh = MeshComp ? MeshComp->GetStaticMesh() : nullptr;
	if (!Mesh)
	{
		return nullptr;
	}

	// An explicit name wins over the SM_ -> GC_ convention. Needed where one mesh yields SEVERAL
	// collections - the windmill is fractured once and then pruned into a falling top and a standing
	// stump, and neither is called GC_WIndmill_Base.
	FString AssetName;
	if (!AssetNameOverride.IsNone())
	{
		AssetName = AssetNameOverride.ToString();
	}
	else
	{
		FString Base = Mesh->GetName();
		if (Base.StartsWith(TEXT("SM_")))
		{
			Base.RightChopInline(3);
		}
		AssetName = FString::Printf(TEXT("GC_%s"), *Base);
	}
	const FString FullPath = FString::Printf(TEXT("%s/%s.%s"), *Folder, *AssetName, *AssetName);

	UGeometryCollection* Collection = LoadObject<UGeometryCollection>(nullptr, *FullPath);
	if (!Collection)
	{
		return nullptr;
	}

	// A stub with one transform is the root and nothing else. GC_House_Window_C is exactly that, and
	// using it hid a window behind a swap that reported success.
	if (Collection->NumElements(FGeometryCollection::TransformGroup) <= 1)
	{
		UE_LOG(LogGSCrumble, Warning,
			TEXT("[GoblinSiege] %s has no geometry (%d transforms) - it is a stub, not a fracture."),
			*AssetName, Collection->NumElements(FGeometryCollection::TransformGroup));
		return nullptr;
	}

	const FTransform Xform = Source->GetActorTransform();
	AGeometryCollectionActor* Proxy = World->SpawnActorDeferred<AGeometryCollectionActor>(
		AGeometryCollectionActor::StaticClass(), Xform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Proxy)
	{
		return nullptr;
	}

	if (UGeometryCollectionComponent* GCC = Proxy->GetGeometryCollectionComponent())
	{
		// Rest collection BEFORE FinishSpawningActor, or the component registers empty and reports
		// zero transforms until something re-registers it.
		GCC->SetRestCollection(Collection);
		GCC->ObjectType = EObjectStateTypeEnum::Chaos_Object_Static;
		GCC->SetCollisionProfileName(TEXT("Destructible"));
		GCC->SetHiddenInGame(true);

		// THE WRECKAGE STAYS. Both of these default to TRUE on the component, so Chaos deletes
		// pieces once they settle and fall asleep - Michael: "the top of the mill keeps
		// disappearing?". The asset-level remove_on_max_sleep is already False on every collection
		// here; it is the COMPONENT flags that were doing it, and nothing was setting them.
		//
		// #192 reached the same conclusion on the statue and gave the reason: a raid whose ruins
		// evaporate has no memory. A player sweeping back through the hamlet has to find the wreck
		// they made still lying there.
		GCC->bAllowRemovalOnSleep = false;
		GCC->bAllowRemovalOnBreak = false;
	}
	Proxy->SetReplicates(true);
	UGameplayStatics::FinishSpawningActor(Proxy, Xform);

	if (UGeometryCollectionComponent* GCC = Proxy->GetGeometryCollectionComponent())
	{
		GCC->SetSimulatePhysics(false);
	}
	return Proxy;
}

bool UGSCrumbleComponent::Crumble(const FVector& Impulse, const FVector& ImpulseAt)
{
	// A thing is destroyed once. Not defensive: the grapple haul ticks every frame and would
	// otherwise re-shove something already mid-fall, which reads as it being swatted.
	if (Release.bReleased)
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
		// Survivable and worth saying. A destroyed thing that does not come apart is, from the
		// player's side, indistinguishable from the torch or the rope not having worked at all - and
		// this is the expected state for every building until its fracture asset is authored, so the
		// message has to name what is missing rather than read as an error.
		UE_LOG(LogGSCrumble, Warning,
			TEXT("[GoblinSiege] '%s' was destroyed but has no geometry collection to release - ")
			TEXT("it will simply stop, not crumble. Author a GC for its mesh and it starts working ")
			TEXT("with no code change."),
			*Owner->GetName());
		return false;
	}

	Release.bReleased = true;
	Release.Impulse = Impulse;
	Release.ImpulseAt = ImpulseAt;

	// The server does not get its own RepNotify, so it calls the same path directly - the pattern
	// UGSBreakableComponent::Break already uses for bBroken. One code path, both sides, so a
	// divergence between what the host sees and what a client sees has nowhere to hide.
	OnRep_Release();
	return true;
}

void UGSCrumbleComponent::OnRep_Release()
{
	if (!Release.bReleased)
	{
		// A reset replicating back down. Nothing to do here - ResetCrumble already restored the
		// visuals on every machine that ran it.
		return;
	}

	ApplyRelease();
	OnCrumbled.Broadcast();

	// Half a second later, say whether any of that actually did anything.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(OutcomeTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]() { ReportCrumbleOutcome(); }), 0.5f, false);

		// Later than the outcome line on purpose. Half a second was far too early to call anything
		// "hanging" - most pieces have not started falling yet, and the first version of that warning
		// duly reported 16 of 23 stuck when the real number was two.
		World->GetTimerManager().SetTimer(SweepTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]() { SweepStragglers(); }), 6.0f, true);
	}
}

void UGSCrumbleComponent::ApplyRelease()
{
	AActor* Owner = GetOwner();
	UGeometryCollectionComponent* Collection = ResolveCollection();
	if (!Owner || !Collection)
	{
		return;
	}

	// ---- 1. the swap ---------------------------------------------------------------------------
	// Retire the intact mesh and bring the collection on in its place. Order matters only in that
	// both happen in the same frame, so the player never sees two statues or none.
	UStaticMeshComponent* Intact = ResolveIntactMesh();
	if (Intact)
	{
		Intact->SetHiddenInGame(true);
		// Collision off as well as hidden: an invisible statue you still bump into reads as the
		// monument having failed to fall, and the debris would land on a wall that is not there.
		Intact->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Three outcomes, and they must read differently. "No intact mesh at all" is the correct and
	// expected shape for an actor whose only visual IS the collection, and logging that as a failure
	// would train everyone to ignore the line - which is how the old topple log came to announce
	// success three times while the statue stood there. Only the ambiguous case is a warning.
	if (Intact)
	{
		UE_LOG(LogGSCrumble, Log, TEXT("[GoblinSiege] '%s' SWAP: intact mesh hidden, collection revealed."),
			*Owner->GetName());
	}
	else if (TInlineComponentArray<UStaticMeshComponent*>(Owner).Num() == 0)
	{
		UE_LOG(LogGSCrumble, Log,
			TEXT("[GoblinSiege] '%s' SWAP: nothing to retire - the collection is this actor's only visual."),
			*Owner->GetName());
	}
	else
	{
		UE_LOG(LogGSCrumble, Warning,
			TEXT("[GoblinSiege] '%s' SWAP: no intact mesh named '%s' and %d candidates to choose from, so ")
			TEXT("nothing was hidden - it will stand there over its own debris. Name one on the component."),
			*Owner->GetName(), *IntactMeshComponentName.ToString(),
			TInlineComponentArray<UStaticMeshComponent*>(Owner).Num());
	}

	Collection->SetHiddenInGame(false);

	// ---- 2. dynamic BEFORE simulating ----------------------------------------------------------
	// BOTH of these, and the order matters. Measured live in PIE on 2026-08-19 after three topples
	// that logged success and moved nothing:
	//
	//   SetSimulatePhysics(true) does NOT change ObjectType. The component then reports
	//   is_simulating_physics == true while remaining Chaos_Object_Kinematic - and a kinematic body
	//   ignores impulses by definition. Every impulse was applied to something that could not
	//   respond, silently, with a log line claiming the statue had been toppled.
	//
	// Setting ObjectType to Dynamic first and THEN enabling simulation produced immediate linear
	// (18.7, 64.9, 84.1) and angular (32.6, 5.7, 59.5) velocity from the identical impulse.
	//
	// The thing rests Kinematic - it has a physics proxy so it can be woken, but it does not fall
	// under gravity while standing. Chaos_Object_Static would leave nothing to wake at all.
	// Carry the burn across BEFORE it is revealed, so it never flashes clean for a frame.
	if (CharAmountOnRelease > 0.f)
	{
		const int32 NumMats = Collection->GetNumMaterials();
		for (int32 i = 0; i < NumMats; ++i)
		{
			if (UMaterialInstanceDynamic* MID = Collection->CreateDynamicMaterialInstance(i, nullptr))
			{
				MID->SetScalarParameterValue(FName("GS_BurnAmount"), CharAmountOnRelease);
			}
		}
		UE_LOG(LogGSCrumble, Log,
			TEXT("[GoblinSiege] %s charred the wreck to %.2f across %d material slot(s)."),
			*Owner->GetName(), CharAmountOnRelease, NumMats);
	}

	// ---- 2a. ANCHOR THE BASE WHILE IT IS STILL STATIC -------------------------------------------
	//
	// ANCHORS ARE AN INITIAL STATE, NOT A COMMAND. That is the conclusion after three failures, each
	// of which anchored AFTER the collection had already gone dynamic:
	//
	//   anchor-then-crumble : the crumble broke the bonds holding the anchored base, and Michael
	//                         watched "everything underneath the marker explode and the top stay still".
	//   anchor, no crumble  : nothing could separate at all - 27 pinned, 22 above the line, 0 movement.
	//   crumble-then-anchor : the top fell correctly (median 849uu) but 25 of 27 base pieces went too,
	//                         because anchoring pieces that are already broken and simulating does not
	//                         re-pin them.
	//
	// So the anchors go on here, before ObjectType flips and before physics starts - the same reason
	// GC_Statue_Warrior ships Chaos_Object_Static rather than being promoted at runtime.
	if (KeepAnchoredBelowFraction > 0.f)
	{
		// Clear whatever the fracture pipeline authored first. On GC_Statue_Warrior that turned out to
		// be everything, which is what pinned the statue through four wrong fixes in #193.
		Collection->RemoveAllAnchors();

		const TArray<FTransform> Rest = Collection->GetLocalRestTransforms();
		if (Rest.Num() > 0)
		{
			float MinZ = TNumericLimits<float>::Max();
			float MaxZ = TNumericLimits<float>::Lowest();
			for (const FTransform& T : Rest)
			{
				MinZ = FMath::Min(MinZ, static_cast<float>(T.GetTranslation().Z));
				MaxZ = FMath::Max(MaxZ, static_cast<float>(T.GetTranslation().Z));
			}
			const float CutZ = MinZ + (MaxZ - MinZ) * KeepAnchoredBelowFraction;

			int32 Pinned = 0;
			for (int32 i = 0; i < Rest.Num(); ++i)
			{
				if (static_cast<float>(Rest[i].GetTranslation().Z) <= CutZ)
				{
					Collection->SetAnchoredByIndex(i, true);
					++Pinned;
				}
			}

			UE_LOG(LogGSCrumble, Log,
				TEXT("[GoblinSiege] '%s' anchored %d of %d piece(s) at or below local z=%.0f BEFORE going ")
				TEXT("dynamic (rest span %.0f..%.0f, fraction %.2f)."),
				*Owner->GetName(), Pinned, Rest.Num(), CutZ, MinZ, MaxZ, KeepAnchoredBelowFraction);
		}
	}

	Collection->ObjectType = EObjectStateTypeEnum::Chaos_Object_Dynamic;
	Collection->SetSimulatePhysics(true);

	// Debris damage (2026-08-30). Opted in per-instance, off by default like the whole collapse
	// shape above - see bEnableDamageFromCollision. Bound here, once, at release: pieces are inert
	// (Static, no collision events) until this exact moment, so there is nothing to notify on
	// before now, and binding once at release rather than in BeginPlay keeps a component that never
	// crumbles at zero per-frame cost.
	if (bEnableDamageFromCollision)
	{
		Collection->SetNotifyRigidBodyCollision(true);
		Collection->OnChaosPhysicsCollision.AddDynamic(this, &UGSCrumbleComponent::HandlePieceCollision);
	}

	// DETACH, OR IT CANNOT MOVE NO MATTER WHAT ELSE IS TRUE.
	//
	// A collection attached to a parent scene component has its transform driven by the ATTACHMENT,
	// not by the solver. It reports simulating=1, object_type=Dynamic, accepts anchors being removed
	// and accepts impulses - and sits exactly where its parent puts it.
	//
	// Measured 2026-08-26, and this is the whole reason the arena worked and the level did not: in
	// L_CombatArena the collection WAS the actor's root, unattached, and the house folded. On
	// L_Tutorial_Island it is attached to a StaticMeshActor's root by EnsurePieceCollection, and the
	// identical release logged "simulating 1 ... DID NOT MOVE" and the house simply vanished - intact
	// mesh hidden, collection pinned to a parent it could not leave.
	//
	// Skipped when the collection IS the root (the statue's shape), which has no parent to leave.
	AActor* CollectionOwner = Collection->GetOwner();
	if (CollectionOwner && Collection != CollectionOwner->GetRootComponent() && Collection->GetAttachParent())
	{
		// Name read BEFORE the detach - afterwards GetAttachParent() is null and the log would be
		// reporting on a pointer it no longer has.
		const FString ParentName = Collection->GetAttachParent()->GetName();
		Collection->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		UE_LOG(LogGSCrumble, Log,
			TEXT("[GoblinSiege] '%s' detached the collection from '%s' - an attached body is driven by ")
			TEXT("its parent and cannot fall."),
			*Owner->GetName(), *ParentName);
	}

	// ---- 3 and 4. anchors off, shove one frame later -------------------------------------------
	// ONE FRAME LATER, and this is the whole difference between a thing that falls and one that
	// stands there reporting success.
	//
	// Measured 2026-08-19: promoting to Dynamic and applying the impulse in the SAME frame produced
	// "OUTCOME: DID NOT MOVE - linear 0 uu/s, angular 0 deg/s, state 4, simulating 1" - the component
	// already reporting itself dynamic and simulating, with the impulse simply gone. Chaos does not
	// switch the proxy's object state until the next physics tick, so an impulse dispatched before
	// that lands on a body which is still kinematic, and a kinematic body discards it silently.
	//
	// The earlier hand test that DID move the statue only worked because the promotion and the
	// impulse happened in two separate Python calls - a frame apart - which is exactly the accident
	// this reproduces deliberately.
	const FVector Shove = Release.Impulse;
	const FVector ShoveAt = Release.ImpulseAt;
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
					//
					// Unconditional, unlike the impulse below: a building released with no shove at
					// all still has to stop being pinned, or it burns to the ground and hangs in the
					// air exactly as it stood.
					// RemoveAllAnchors ONLY when nothing is meant to stay. With a base anchored above,
					// this call would undo the one thing that makes a sink a sink.
					if (KeepAnchoredBelowFraction <= 0.f)
					{
						Late->RemoveAllAnchors();
					}

					// Break the cluster bonds so pieces can separate. Anchored particles stay kinematic
					// through this - that is the point of setting them before simulation started.
					for (int32 Pass = 0; Pass < ClusterCrumblePasses; ++Pass)
					{
						Late->CrumbleActiveClusters();
					}

					// UNPIN EVERY PIECE EXPLICITLY when nothing is meant to stay.
					//
					// RemoveAllAnchors above should already have done this, and evidently does not reach
					// everything: Michael's screenshot showed sail and cap fragments hanging in the sky,
					// and they measured as displaced 0, 1, 24 and 52 uu - pinned, not resting on anything.
					// Whether they were still anchored or sat in a cluster level the passes never reached,
					// clearing each index by hand costs nothing and removes the ambiguity.
					if (KeepAnchoredBelowFraction <= 0.f)
					{
						const int32 Count = Late->GetLocalRestTransforms().Num();
						for (int32 i = 0; i < Count; ++i)
						{
							Late->SetAnchoredByIndex(i, false);
						}
					}

					if (CollapseShoveCount > 0)
					{
						ApplyCollapseRing(Late);
					}
					else if (!Shove.IsNearlyZero())
					{
						Late->AddImpulseAtLocation(Shove, ShoveAt);
					}
				}
			}));
	}
	else if (!Shove.IsNearlyZero())
	{
		Collection->RemoveAllAnchors();
		Collection->AddImpulseAtLocation(Shove, ShoveAt);
	}

	// Built into a named local rather than inlined as *FString::Printf(...) in the ternary: that
	// idiom hands UE_LOG a pointer into a temporary, and it only survives because the temporary
	// happens to outlive the full expression. Not a trap worth leaving for the next reader.
	const FString How = Shove.IsNearlyZero()
		? FString(TEXT("under its own weight"))
		: FString::Printf(TEXT("with impulse %.0f at (%.0f, %.0f, %.0f)"),
			Shove.Size(), ShoveAt.X, ShoveAt.Y, ShoveAt.Z);

	UE_LOG(LogGSCrumble, Log, TEXT("[GoblinSiege] '%s' released %s."), *Owner->GetName(), *How);
}

void UGSCrumbleComponent::HandlePieceCollision(const FChaosPhysicsCollisionInfo& CollisionInfo)
{
	// Authoritative only - every client still sees and feels the collision physically (that part is
	// simulated locally, same as the rest of this component's multiplayer story - see the class
	// comment), it just does not independently apply the GameplayEffect.
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	// Below this, a collision is rubble settling against its own neighbours, not a piece landing ON
	// something - see MinImpulseToDamage's own comment for why that distinction has to exist at all.
	if (CollisionInfo.AccumulatedImpulse.Size() < MinImpulseToDamage)
	{
		return;
	}

	UPrimitiveComponent* OtherComp = CollisionInfo.OtherComponent;
	AActor* OtherActor = OtherComp ? OtherComp->GetOwner() : nullptr;
	if (!OtherActor || OtherActor == Owner)
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OtherActor);
	if (!TargetASC)
	{
		return; // no ASC - other debris, world geometry, non-GAS actors simply aren't damageable
	}

	if (TargetASC->HasMatchingGameplayTag(GSTags::State_Dead)
		|| TargetASC->HasMatchingGameplayTag(GSTags::State_Invulnerable))
	{
		return;
	}

	if (!DebrisDamageEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
	Context.AddInstigator(Owner, Owner);

	const FGameplayEffectSpecHandle SpecHandle =
		TargetASC->MakeOutgoingSpec(DebrisDamageEffectClass, 1.f, Context);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (!Spec)
	{
		return;
	}

	// Blast, not a dedicated Damage.Debris tag - falling masonry is blunt-force trauma, and
	// GSDamageExecCalculation already treats Damage.Blast as armor-bypassing, which is exactly right
	// for a stone block rather than a slash or a puncture. See UGSDamageExecCalculation.
	Spec->AddDynamicAssetTag(GSTags::Damage_Blast);
	Spec->SetSetByCallerMagnitude(GSTags::Damage_Blast, DebrisDamage);

	TargetASC->ApplyGameplayEffectSpecToSelf(*Spec);

	UE_LOG(LogGSCrumble, Log,
		TEXT("[GoblinSiege] '%s' debris hit '%s' for %.0f (impulse %.0f)."),
		*Owner->GetName(), *OtherActor->GetName(), DebrisDamage, CollisionInfo.AccumulatedImpulse.Size());
}

void UGSCrumbleComponent::ApplyCollapseRing(UGeometryCollectionComponent* Collection)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Collection || CollapseShoveCount <= 0)
	{
		return;
	}

	// Ring geometry is derived from the owner's own bounds rather than authored per house, so one
	// setting covers a kit of 43 differently-sized buildings.
	FVector Origin, Extent;
	Owner->GetActorBounds(false, Origin, Extent);

	const float BaseZ = Origin.Z - Extent.Z;
	const float RingZ = BaseZ + (Extent.Z * 2.f) * CollapseShoveHeightFraction;
	const float Radius = FMath::Min(Extent.X, Extent.Y) * CollapseShoveRadiusFraction;

	for (int32 i = 0; i < CollapseShoveCount; ++i)
	{
		const float Angle = (2.f * PI * i) / CollapseShoveCount;
		const float dx = FMath::Cos(Angle) * Radius;
		const float dy = FMath::Sin(Angle) * Radius;

		// Down, and leaning in toward the centre. The inward lean is what folds it into its own
		// footprint; straight down alone just squashes it where it stands.
		FVector Dir(-dx, -dy, 0.f);
		Dir = Dir.GetSafeNormal() * CollapseInwardRatio;
		Dir.Z = -1.f;
		Dir = Dir.GetSafeNormal();

		const FVector At(Origin.X + dx, Origin.Y + dy, RingZ);
		Collection->AddImpulseAtLocation(Dir * CollapseShoveMagnitude, At);
	}

	UE_LOG(LogGSCrumble, Log,
		TEXT("[GoblinSiege] '%s' collapse ring: %d shoves of %.0f, inward %.2f, at z=%.0f radius %.0f."),
		*Owner->GetName(), CollapseShoveCount, CollapseShoveMagnitude,
		CollapseInwardRatio, RingZ, Radius);
}

void UGSCrumbleComponent::ReportCrumbleOutcome()
{
	// Hanging pieces are the failure this log kept missing. Horizontal spread and the LOWEST piece
	// both looked healthy while fragments sat in the sky, because neither question asks "did anything
	// simply not fall". It took a screenshot to see it. Now it is a number.
	if (UGeometryCollectionComponent* GCC = ResolveCollection())
	{
		const TArray<FTransform> RestT = GCC->GetLocalRestTransforms();
		const TArray<FTransform> NowT = GCC->GetCurrentTransforms();
		int32 Unmoved = 0;
		float HighestUnmoved = -FLT_MAX;
		const int32 N = FMath::Min(RestT.Num(), NowT.Num());
		for (int32 i = 0; i < N; ++i)
		{
			const float Displaced = (NowT[i].GetTranslation() - RestT[i].GetTranslation()).Size();
			if (Displaced < 60.f)
			{
				++Unmoved;
				HighestUnmoved = FMath::Max(HighestUnmoved, static_cast<float>(NowT[i].GetTranslation().Z));
			}
		}
		if (Unmoved > 0)
		{
			UE_LOG(LogGSCrumble, Warning,
				TEXT("[GoblinSiege] %d of %d piece(s) did not move, highest at local z=%.0f - if that is ")
				TEXT("above the wreck they are hanging in the air."),
				Unmoved, N, HighestUnmoved);
		}
	}

	UGeometryCollectionComponent* Collection = ResolveCollection();
	AActor* Owner = GetOwner();
	if (!Collection || !Owner)
	{
		return;
	}

	const FVector Linear = Collection->GetPhysicsLinearVelocity();
	const FVector Angular = Collection->GetPhysicsAngularVelocityInDegrees();
	const bool bMoved = Linear.SizeSquared() > 1.f || Angular.SizeSquared() > 1.f;

	// Netmode in the line on purpose: with the crumble now running on every machine, "it moved on
	// the host" and "it moved here" are different claims and the log has to distinguish them.
	// Mass and attachment are in the line because both have now silently defeated a release that
	// reported every other field correct. A collection whose mass Chaos derives from density can be
	// tens of thousands of kilos, at which point an impulse sized by eye moves it by millimetres -
	// so the log states the ratio rather than leaving it to be recomputed by hand later.
	const float Mass = Collection->GetMass();
	UE_LOG(LogGSCrumble, Log,
		TEXT("[GoblinSiege] '%s' OUTCOME (%s): %s - linear %.0f uu/s, angular %.0f deg/s, state %d, ")
		TEXT("simulating %d, mass %.0f kg, attached %s."),
		*Owner->GetName(),
		Owner->HasAuthority() ? TEXT("server") : TEXT("client"),
		bMoved ? TEXT("MOVED") : TEXT("DID NOT MOVE"),
		Linear.Size(), Angular.Size(),
		static_cast<int32>(Collection->ObjectType),
		Collection->IsSimulatingPhysics() ? 1 : 0,
		Mass,
		Collection->GetAttachParent() ? TEXT("YES - it cannot fall") : TEXT("no"));
}


void UGSCrumbleComponent::SweepStragglers()
{
	UGeometryCollectionComponent* Collection = ResolveCollection();
	AActor* Owner = GetOwner();
	if (!Collection || !Owner || KeepAnchoredBelowFraction > 0.f)
	{
		// A thing with a deliberate standing base has pieces that are SUPPOSED not to move.
		return;
	}

	const TArray<FTransform> Rest = Collection->GetLocalRestTransforms();
	const TArray<FTransform> Now = Collection->GetCurrentTransforms();
	const int32 N = FMath::Min(Rest.Num(), Now.Num());

	// THE RULE IS SIMPLY "EVERYTHING MUST MOVE".
	//
	// The first version worked out where the rest of the wreck had settled and only swept pieces
	// above it. That needed something to have moved already, and on a slow collapse nothing had by
	// the time it ran - so it took its own "nothing moved at all" early-out and did nothing, while
	// Michael watched a fragment hang in the sky. A guard that disables the fix in exactly the case
	// the fix exists for.
	//
	// When KeepAnchoredBelowFraction is zero, every piece in this collection is meant to come down.
	// So a piece that has not moved is a straggler, full stop. No reference height to compute and
	// nothing to be too early for.
	// THREE PASSES AND STOP.
	//
	// Left repeating, this fires every six seconds forever - measured, and it did: the same single
	// straggler swept at 23:10:12, :18 and :24 and onward. Index 0 of a collection is its ROOT
	// transform, which is bookkeeping rather than geometry; it never moves because there is nothing
	// there to move, so no amount of strain will ever satisfy the test. A backstop that cannot
	// succeed must at least know when to stop trying.
	++SweepPassesRun;
	if (SweepPassesRun >= 3)
	{
		if (UWorld* W = GetWorld())
		{
			W->GetTimerManager().ClearTimer(SweepTimer);
		}
	}

	int32 Freed = 0;
	for (int32 i = 0; i < N; ++i)
	{
		// Skip the root. It is not a shard and cannot fall.
		if (i == 0)
		{
			continue;
		}
		if ((Now[i].GetTranslation() - Rest[i].GetTranslation()).Size() >= 60.f)
		{
			continue;
		}

		const FVector At = Owner->GetActorTransform().TransformPosition(Now[i].GetTranslation());
		Collection->CrumbleCluster(i);
		Collection->ApplyExternalStrain(i, At, 300.f, 3, 1.f, 1000000.f);
		Collection->ApplyBreakingLinearVelocity(i, FVector(0.f, 0.f, -300.f));
		++Freed;
	}

	if (Freed > 0)
	{
		UE_LOG(LogGSCrumble, Log,
			TEXT("[GoblinSiege] '%s' swept %d straggler(s) of %d that had not moved."),
			*Owner->GetName(), Freed, N);
	}
}

void UGSCrumbleComponent::ResetCrumble()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Deliberately NOT authority-gated, unlike Crumble. This is a testing verb, and in PIE it has to
	// be usable from whichever machine the tester is looking at. The server's reset replicates and
	// the clients run their own; a client-only reset puts that one client's view back and is exactly
	// what somebody debugging a client-side divergence wants.
	if (Owner->HasAuthority())
	{
		Release = FGSCrumbleRelease();
	}

	if (UGeometryCollectionComponent* Collection = ResolveCollection())
	{
		// Back to inert-and-hidden, in the reverse order ApplyRelease brought it on.
		Collection->SetSimulatePhysics(false);
		Collection->ObjectType = EObjectStateTypeEnum::Chaos_Object_Kinematic;
		Collection->SetHiddenInGame(true);
	}

	if (UStaticMeshComponent* Intact = ResolveIntactMesh())
	{
		Intact->SetHiddenInGame(false);
		Intact->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	UE_LOG(LogGSCrumble, Log, TEXT("[GoblinSiege] '%s' put back together."), *Owner->GetName());
}

// Console: put every crumbled thing in the play world back together, so the raid can be retried
// without restarting PIE. Cheat-only - this exists to make the feature testable, not playable.
static FAutoConsoleCommandWithWorld GSCrumbleResetAllCmd(
	TEXT("GS.Crumble.ResetAll"),
	TEXT("Put every crumbled monument and building back together (testing)."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		int32 Count = 0;
		for (TObjectIterator<UGSCrumbleComponent> It; It; ++It)
		{
			UGSCrumbleComponent* Crumble = *It;
			// World-filtered: without this an editor-world CDO or another PIE instance gets reset too.
			if (IsValid(Crumble) && Crumble->GetWorld() == World)
			{
				Crumble->ResetCrumble();
				++Count;
			}
		}
		UE_LOG(LogGSCrumble, Log, TEXT("[GoblinSiege] GS.Crumble.ResetAll restored %d thing(s)."), Count);
	}));
