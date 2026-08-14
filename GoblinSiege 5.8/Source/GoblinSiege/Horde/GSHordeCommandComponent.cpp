#include "Horde/GSHordeCommandComponent.h"

#include "Horde/GSHordeSubsystem.h"
#include "Characters/GSCharacterBase.h"
#include "Interaction/GSInteractableComponent.h"
#include "Destruction/GSBreakableComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "CollisionQueryParams.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSHordeOrderWheel, Log, All);

#define LOCTEXT_NAMESPACE "GSHordeOrderWheel"

UGSHordeCommandComponent::UGSHordeCommandComponent()
{
	// Ticks for ONE reason: keeping the reticle honest about what is under the crosshair (#149).
	// The wheel itself is still entirely input-driven and does nothing between a press and a release.
	// Throttled to CrosshairScanIntervalSeconds inside TickCrosshair, and early-outs on anything that
	// is not a locally controlled pawn, so a dedicated server and every remote proxy pay nothing.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// Required for ServerIssueOrder to route: an RPC on a component only reaches the server if the
	// component itself is registered as replicated, even when none of its properties are.
	SetIsReplicatedByDefault(true);
}

void UGSHordeCommandComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickCrosshair();
}

// ---------------------------------------------------------------------------------------------
// The live crosshair (#149)
//
// Michael, after using the wheel for the first time: "it's hard to attempt to aim a command at
// something", then "make the reticle turn gold on a valid target".
//
// This runs the SAME resolution the latch uses - TraceForOrder, which resolves through
// ResolveOrderSubject - so the reticle cannot promise a target the order would then refuse. Sharing
// the function rather than re-deriving "is this orderable" is the whole point: two answers to that
// question is how a reticle starts lying, and this project has already been bitten by a lying
// instrument twice this session.
// ---------------------------------------------------------------------------------------------

void UGSHordeCommandComponent::TickCrosshair()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();

	// Locally controlled only. A dedicated server has no reticle, a remote proxy's crosshair is
	// somebody else's business, and neither should be paying for a sweep.
	if (!Pawn || !World || !Pawn->IsLocallyControlled())
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (Now < NextCrosshairScanTime)
	{
		return;
	}
	NextCrosshairScanTime = Now + FMath::Max(CrosshairScanIntervalSeconds, 0.f);

	// While the wheel is open the latch is frozen by design - the target was captured on the press
	// and must not drift while the player drags. Reporting a live scan here would have the reticle
	// disagree with the order about to be issued.
	if (bWheelOpen)
	{
		return;
	}

	FVector Location = FVector::ZeroVector;
	AActor* Subject = nullptr;
	TraceForOrder(World, Pawn, OrderTraceDistance, OrderTraceRadius, Location, Subject);

	const bool bNowHasTarget = (Subject != nullptr);
	if (bNowHasTarget == bHasCrosshairTarget && Subject == CrosshairTarget.Get())
	{
		return;
	}

	bHasCrosshairTarget = bNowHasTarget;
	CrosshairTarget = Subject;
	OnCrosshairTargetChanged.Broadcast(bHasCrosshairTarget, Subject);
}

// ---------------------------------------------------------------------------------------------
// The wheel. Mirrors UGSWeaponComponent::SlotForDirection / OpenWeaponWheel / AddWheelInput /
// CloseWeaponWheel - see the header for why this is a clone rather than a shared base.
// ---------------------------------------------------------------------------------------------

EGSHordeOrder UGSHordeCommandComponent::OrderForDirection(FVector2D Direction, EGSHordeOrder Fallback) const
{
	if (Direction.Size() < WheelDeadZone)
	{
		return Fallback;
	}

	// Screen space has +Y DOWN, so negate it for a conventional maths angle where up is +90. Getting
	// this backwards mirrors the wheel top-to-bottom, which reads as "the sectors feel wrong" rather
	// than as an error - and here it would swap Attack with Follow, the one confusion the layout is
	// arranged to prevent.
	const float Degrees = FRotator::ClampAxis(
		FMath::RadiansToDegrees(FMath::Atan2(-Direction.Y, Direction.X)));

	// Four 90-degree sectors centred on 90 (up), 180 (left), 270 (down) and 0 (right).
	if (Degrees >= 45.f && Degrees < 135.f)  { return EGSHordeOrder::Attack; }
	if (Degrees >= 135.f && Degrees < 225.f) { return EGSHordeOrder::Loot; }
	if (Degrees >= 225.f && Degrees < 315.f) { return EGSHordeOrder::Follow; }
	return EGSHordeOrder::Hold;
}

