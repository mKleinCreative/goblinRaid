#include "Weapons/GSGrappleHaulComponent.h"

#include "Destruction/GSTopplableComponent.h"
#include "Characters/GSCharacterBase.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSGrapple, Log, All);

UGSGrappleHaulComponent::UGSGrappleHaulComponent()
{
	// Ticks only while hauling; enabled in NotifyHookAttached and switched off again on end, so an
	// idle player pays nothing for a rope they are not pulling.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UGSGrappleHaulComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AGSCharacterBase* Character = Cast<AGSCharacterBase>(GetOwner()))
	{
		Character->OnHealthChanged.AddDynamic(this, &UGSGrappleHaulComponent::HandleOwnerHealthChanged);
	}
}

void UGSGrappleHaulComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AGSCharacterBase* Character = Cast<AGSCharacterBase>(GetOwner()))
	{
		Character->OnHealthChanged.RemoveDynamic(this, &UGSGrappleHaulComponent::HandleOwnerHealthChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void UGSGrappleHaulComponent::HandleOwnerHealthChanged(float /*NewHealth*/, float /*MaxHealth*/, float Delta)
{
	// Only damage. Healing mid-haul is not a reason to drop a rope, and Delta is the only thing that
	// distinguishes them - the same test UGSInteractionComponent uses to abort a channel.
	if (Delta < 0.f && bHauling)
	{
		UE_LOG(LogGSGrapple, Log, TEXT("[GoblinSiege] Grip broken by damage at %.0f%% - rope dropped."),
			HaulProgress * 100.f);

		EndHaul(false);
	}
}

void UGSGrappleHaulComponent::NotifyHookAttached(AActor* InHookActor, AActor* HitActor, const FVector& AnchorPoint)
{
	// TEMP DEBUG (2026-08-31, live grapple diagnosis with Michael) - the IsValid check right below
	// this was a completely silent early-return, no log either way, which is exactly the "refuses
	// correctly and says nothing at all" failure this class's own header warns about. Remove once
	// the grapple-to-objective connection is confirmed working end to end.
	UE_LOG(LogGSGrapple, Warning, TEXT("[GS.Grapple.DEBUG] NotifyHookAttached called. InHookActor=%s HitActor=%s"),
		InHookActor ? *InHookActor->GetName() : TEXT("NULL"),
		HitActor ? *HitActor->GetName() : TEXT("NULL"));

	NotifyHookDetached();   // a second hook supersedes the first

	AActor* Owner = GetOwner();
	if (!IsValid(HitActor) || !IsValid(Owner))
	{
		UE_LOG(LogGSGrapple, Warning, TEXT("[GS.Grapple.DEBUG] Bailing: HitActor valid=%d Owner valid=%d"),
			IsValid(HitActor), IsValid(Owner));
		return;
	}

	// Tracked even when the target turns out not to be topplable, so a hook in a wall is still a hook
	// somebody can throw away by throwing again.
	HookActor = InHookActor;

	// Range is judged at the moment of the bite, before anything else, so an over-long throw fails the
	// same way whatever it hit - a distant statue and a distant wall both simply do not hold.
	const float BiteDistance = FVector::Dist(Owner->GetActorLocation(), AnchorPoint);
	if (MaxGrappleDistanceUU > 0.f && BiteDistance > MaxGrappleDistanceUU)
	{
		UE_LOG(LogGSGrapple, Log,
			TEXT("[GoblinSiege] Hook fell away: bit '%s' at %.0fuu, past the %.0fuu limit."),
			*HitActor->GetName(), BiteDistance, MaxGrappleDistanceUU);

		ReleaseHook();
		return;
	}

	// The rope's reach limit applies the moment ANY hook bites, haul or no haul - Michael, live
	// playtest 2026-08-31: "you're able to lay out the rope segment for much longer than you should
	// be able to." Before this, HaulAnchorPoint/AnchorDistanceAtAttach and the tick that reads them
	// (ConstrainToRope, below) were only ever set on the haul path - a hook stuck in a plain wall (the
	// class header's own "movement tool first" case, and the MOST common throw) enabled no clamp at
	// all, so the rope paid out forever. MaxRopeStretchUU was already written generically ("How far
	// away the hook may bite" / "how far ... before it stops you") for exactly this dual purpose.
	HaulAnchorPoint = AnchorPoint;
	AnchorDistanceAtAttach = BiteDistance;
	SetComponentTickEnabled(true);

	UGSTopplableComponent* Topplable = HitActor->FindComponentByClass<UGSTopplableComponent>();

	// Hooking a wall is legal and common - the grapple is a movement tool first, so this stays quiet
	// and the hook stays put. The rope still holds the player to it (see above); it just never starts
	// a haul.
	if (!Topplable)
	{
		// TEMP DEBUG: bumped from Verbose to Warning so it's guaranteed visible while diagnosing.
		UE_LOG(LogGSGrapple, Warning, TEXT("[GoblinSiege] Hook bit '%s' - not a monument, no haul."),
			*HitActor->GetName());
		return;
	}

	// Already down. SEPARATED from the case above on purpose: these were one silent gate, and merging
	// "this is a wall" with "this idol is already rubble" is what made the second throw unexplainable.
	if (Topplable->IsToppled())
	{
		UE_LOG(LogGSGrapple, Log,
			TEXT("[GoblinSiege] '%s' is already down - nothing left to haul on."), *HitActor->GetName());

		OnHaulRefused.Broadcast(HitActor);
		ReleaseHook();   // do not leave a rope strung to a pile of rubble
		return;
	}

	// HaulAnchorPoint/AnchorDistanceAtAttach/tick were already set above, identically for both the
	// haul and plain-anchor cases - not repeated here.
	HaulTarget = HitActor;
	HaulTopplable = Topplable;
	HaulDuration = FMath::Max(Topplable->GetHaulSeconds(), 0.1f);
	HaulProgress = 0.f;
	bHauling = true;

	UE_LOG(LogGSGrapple, Log,
		TEXT("[GoblinSiege] Haul started on '%s' - anchored %.0fuu away, needs %.1fs of tension."),
		*HitActor->GetName(), AnchorDistanceAtAttach, HaulDuration);

	OnHaulStarted.Broadcast(HitActor, HaulDuration);
}

void UGSGrappleHaulComponent::NotifyHookDetached()
{
	if (bHauling)
	{
		EndHaul(false);   // which releases the hook itself
		return;
	}

	// Not hauling, but there may still be a hook in a wall. Most throws are exactly this case - the
	// grapple is a movement tool first - so releasing only on the haul path would leave a rope hanging
	// off the scenery after every throw that did not happen to find a monument.
	ReleaseHook();
}

void UGSGrappleHaulComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Ticks whenever ANY hook is attached now, not only while hauling - see the comment in
	// NotifyHookAttached this pairs with. A plain anchor hook (no UGSTopplableComponent on what it
	// hit) still needs ConstrainToRope every frame, or the rope pays out forever.
	if (!HookActor.IsValid())
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	// The rope holds you before anything else happens this frame, whether or not there is a haul in
	// progress - tension (below) is measured against a position the rope actually permits, and a
	// plain anchor hook has nothing else to do this tick besides this call.
	ConstrainToRope(Owner);

	if (!bHauling)
	{
		return;
	}

	UGSTopplableComponent* Topplable = HaulTopplable.Get();

	// The statue can be destroyed by something else mid-haul - another goblin, a torch. Ending rather
	// than continuing to measure against a dead actor.
	if (!Topplable || !HaulTarget.IsValid() || Topplable->IsToppled())
	{
		EndHaul(false);
		return;
	}

	const float Distance = FVector::Dist(Owner->GetActorLocation(), HaulAnchorPoint);
	const bool bTaut = Distance > AnchorDistanceAtAttach + TautSlackUU;

	if (bTaut)
	{
		HaulProgress = FMath::Clamp(HaulProgress + DeltaTime / HaulDuration, 0.f, 1.f);
	}
	else
	{
		// Slack. Decays rather than resets: a stumble backwards should not throw away the heave, but
		// standing still should not hold it either.
		HaulProgress = FMath::Clamp(HaulProgress - DeltaTime * SlackDecayPerSecond, 0.f, 1.f);
	}

	OnHaulProgress.Broadcast(HaulProgress);

	if (HaulProgress >= 1.f)
	{
		// Pull direction is anchor-to-player, so the monument comes down TOWARDS whoever hauled it.
		// That is the shot: you back away, it follows you over.
		const FVector PullDirection = (Owner->GetActorLocation() - HaulAnchorPoint).GetSafeNormal();

		Topplable->Topple(Owner, PullDirection, HaulAnchorPoint);
		EndHaul(true);
	}
}

void UGSGrappleHaulComponent::ConstrainToRope(AActor* Owner)
{
	const float RopeLimit = AnchorDistanceAtAttach + MaxRopeStretchUU;

	// HORIZONTAL only, and this is the important part. Clamping in all three axes would drag the
	// hauler up or down towards the anchor - and the anchor is high on a statue, so leaning back at
	// the end of the rope would hoist a goblin off the ground. The player is walking on a floor; the
	// rope's job is to stop them walking AWAY, not to lift them.
	FVector Location = Owner->GetActorLocation();
	FVector Flat = Location - HaulAnchorPoint;
	Flat.Z = 0.f;

	const float FlatDistance = Flat.Size();
	if (FlatDistance <= RopeLimit || FlatDistance < KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector Direction = Flat / FlatDistance;
	Location.X = HaulAnchorPoint.X + Direction.X * RopeLimit;
	Location.Y = HaulAnchorPoint.Y + Direction.Y * RopeLimit;

	// Swept, so the clamp cannot post the hauler through a wall they were backed against.
	Owner->SetActorLocation(Location, /*bSweep*/ true);

	// Kill only the OUTWARD part of the velocity. Zeroing it wholesale would freeze a player who is
	// trying to walk sideways around the monument, which is legal and is how you change the direction
	// it falls - and the movement component would otherwise keep pressing them into the clamp every
	// frame, which reads as juddering rather than as a taut line.
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			const float Outward = FVector::DotProduct(Movement->Velocity, Direction);
			if (Outward > 0.f)
			{
				Movement->Velocity -= Direction * Outward;
			}
		}
	}
}

