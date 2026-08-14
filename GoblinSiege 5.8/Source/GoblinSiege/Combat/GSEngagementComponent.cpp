#include "Combat/GSEngagementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Characters/GSCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Combat/GSGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UGSEngagementComponent::UGSEngagementComponent()
{
	// Nothing here ticks. Every piece of state is touched only when someone asks a question, and
	// PruneStale keeps it honest at that moment - a per-frame tick on every combatant would be a
	// real cost for a component that spends most of a raid idle.
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UGSEngagementComponent::BeginPlay()
{
	Super::BeginPlay();
	RingSlots.SetNum(FMath::Max(1, RingSlotCount));
}

float UGSEngagementComponent::NowSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

void UGSEngagementComponent::ConfigureLimits(int32 InTokenBudget, int32 InMaxEngaged)
{
	if (InTokenBudget > 0)
	{
		TokenBudget = InTokenBudget;
	}
	if (InMaxEngaged > 0)
	{
		MaxEngagedAttackers = InMaxEngaged;
	}
}

/** A corpse is not an attacker. Weak pointers are NOT enough on their own here: CorpseLifespan
 *  defaults to 0 ("never destroy"), so a dead goblin's actor stays valid indefinitely and would hold
 *  its grant until the watchdog swept it three seconds later - three seconds of a victim's budget
 *  spent on someone who is lying on the floor. */
static bool GSIsDeadOrGone(const TWeakObjectPtr<AActor>& Weak)
{
	const AActor* Actor = Weak.Get();
	if (!IsValid(Actor))
	{
		return true;
	}
	const AGSCharacterBase* Character = Cast<AGSCharacterBase>(Actor);
	return Character && !Character->IsAlive();
}

void UGSEngagementComponent::PruneStale()
{
	const float Now = NowSeconds();

	Grants.RemoveAll([this, Now](const FGSAttackGrant& Grant)
		{
			if (GSIsDeadOrGone(Grant.Holder))
			{
				return true;
			}
			// The watchdog deliberately ignores bLocked: a LOCKED grant that has outlived the
			// watchdog is the worst case, not an exempt one - it means an attack started, never
			// reached its recovery notify, and would otherwise hold the budget forever.
			return (Now - Grant.GrantedTime) > TokenWatchdogSeconds;
		});

	Engaged.RemoveAll([](const TWeakObjectPtr<AActor>& Actor) { return GSIsDeadOrGone(Actor); });

	for (FGSRingSlot& Slot : RingSlots)
	{
		if (GSIsDeadOrGone(Slot.Claimant))
		{
			Slot.Claimant = nullptr;
			continue;
		}
		if (SlotClaimTimeoutSeconds > 0.f && (Now - Slot.ClaimedTime) > SlotClaimTimeoutSeconds)
		{
			// Claimed but never arrived - stuck on geometry, or pathing around the long way. Free
			// it rather than let one agent hold a place in the ring for the rest of the fight.
			Slot.Claimant = nullptr;
		}
	}
}

int32 UGSEngagementComponent::RelevancePriority(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return 0;
	}

	int32 Priority = 0;
	if (Actor->WasRecentlyRendered(0.5f))
	{
		Priority += 2;
	}

	// Distance to the viewer, not to the player pawn: what matters is whether this fight is in
	// frame, and in a cinematic or a death cam those are different things.
	if (GEngine && Actor->GetWorld())
	{
		if (const APlayerController* PC = GEngine->GetFirstLocalPlayerController(Actor->GetWorld()))
		{
			FVector ViewLoc;
			FRotator ViewRot;
			PC->GetPlayerViewPoint(ViewLoc, ViewRot);
			if (FVector::DistSquared(ViewLoc, Actor->GetActorLocation()) < FMath::Square(1500.f))
			{
				Priority += 1;
			}
		}
	}
	return Priority;
}