bool UGSHordeCommandComponent::OpenOrderWheel()
{
	if (bWheelOpen)
	{
		return true;
	}

	// Latch FIRST. The whole design of this gesture is that the target is what you were looking at
	// when you reached for the key, not where the camera happened to end up after you dragged - and
	// the drag is about to start swallowing look input, so there is no later moment at which the
	// camera still points where the player meant.
	if (!LatchTargetUnderCamera())
	{
		UE_LOG(LogGSHordeOrderWheel, Verbose,
			TEXT("Order wheel refused to open: the camera trace found nowhere to send anybody."));
		return false;
	}

	bWheelOpen = true;
	WheelAccum = FVector2D::ZeroVector;

	// Starts on None, which is also the dead-zone fallback - so a tap with no drag commits nothing.
	// The weapon wheel gets the same "no-op release" property by starting on the slot already held;
	// here there is no standing verb to start from, so None does the job explicitly.
	WheelHighlight = EGSHordeOrder::None;

	OnOrderWheelOpenChanged.Broadcast(true);
	OnOrderWheelHighlightChanged.Broadcast(WheelHighlight);
	return true;
}

void UGSHordeCommandComponent::AddWheelInput(FVector2D Delta)
{
	if (!bWheelOpen)
	{
		return;
	}
	WheelAccum += Delta;

	const EGSHordeOrder NewHighlight = OrderForDirection(WheelAccum, EGSHordeOrder::None);
	if (NewHighlight != WheelHighlight)
	{
		WheelHighlight = NewHighlight;
		// On change only - a per-frame broadcast would have the widget repainting 120 times a second
		// to draw the same four labels.
		OnOrderWheelHighlightChanged.Broadcast(WheelHighlight);
	}
}

void UGSHordeCommandComponent::CloseOrderWheel(bool bCommit)
{
	if (!bWheelOpen)
	{
		return;
	}
	bWheelOpen = false;

	const EGSHordeOrder Chosen = bCommit ? WheelHighlight : EGSHordeOrder::None;
	const FVector CommitLocation = LatchedLocation;
	AActor* CommitSubject = LatchedSubject.Get();

	WheelAccum = FVector2D::ZeroVector;
	WheelHighlight = EGSHordeOrder::None;
	OnOrderWheelOpenChanged.Broadcast(false);

	// Released inside the dead zone, or aborted. Nothing is issued and the standing order is left
	// exactly as it was - cancelling must not clear an order the player gave a minute ago.
	if (Chosen == EGSHordeOrder::None)
	{
		return;
	}

	ServerIssueOrder(Chosen, CommitSubject, CommitLocation);
}

// ---------------------------------------------------------------------------------------------
// The latch
// ---------------------------------------------------------------------------------------------