void UGSGrappleHaulComponent::EndHaul(bool bCompleted)
{
	if (!bHauling)
	{
		return;
	}

	bHauling = false;
	HaulProgress = 0.f;
	HaulTarget.Reset();
	HaulTopplable.Reset();
	// Tick is disabled in ReleaseHook (below) now, not here - a haul ending must not disable the
	// tick that a plain anchor hook still needs (see NotifyHookAttached/TickComponent).

	// The rope goes with the haul, whether it succeeded or not. On success the monument is already
	// coming down and the hook is buried in something that no longer exists as a standing object; on
	// failure the player has let go. Either way, leaving the hook in the world leaves a rope stretching
	// to a pile of rubble.
	ReleaseHook();

	OnHaulEnded.Broadcast(bCompleted);
}

void UGSGrappleHaulComponent::ReleaseHook()
{
	if (AActor* Hook = HookActor.Get())
	{
		// Destroying the hook takes the rope with it - AGSGrappleHookProjectile's InstancedStaticMesh
		// rope segments live on the hook actor, so there is no separate visual to tidy up.
		Hook->Destroy();
	}
	HookActor.Reset();

	// The one true "nothing to hold onto anymore" point - see TickComponent, which now ticks for
	// as long as ANY hook is attached, haul or plain anchor.
	SetComponentTickEnabled(false);
}