bool UGSEngagementComponent::CanBeAttacked(bool bRecoilCountsAsOpening) const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return false;
	}

	const UAbilitySystemComponent* ASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	if (!ASC)
	{
		// No ability system means nothing can be read about its state; refusing to gate is the safe
		// answer - it keeps non-GAS actors (a target dummy, a breakable) hittable.
		return true;
	}

	const bool bRecoiling = ASC->HasMatchingGameplayTag(GSTags::State_Recoil);

	// Recoil is the odd one out: it is a punishable OPENING, not a mercy (#087). Only the melee
	// punish passes true here; token acquisition and everyone else still treat it as a veto, so a
	// recoiling target cannot attract a fresh crowd mid-window.
	const bool bIsTheOpening = bRecoilCountsAsOpening && bRecoiling;

	// Death and a broken guard veto absolutely, whoever is asking.
	if (ASC->HasMatchingGameplayTag(GSTags::State_Dead)
		|| ASC->HasMatchingGameplayTag(GSTags::State_GuardBroken))
	{
		return false;
	}

	// HitReact normally vetoes too - piling onto a flinching target is the deletion this component
	// exists to prevent. But during THE opening it must not, and PIE is what proved it: 17 blocks
	// landed and the punish fired 0 times, because AGSCharacterBase::NotifyAttackWasBlocked ends by
	// calling PlayHitReact to sell the clang, and PlayHitReact adds State.HitReact. A recoiling
	// fighter is therefore ALWAYS also flinching, so exempting recoil alone just moved the veto from
	// one tag to the other and the punish stayed dead code.
	//
	// That flinch is the blocked swing's OWN flinch - the same event, not evidence of a second
	// attacker - so it cannot be grounds for refusing the punish that the same event created. A
	// genuine third-party hit landing inside the 0.6s window is indistinguishable from it here and
	// will also be allowed through; that is accepted, because a recoiling target is punishable by
	// design for exactly that window.
	if (!bIsTheOpening && ASC->HasMatchingGameplayTag(GSTags::State_HitReact))
	{
		return false;
	}

	return bRecoilCountsAsOpening || !bRecoiling;
}

int32 UGSEngagementComponent::GetReservedWeight() const
{
	int32 Total = 0;
	for (const FGSAttackGrant& Grant : Grants)
	{
		if (!GSIsDeadOrGone(Grant.Holder))
		{
			Total += Grant.Cost;
		}
	}
	return Total;
}

int32 UGSEngagementComponent::GetAttackerCount() const
{
	int32 Count = 0;
	for (const FGSAttackGrant& Grant : Grants)
	{
		if (!GSIsDeadOrGone(Grant.Holder))
		{
			++Count;
		}
	}
	return Count;
}

bool UGSEngagementComponent::HoldsToken(AActor* Requester) const
{
	for (const FGSAttackGrant& Grant : Grants)
	{
		if (Grant.Holder.Get() == Requester)
		{
			return true;
		}
	}
	return false;
}