bool UGSHordeCommandComponent::TraceForOrder(UWorld* World, APawn* Asker, float Distance, float Radius,
	FVector& OutLocation, AActor*& OutSubject)
{
	OutLocation = FVector::ZeroVector;
	OutSubject = nullptr;

	if (!World || !Asker)
	{
		return false;
	}

	APlayerController* PC = Cast<APlayerController>(Asker->GetController());
	if (!PC)
	{
		return false;
	}

	// The CAMERA's viewpoint, not the pawn's eyes. In a third-person over-the-shoulder rig those are
	// metres apart, and the player is aiming with the former.
	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * Distance;

	FCollisionQueryParams Params(TEXT("GSHordeOrderTrace"), /*bTraceComplex=*/ false, Asker);

	// ---- 1. LOOK FOR A SUBJECT, BY OBJECT TYPE, NOT BY VISIBILITY CHANNEL --------------------
	//
	// THIS IS THE FIX FOR THE BUG MICHAEL HIT ALL SESSION. The previous version swept a sphere on
	// ECC_Visibility and walked the hits looking for something orderable. That reads as though it
	// should work and cannot: SweepMulti stops at the first BLOCKING hit, the landscape blocks
	// Visibility, and a third-person camera is pitched slightly downward - so on flat ground the
	// sphere clips terrain within a few metres and the sweep never reaches the man being aimed at.
	// Every single Attack and Loot came back "the trace found bare ground", including through
	// GS.Horde.Order, which is why the console path lied in exactly the same way as the wheel.
	//
	// Querying by OBJECT TYPE cannot be blocked by the landscape at all, because the landscape is
	// WorldStatic and is simply not in the query. Pawn covers defenders and goblins; WorldDynamic and
	// PhysicsBody cover loot sacks, crates and anything else a designer drops in as a carryable or a
	// breakable.
	FCollisionObjectQueryParams SubjectTypes;
	SubjectTypes.AddObjectTypesToQuery(ECC_Pawn);
	SubjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	SubjectTypes.AddObjectTypesToQuery(ECC_PhysicsBody);

	TArray<FHitResult> SubjectHits;
	World->SweepMultiByObjectType(SubjectHits, ViewLocation, TraceEnd, FQuat::Identity,
		SubjectTypes, FCollisionShape::MakeSphere(FMath::Max(Radius, 1.f)), Params);

	// Hits come back in order along the sweep, so the first ORDERABLE one is the nearest thing worth
	// pointing at. Walking rather than taking [0] matters because the sweep will happily return the
	// player's own goblins, and ResolveOrderSubject rejects those on race - without the walk, aiming
	// past your own warband at a guard would resolve to a goblin and be refused.
	for (const FHitResult& Hit : SubjectHits)
	{
		if (AActor* Subject = ResolveOrderSubject(Hit.GetActor(), Asker))
		{
			OutSubject = Subject;
			// The subject's own location, not the sweep's impact point. A sphere clipping a guard's
			// shoulder impacts out at arm's length; the beacon belongs at his feet.
			OutLocation = Subject->GetActorLocation();
			return true;
		}
	}

	// ---- 2. NO SUBJECT: A GENUINE POINT AT THE WORLD ------------------------------------------
	// A line, not a sphere, and now it is the right tool: this only has to answer "where on the
	// ground", which is what Hold wants, and a line gives a precise point instead of a fat contact.
	FHitResult WorldHit;
	if (World->LineTraceSingleByChannel(WorldHit, ViewLocation, TraceEnd, ECC_Visibility, Params))
	{
		OutLocation = WorldHit.ImpactPoint;
		return true;
	}

	// ---- 3. POINTING AT THE SKY ---------------------------------------------------------------
	// Drop straight down from the end of the trace, so "hold that ridge over there" still works when
	// the crosshair rides above the horizon.
	if (World->LineTraceSingleByChannel(WorldHit, TraceEnd,
		TraceEnd - FVector(0.f, 0.f, Distance), ECC_Visibility, Params))
	{
		OutLocation = WorldHit.ImpactPoint;
		return true;
	}

	return false;
}

bool UGSHordeCommandComponent::LatchTargetUnderCamera()
{
	bHasLatch = false;
	LatchedSubject.Reset();
	LatchedLocation = FVector::ZeroVector;

	APawn* Pawn = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();

	FVector Location = FVector::ZeroVector;
	AActor* Subject = nullptr;
	if (!TraceForOrder(World, Pawn, OrderTraceDistance, OrderTraceRadius, Location, Subject))
	{
		return false;
	}

	LatchedLocation = Location;
	LatchedSubject = Subject;
	bHasLatch = true;
	return true;
}

