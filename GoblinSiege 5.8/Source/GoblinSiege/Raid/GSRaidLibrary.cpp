#include "Raid/GSRaidLibrary.h"
#include "Raid/GSRaidMarker.h"
#include "Destruction/GSBurnObjectiveBase.h"
#include "Destruction/GSFlammableComponent.h"
#include "Destruction/GSBreakableComponent.h"
#include "Interaction/GSInteractableComponent.h"
#include "Destruction/GSBurnFXComponent.h"
#include "GameFramework/Actor.h"
#include "GameplayTagsManager.h"
#include "Engine/Engine.h"   // GEngine->GetWorldFromContextObject in FindStandableSpotNear
#include "Engine/World.h"    // LineTraceSingleByChannel / OverlapBlockingTestByChannel
// FOverlapResult is only FORWARD-DECLARED by Engine/World.h in UE 5.8, so a translation unit that
// iterates the results of an overlap query - rather than just calling one - has to include this
// itself. Omitting it fails as "use of undefined type", which reads like a missing module
// dependency rather than a missing header.
#include "Engine/OverlapResult.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSRaidScripting, Log, All);

FGameplayTag UGSRaidLibrary::MakeTagByName(FName TagName, bool bErrorIfNotFound)
{
	if (TagName.IsNone())
	{
		return FGameplayTag();
	}

	// RequestGameplayTag with ErrorIfNotFound=false, then decide ourselves whether to complain -
	// the engine's own assert on a missing tag would take the editor down, and a script mistyping
	// a tag deserves a log line, not a crash.
	const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(TagName, false);

	if (!Tag.IsValid() && bErrorIfNotFound)
	{
		UE_LOG(LogGSRaidScripting, Error,
			TEXT("[GoblinSiege] Gameplay tag '%s' is not registered. Check spelling against ")
			TEXT("Combat/GSGameplayTags.cpp - an empty tag leaves the carrier invisible to the ")
			TEXT("win condition, which looks like a working level that cannot be won."),
			*TagName.ToString());
	}

	return Tag;
}

bool UGSRaidLibrary::SetObjectiveIdentity(AGSBurnObjectiveBase* Objective, FName TypeTagName, FText DisplayName)
{
	if (!Objective)
	{
		return false;
	}

	const FGameplayTag Tag = MakeTagByName(TypeTagName, true);
	if (!Tag.IsValid())
	{
		return false;
	}

	Objective->SetObjectiveIdentity(Tag, DisplayName);

	UE_LOG(LogGSRaidScripting, Log, TEXT("[GoblinSiege] MODIFIED %s -> %s '%s'"),
		*Objective->GetName(), *TypeTagName.ToString(), *DisplayName.ToString());
	return true;
}

bool UGSRaidLibrary::ConfigureMarker(AGSRaidMarker* Marker, FName MarkerTypeName, FName GroupId,
	int32 OrderIndex, float Radius)
{
	if (!Marker)
	{
		return false;
	}

	const FGameplayTag Tag = MakeTagByName(MarkerTypeName, true);
	if (!Tag.IsValid())
	{
		return false;
	}

	Marker->Configure(Tag, GroupId, OrderIndex, Radius);
	return true;
}

namespace
{
	/**
	 * Give an actor its char, if it has none.
	 *
	 * Separated out and called on BOTH paths below, because the first version of this put it after
	 * MakeActorFlammable's idempotency guard - so an actor that ALREADY had a flammable component
	 * returned early and never got char. Re-running the dressing script on the 67 market stalls
	 * therefore fixed nothing, silently, which is the exact failure mode this whole area keeps
	 * producing.
	 */
	void EnsureBurnFX(AActor* Actor)
	{
		if (!Actor || Actor->FindComponentByClass<UGSBurnFXComponent>())
		{
			return;
		}

		UGSBurnFXComponent* FX = NewObject<UGSBurnFXComponent>(Actor, UGSBurnFXComponent::StaticClass(),
			TEXT("GSBurnFX"), RF_Transactional);
		if (FX)
		{
			Actor->AddInstanceComponent(FX);
			FX->RegisterComponent();
			Actor->MarkPackageDirty();
		}
	}
}

UGSFlammableComponent* UGSRaidLibrary::MakeActorFlammable(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	// Char first, and unconditionally - see EnsureBurnFX.
	EnsureBurnFX(Actor);

	// Idempotent. A dressing script gets re-run - after a crash, after a tweak, after a second pass
	// over a wider radius - and stacking a second flammable component on a stall would double every
	// spread roll it makes.
	if (UGSFlammableComponent* Existing = Actor->FindComponentByClass<UGSFlammableComponent>())
	{
		return Existing;
	}

	UGSFlammableComponent* Comp = NewObject<UGSFlammableComponent>(Actor, UGSFlammableComponent::StaticClass(),
		TEXT("GSFlammable"), RF_Transactional);
	if (!Comp)
	{
		return nullptr;
	}

	// AddInstanceComponent is the part that matters: without it the component is a transient runtime
	// object that vanishes on reload, so the level LOOKS dressed until you close the editor.
	Actor->AddInstanceComponent(Comp);
	Comp->RegisterComponent();

	Actor->MarkPackageDirty();

	return Comp;
}