bool UGSEngagementComponent::TryAcquireToken(AActor* Requester, int32 Cost)
{
	if (!IsValid(Requester) || Cost <= 0)
	{
		return false;
	}

	PruneStale();

	if (!CanBeAttacked())
	{
		return false;
	}

	// Re-price an existing grant rather than refusing it. An attacker escalating from a light to a
	// heavy already has permission; making it release first would put it at the back of the queue
	// behind whoever was waiting, for no design reason.
	int32 ExistingIndex = INDEX_NONE;
	int32 ReservedByOthers = 0;
	for (int32 i = 0; i < Grants.Num(); ++i)
	{
		if (Grants[i].Holder.Get() == Requester)
		{
			ExistingIndex = i;
		}
		else if (Grants[i].Holder.IsValid())
		{
			ReservedByOthers += Grants[i].Cost;
		}
	}

	if (ReservedByOthers + Cost <= TokenBudget)
	{
		if (ExistingIndex != INDEX_NONE)
		{
			Grants[ExistingIndex].Cost = Cost;
			Grants[ExistingIndex].GrantedTime = NowSeconds();
			return true;
		}

		FGSAttackGrant Added;
		Added.Holder = Requester;
		Added.Cost = Cost;
		Added.GrantedTime = NowSeconds();
		Grants.Add(Added);
		return true;
	}

	// ---- preemption ------------------------------------------------------------------------
	// The budget is full. An on-screen attacker may take a grant from an off-screen one, so the
	// fight the player is watching never stalls because four goblins behind him got there first.
	// Only UNLOCKED grants can be taken: a swing already in its committed frames is an animation
	// the player can see, and rewinding it would look worse than the wait it saves.
	const int32 RequesterPriority = RelevancePriority(Requester);
	int32 FreedWeight = 0;
	TArray<int32> Victims;

	for (int32 i = 0; i < Grants.Num(); ++i)
	{
		const FGSAttackGrant& Grant = Grants[i];
		if (!Grant.Holder.IsValid() || Grant.bLocked || Grant.Holder.Get() == Requester)
		{
			continue;
		}
		if (RelevancePriority(Grant.Holder.Get()) >= RequesterPriority)
		{
			continue;
		}
		Victims.Add(i);
		FreedWeight += Grant.Cost;

		if (ReservedByOthers - FreedWeight + Cost <= TokenBudget)
		{
			break;
		}
	}

	if (ReservedByOthers - FreedWeight + Cost > TokenBudget)
	{
		return false;
	}

	// Descending so the indices stay valid as they are removed.
	Victims.Sort([](const int32 A, const int32 B) { return A > B; });
	for (const int32 Index : Victims)
	{
		Grants.RemoveAt(Index);
	}

	FGSAttackGrant Added;
	Added.Holder = Requester;
	Added.Cost = Cost;
	Added.GrantedTime = NowSeconds();
	Grants.Add(Added);
	return true;
}

void UGSEngagementComponent::ReleaseToken(AActor* Requester)
{
	Grants.RemoveAll([Requester](const FGSAttackGrant& Grant)
		{
			return !Grant.Holder.IsValid() || Grant.Holder.Get() == Requester;
		});
}

void UGSEngagementComponent::SetTokenLocked(AActor* Requester, bool bLocked)
{
	for (FGSAttackGrant& Grant : Grants)
	{
		if (Grant.Holder.Get() == Requester)
		{
			Grant.bLocked = bLocked;
			// Locking re-stamps the clock so the watchdog measures the committed window rather than
			// the whole approach-and-wait before it.
			if (bLocked)
			{
				Grant.GrantedTime = NowSeconds();
			}
			return;
		}
	}
}

// ---------------------------------------------------------------------------- capacity

bool UGSEngagementComponent::HasEngagementRoom(const AActor* Requester) const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& Actor : Engaged)
	{
		// GSIsDeadOrGone, not IsValid: CorpseLifespan is 0 ("never destroy"), so a dead attacker stays
		// a valid UObject forever and used to keep counting against this victim's capacity. That was
		// self-sustaining starvation, not a slow leak - this function is const and cannot prune, and
		// the only thing that DOES prune (RegisterEngaged) is reached solely by an attacker that this
		// function first said yes to. Three corpses registered on a live victim made it permanently
		// "full", so every live attacker skipped it and piled onto whoever was left. Measured
		// 2026-08-09: four dead guards each still holding engaged=3.
		if (GSIsDeadOrGone(Actor))
		{
			continue;
		}
		if (Actor.Get() == Requester)
		{
			// Already assigned - keeping your existing target must never be refused, or an agent at
			// capacity would drop and re-acquire in a loop.
			return true;
		}
		++Count;
	}
	return Count < MaxEngagedAttackers;
}

void UGSEngagementComponent::RegisterEngaged(AActor* Attacker)
{
	if (!IsValid(Attacker))
	{
		return;
	}
	PruneStale();
	for (const TWeakObjectPtr<AActor>& Actor : Engaged)
	{
		if (Actor.Get() == Attacker)
		{
			return;
		}
	}
	Engaged.Add(Attacker);
}

void UGSEngagementComponent::UnregisterEngaged(AActor* Attacker)
{
	Engaged.RemoveAll([Attacker](const TWeakObjectPtr<AActor>& Actor)
		{
			return !Actor.IsValid() || Actor.Get() == Attacker;
		});
}