AActor* UGSHordeCommandComponent::ResolveOrderSubject(AActor* HitActor, const AActor* Asker)
{
	if (!IsValid(HitActor))
	{
		return nullptr;
	}

	// A living hostile. The race test is the same one UGSHordeSubsystem::ScanForThreats applies, and
	// it matters here for a reason that is easy to miss: without it, ordering an attack while the
	// crosshair drifts over one of your own goblins would register a fellow goblin as a threat and
	// the warband would turn on itself.
	if (const AGSCharacterBase* AsCharacter = Cast<AGSCharacterBase>(HitActor))
	{
		if (!AsCharacter->IsAlive())
		{
			return nullptr;
		}

		const AGSCharacterBase* AskerCharacter = Cast<AGSCharacterBase>(Asker);
		if (AskerCharacter && !AskerCharacter->IsHostileTo(HitActor))
		{
			return nullptr;
		}
		return HitActor;
	}

	// A sack, a pig, a rope chain - anything the carry framework already understands.
	if (const UGSInteractableComponent* Interactable = HitActor->FindComponentByClass<UGSInteractableComponent>())
	{
		if (Interactable->IsCarryable())
		{
			return HitActor;
		}
	}

	// A crate, a fence, a stall. Already-broken pieces are not worth ordering an attack on.
	if (const UGSBreakableComponent* Breakable = HitActor->FindComponentByClass<UGSBreakableComponent>())
	{
		if (!Breakable->IsBroken())
		{
			return HitActor;
		}
	}

	// Scenery. The order degrades to location-only rather than being refused.
	return nullptr;
}

FText UGSHordeCommandComponent::GetLatchedSubjectLabel() const
{
	const AActor* Subject = LatchedSubject.Get();
	if (!Subject)
	{
		return LOCTEXT("OrderSubjectGround", "the ground");
	}

	// The CLASS name, cleaned, rather than the instance name: "CastleGuard01" is a thing the player
	// can recognise, "BP_CastleGuard01_C_2" is not. Deliberately not a designer-facing display name
	// field - inventing one would mean touching every adversary Blueprint for a debug-grade label,
	// and the day this wants real names it should read them off the race/archetype data.
	FString Name = Subject->GetClass()->GetName();
	Name.RemoveFromEnd(TEXT("_C"));
	Name.RemoveFromStart(TEXT("BP_"));
	Name.RemoveFromStart(TEXT("SK_"));
	Name.RemoveFromStart(TEXT("SM_"));
	return FText::FromString(Name);
}

// ---------------------------------------------------------------------------------------------
// The one network hop
// ---------------------------------------------------------------------------------------------

void UGSHordeCommandComponent::ServerIssueOrder_Implementation(EGSHordeOrder InVerb, AActor* InSubject, FVector InLocation)
{
	// FVector rather than FVector_NetQuantize10 on purpose. Quantising would save a handful of bytes
	// on a message the player sends a few times a minute, against a real cost: the correct include for
	// the quantised types has moved between engine versions, and on this machine finding that out is a
	// six-minute editor-closed build.

	AActor* Owner = GetOwner();
	APawn* Pawn = Cast<APawn>(Owner);
	if (!Pawn)
	{
		return;
	}

	AController* Summoner = Pawn->GetController();
	if (!Summoner)
	{
		return;
	}

	if (InVerb == EGSHordeOrder::None)
	{
		return;
	}

	// A client-supplied location is untrusted input. This is not an anti-cheat measure so much as a
	// correctness one: a stale trace from a laggy client, or a pawn that teleported between press and
	// release, would otherwise plant a beacon on the far side of the map.
	if (FVector::Dist(InLocation, Pawn->GetActorLocation()) > MaxOrderRange)
	{
		UE_LOG(LogGSHordeOrderWheel, Warning,
			TEXT("Rejected an order from %s: %.0fuu away, limit %.0f."),
			*GetNameSafe(Owner), FVector::Dist(InLocation, Pawn->GetActorLocation()), MaxOrderRange);
		return;
	}

	// Re-resolve the nominated subject server-side. The client picked it up to a round-trip ago and
	// it may have died, been broken, or been picked up by somebody else in the meantime.
	AActor* Subject = ResolveOrderSubject(InSubject, Pawn);

	if (UGSHordeSubsystem* Horde = UGSHordeSubsystem::Get(this))
	{
		Horde->IssueOrder(Summoner, InVerb, Subject, InLocation);
	}
}

#undef LOCTEXT_NAMESPACE