UGSBreakableComponent* UGSRaidLibrary::MakeActorBreakable(AActor* Actor, bool bOpensBuilding)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (UGSBreakableComponent* Existing = Actor->FindComponentByClass<UGSBreakableComponent>())
	{
		return Existing;
	}

	UGSBreakableComponent* Comp = NewObject<UGSBreakableComponent>(Actor, UGSBreakableComponent::StaticClass(),
		TEXT("GSBreakable"), RF_Transactional);
	if (!Comp)
	{
		return nullptr;
	}

	// AddInstanceComponent is the whole point - see the header.
	Actor->AddInstanceComponent(Comp);
	Comp->RegisterComponent();
	Comp->SetOpensBuilding(bOpensBuilding);

	Actor->MarkPackageDirty();
	return Comp;
}

UGSInteractableComponent* UGSRaidLibrary::MakeActorCarryable(AActor* Actor, int32 LootValue,
	FText PromptText, float ChannelSeconds)
{
	if (!Actor)
	{
		return nullptr;
	}

	// Idempotent, and the guard matters more here than for flammable: these get run over whole herds
	// by script, and a re-run must not give one pig two prompts.
	if (UGSInteractableComponent* Existing = Actor->FindComponentByClass<UGSInteractableComponent>())
	{
		return Existing;
	}

	UGSInteractableComponent* Comp = NewObject<UGSInteractableComponent>(Actor,
		UGSInteractableComponent::StaticClass(), TEXT("GSCarryable"), RF_Transactional);
	if (!Comp)
	{
		return nullptr;
	}

	// AddInstanceComponent is the whole point - see the header.
	Actor->AddInstanceComponent(Comp);
	Comp->RegisterComponent();

	// WHERE the player interacts with it, derived from the actor rather than guessed. A decorative
	// animal's origin sits ON the terrain, and the interaction component's line-of-sight trace runs
	// eye -> interaction point, so an origin-height point is blocked by the ground ~15 uu short and
	// the focus is silently refused. Using the actor's bounds centre puts the point inside the body,
	// which is right for a pig, a sheep and a chicken without anyone tuning three numbers.
	FVector BoundsOrigin, BoundsExtent;
	Actor->GetActorBounds(/*bOnlyCollidingComponents*/ true, BoundsOrigin, BoundsExtent);
	const FVector LocalCentre = Actor->GetActorTransform().InverseTransformPosition(BoundsOrigin);

	// The component configures itself - its authored fields are protected, and one initialiser called
	// once at dressing time is the only moment any of them should change.
	Comp->InitialiseAsCarryable(LootValue, PromptText, ChannelSeconds, LocalCentre);

	Actor->MarkPackageDirty();
	return Comp;
}

int32 UGSRaidLibrary::CountCarryable(const TArray<AActor*>& Actors)
{
	int32 Count = 0;
	for (const AActor* Actor : Actors)
	{
		if (const UGSInteractableComponent* Comp =
			Actor ? Actor->FindComponentByClass<UGSInteractableComponent>() : nullptr)
		{
			if (Comp->IsCarryable())
			{
				++Count;
			}
		}
	}
	return Count;
}

int32 UGSRaidLibrary::CountBreakable(const TArray<AActor*>& Actors)
{
	int32 Count = 0;
	for (const AActor* Actor : Actors)
	{
		if (Actor && Actor->FindComponentByClass<UGSBreakableComponent>())
		{
			++Count;
		}
	}
	return Count;
}

int32 UGSRaidLibrary::CountFlammable(const TArray<AActor*>& Actors)
{
	int32 Count = 0;
	for (const AActor* Actor : Actors)
	{
		if (Actor && Actor->FindComponentByClass<UGSFlammableComponent>())
		{
			++Count;
		}
	}
	return Count;
}

namespace
{
	// Mirrors AGSRunicSite's spawn-search tuning. Restated as constants rather than shared, because
	// the site's values are EditAnywhere on a placed actor and this static helper has no actor to
	// read them from. A copy that visibly restates the numbers is better than one that silently
	// drifts from them - if these ever need to agree, they should agree through a shared struct, not
	// through nobody noticing.
	constexpr float StandTraceUpDistance   = 500.f;
	constexpr float StandTraceDownDistance = 5000.f;
	constexpr float StandCapsuleRadius     = 42.f;
	constexpr float StandCapsuleHalfHeight = 96.f;
	constexpr int32 StandBearingCount      = 8;