int32 UGSEngagementComponent::GetEngagedCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& Actor : Engaged)
	{
		// Corpses excluded here too, so GS.Combat.CrowdStats stops reporting a pile of bodies as a
		// live gang. The readout said "engaged 3" on four dead guards and that is what sent an
		// investigation after a freeze that was really just everyone being dead.
		if (!GSIsDeadOrGone(Actor))
		{
			++Count;
		}
	}
	return Count;
}

int32 UGSEngagementComponent::GetEngagedCountExcluding(const AActor* Ignore) const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& Actor : Engaged)
	{
		// Same corpse filter as GetEngagedCount - a spread-out rule that counted bodies would send
		// agents away from a victim nobody live is actually on, which is #106 pointed the other way.
		if (!GSIsDeadOrGone(Actor) && Actor.Get() != Ignore)
		{
			++Count;
		}
	}
	return Count;
}

void UGSEngagementComponent::GetEngagedActors(TArray<AActor*>& Out) const
{
	Out.Reset();
	for (const TWeakObjectPtr<AActor>& Actor : Engaged)
	{
		// Same corpse filter as GetEngagedCount, deliberately - a diagnostic that disagrees with the
		// count it sits next to is worse than no diagnostic. #106 published a crowding baseline
		// measured partly on bodies and it took a while to notice.
		if (!GSIsDeadOrGone(Actor))
		{
			Out.Add(Actor.Get());
		}
	}
}

AActor* UGSEngagementComponent::GetSlotClaimant(int32 SlotIndex) const
{
	if (!RingSlots.IsValidIndex(SlotIndex))
	{
		return nullptr;
	}
	const TWeakObjectPtr<AActor>& Claimant = RingSlots[SlotIndex].Claimant;
	return GSIsDeadOrGone(Claimant) ? nullptr : Claimant.Get();
}

// ---------------------------------------------------------------------------- body size

float UGSEngagementComponent::GetBodyRadius(const AActor* A)
{
	// Scaled, not the raw property: a character scaled up in its Blueprint has a collision radius
	// larger than CapsuleRadius reports, and spacing derived from the unscaled number would be
	// exactly as wrong as the hardcoded constants this exists to replace.
	if (const ACharacter* Char = Cast<ACharacter>(A))
	{
		if (const UCapsuleComponent* Capsule = Char->GetCapsuleComponent())
		{
			return Capsule->GetScaledCapsuleRadius();
		}
	}
	// Not a character, or a character with no capsule. Zero rather than a guessed default: a caller
	// adding two radii gets the other body's real size and its own margin, which degrades to
	// "keep Margin apart" instead of silently spacing on a number nobody chose.
	return 0.f;
}

float UGSEngagementComponent::GetMinSeparation(const AActor* A, const AActor* B, float Margin)
{
	return GetBodyRadius(A) + GetBodyRadius(B) + FMath::Max(0.f, Margin);
}

// ---------------------------------------------------------------------------- ring slots

int32 UGSEngagementComponent::ClaimRingSlot(AActor* Claimant)
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Claimant) || !IsValid(Owner))
	{
		return INDEX_NONE;
	}

	PruneStale();

	if (RingSlots.Num() == 0)
	{
		RingSlots.SetNum(FMath::Max(1, RingSlotCount));
	}

	// Keeping an existing claim is what stops an agent re-picking a different slot every scan and
	// sliding sideways around the target forever.
	for (int32 i = 0; i < RingSlots.Num(); ++i)
	{
		if (RingSlots[i].Claimant.Get() == Claimant)
		{
			// Re-stamp the clock while the claimant is ARRIVED OR STILL CLOSING. Previously this
			// returned without touching ClaimedTime, so PruneStale's SlotClaimTimeoutSeconds expired
			// the claim of an agent standing perfectly on its slot every 4 seconds and made it
			// re-race for a place - a visible reset of the formation, and the exclusivity the ring
			// exists to provide evaporated on a timer.
			//
			// NOT an unconditional re-stamp: that would make the timeout unreachable and let an agent
			// stuck on geometry hold a place for the rest of the fight, which is the very thing the
			// timeout is for. Measuring "arrived, or closing" keeps the watchdog pointed at the only
			// case it should ever fire on - a claimant making no progress at all.
			const float Dist = FVector::Dist2D(Claimant->GetActorLocation(), GetRingSlotLocation(i));
			if (Dist <= SlotArrivedRadius || Dist < RingSlots[i].ClosestApproach - SlotProgressEpsilon)
			{
				RingSlots[i].ClosestApproach = FMath::Min(RingSlots[i].ClosestApproach, Dist);
				RingSlots[i].ClaimedTime = NowSeconds();
			}
			return i;
		}
	}

	// Start from the slot nearest the claimant's CURRENT bearing and walk outwards, so an agent
	// takes the nearest free place on its own side rather than crossing the pack.
	FVector Bearing = Claimant->GetActorLocation() - Owner->GetActorLocation();
	Bearing.Z = 0.f;
	if (Bearing.IsNearlyZero())
	{
		Bearing = FVector::ForwardVector;
	}

	const int32 Count = RingSlots.Num();
	const float StepRadians = 2.f * PI / static_cast<float>(Count);
	const int32 Preferred = FMath::RoundToInt(FMath::Atan2(Bearing.Y, Bearing.X) / StepRadians);

	for (int32 Offset = 0; Offset < Count; ++Offset)
	{
		// 0, +1, -1, +2, -2 ... outwards from the preferred bearing in both directions.
		const int32 Signed = (Offset % 2 == 0) ? (Offset / 2) : -((Offset + 1) / 2);
		const int32 Index = ((Preferred + Signed) % Count + Count) % Count;

		// GSIsDeadOrGone, not IsValid: a corpse stays a valid UObject (CorpseLifespan 0) and would
		// hold a place in the ring for the rest of the raid.
		if (GSIsDeadOrGone(RingSlots[Index].Claimant))
		{
			RingSlots[Index].Claimant = Claimant;
			RingSlots[Index].ClaimedTime = NowSeconds();
			// Seed the progress baseline with the real starting distance. Leaving it at FLT_MAX would
			// make the very first progress test pass trivially and re-stamp the clock for an agent
			// that had not moved.
			RingSlots[Index].ClosestApproach =
				FVector::Dist2D(Claimant->GetActorLocation(), GetRingSlotLocation(Index));
			return Index;
		}
	}

	// Ring full. The caller holds at the outer radius and menaces - see UBTTask_MenaceOrbit.
	return INDEX_NONE;
}

void UGSEngagementComponent::ReleaseRingSlot(AActor* Claimant)
{
	for (FGSRingSlot& Slot : RingSlots)
	{
		if (!Slot.Claimant.IsValid() || Slot.Claimant.Get() == Claimant)
		{
			Slot.Claimant = nullptr;
		}
	}
}

FVector UGSEngagementComponent::GetRingSlotLocation(int32 SlotIndex) const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return FVector::ZeroVector;
	}

	const FVector Centre = Owner->GetActorLocation();
	if (!RingSlots.IsValidIndex(SlotIndex))
	{
		return Centre;
	}

	// WORLD-space angles, not relative to the owner's facing. A ring that rotates with the defender
	// would drag every attacker around with him every time he turned to swing, which reads as the
	// crowd sliding rather than standing.
	const float StepRadians = 2.f * PI / static_cast<float>(RingSlots.Num());
	const float Angle = StepRadians * static_cast<float>(SlotIndex);
	return Centre + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * RingRadius;
}

int32 UGSEngagementComponent::GetClaimedSlotCount() const
{
	int32 Count = 0;
	for (const FGSRingSlot& Slot : RingSlots)
	{
		if (!GSIsDeadOrGone(Slot.Claimant))
		{
			++Count;
		}
	}
	return Count;
}

void UGSEngagementComponent::ReleaseAll(AActor* Requester)
{
	ReleaseToken(Requester);
	ReleaseRingSlot(Requester);
	UnregisterEngaged(Requester);
}