	/** Rings in uu; the first is the origin itself. Same fan shape the runic site uses. */
	constexpr float StandRingRadii[] = { 0.f, 300.f, 700.f, 1200.f, 2000.f };
}

bool UGSRaidLibrary::FindStandableSpotNear(const UObject* WorldContextObject, FVector Origin,
	FVector& OutSpot, const AActor* IgnoreActor)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSFindStandableSpot), false, IgnoreActor);
	const FCollisionShape Capsule =
		FCollisionShape::MakeCapsule(StandCapsuleRadius, StandCapsuleHalfHeight);

	for (const float Radius : StandRingRadii)
	{
		// One sample at the origin, then a fan at each radius, so the search grows outward evenly -
		// a drowning goblin should wash up at the NEAREST bank, not whichever axis happened to be
		// tested first.
		const int32 Samples = (Radius <= 0.f) ? 1 : StandBearingCount;
		for (int32 i = 0; i < Samples; ++i)
		{
			const float Angle = (2.f * PI * i) / FMath::Max(1, Samples);
			const FVector Candidate = Origin +
				FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);

			FHitResult Hit;
			const FVector Start = Candidate + FVector(0.f, 0.f, StandTraceUpDistance);
			const FVector End   = Candidate - FVector(0.f, 0.f, StandTraceDownDistance);
			if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
			{
				continue; // nothing under this bearing at all
			}

			// Would a pawn actually FIT? This is the half the runic site's first spawn fix lacked,
			// and the only check that can reject a point buried inside a building (#009).
			const FVector Centre = Hit.Location + FVector(0.f, 0.f, StandCapsuleHalfHeight + 10.f);
			if (World->OverlapBlockingTestByChannel(Centre, FQuat::Identity, ECC_Pawn, Capsule, Params))
			{
				continue;
			}

			OutSpot = Centre;
			return true;
		}
	}

	return false;
}

int32 UGSRaidLibrary::SmashBreakablesInArc(const UObject* WorldContextObject, AActor* Instigator,
	const FVector& Origin, float Radius, const FVector& Forward, float ArcDegrees,
	int32 Damage, TSet<TObjectPtr<AActor>>& AlreadyHit)
{
	if (Damage <= 0 || Radius <= 0.f)
	{
		return 0;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(
		WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return 0;
	}

	// WorldStatic AND WorldDynamic. A placed prop is usually WorldStatic; anything already simulating
	// - a crate someone pushed, a collection mid-break - is WorldDynamic. Missing the second would
	// make props stop being hittable at exactly the moment they became interesting.
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSSmashArc), false, Instigator);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjParams,
		FCollisionShape::MakeSphere(Radius), Params);

	const FVector FlatForward = FVector(Forward.X, Forward.Y, 0.f).GetSafeNormal();
	const float CosHalfArc = FMath::Cos(FMath::DegreesToRadians(ArcDegrees * 0.5f));
	const FVector SwingOrigin = Instigator ? Instigator->GetActorLocation() : Origin;

	int32 Smashed = 0;

	for (const FOverlapResult& O : Overlaps)
	{
		AActor* Target = O.GetActor();
		if (!Target || Target == Instigator || AlreadyHit.Contains(Target))
		{
			continue;
		}

		UGSBreakableComponent* Breakable = Target->FindComponentByClass<UGSBreakableComponent>();
		if (!Breakable || Breakable->IsBroken())
		{
			continue;
		}

		// Same flattened-dot arc test the character sweep uses. A sphere centred in front still
		// reaches behind the shoulders, and smashing a crate that is behind you reads as a bug even
		// when the maths is right.
		if (ArcDegrees < 360.f && !FlatForward.IsNearlyZero())
		{
			FVector ToTarget = Target->GetActorLocation() - SwingOrigin;
			ToTarget.Z = 0.f;
			if (!ToTarget.IsNearlyZero()
				&& FVector::DotProduct(ToTarget.GetSafeNormal(), FlatForward) < CosHalfArc)
			{
				continue;
			}
		}

		// Mark before applying: ApplySmash can destroy or otherwise invalidate the actor, and one
		// swing must never hit the same prop twice regardless of what the break does.
		AlreadyHit.Add(Target);

		const FVector ImpactPoint = O.GetComponent()
			? O.GetComponent()->GetComponentLocation()
			: Target->GetActorLocation();

		Breakable->ApplySmash(Damage, ImpactPoint, FlatForward * 500.f, Instigator);
		++Smashed;
	}

	return Smashed;
}
