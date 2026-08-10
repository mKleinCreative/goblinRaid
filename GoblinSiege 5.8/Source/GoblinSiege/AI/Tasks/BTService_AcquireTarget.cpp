#include "AI/Tasks/BTService_AcquireTarget.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AI/GSAIDebug.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Characters/GSCharacterBase.h"
#include "Combat/GSEngagementComponent.h"
#include "Combat/GSGameplayTags.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

UBTService_AcquireTarget::UBTService_AcquireTarget()
{
	NodeName = TEXT("Acquire Target (nearest hostile)");

	// 0.15s, down from 0.5s. The node now has two jobs on two clocks: catching a windup, which is
	// 0.22s at its shortest and would be missed outright by a 0.5s sample, and scanning for
	// candidates, which does not need to run anywhere near that often and is throttled separately by
	// ReacquireIntervalSeconds. A tick that does neither costs one distance check and one tag lookup.
	Interval = 0.15f;
	RandomDeviation = 0.03f;

	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_AcquireTarget, TargetKey),
		AActor::StaticClass());
	TargetKey.SelectedKeyName = TEXT("TargetActor");

	TargetLocationKey.AddVectorFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTService_AcquireTarget, TargetLocationKey));
	TargetLocationKey.SelectedKeyName = TEXT("TargetLocation");

	TargetIsAttackingKey.AddBoolFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTService_AcquireTarget, TargetIsAttackingKey));
	TargetIsAttackingKey.SelectedKeyName = TEXT("TargetIsAttacking");
}

void UBTService_AcquireTarget::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		TargetKey.ResolveSelectedKey(*BBAsset);
		TargetLocationKey.ResolveSelectedKey(*BBAsset);
		TargetIsAttackingKey.ResolveSelectedKey(*BBAsset);
	}
}

bool UBTService_AcquireTarget::IsEngageable(const AGSCharacterBase& Self, const AActor* Candidate) const
{
	const AGSCharacterBase* Char = Cast<AGSCharacterBase>(Candidate);
	if (!IsValid(Char) || Char == &Self)
	{
		return false;
	}

	// A corpse is not a target. Without this the whole patrol stands over the body swinging, which
	// also parks them on the PlayerStart and blocks the respawn that would end the situation.
	// bIsDead flips before the ragdoll, so this is the earliest honest answer.
	if (!Char->IsAlive())
	{
		return false;
	}

	// See FindNearestHostile's comment: "no opinion" is hostile for damage, ignorable for chasing.
	if (!Char->GetRaceTag().IsValid())
	{
		return false;
	}

	if (!Self.IsHostileTo(Char))
	{
		return false;
	}

	// Belt and braces against the window between HandleDeath's tag and bIsDead. Cheap, and the
	// alternative is a defender committing a 0.85s heavy into a body that is already falling.
	if (const UAbilitySystemComponent* ASC =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Char))
	{
		if (ASC->HasMatchingGameplayTag(GSTags::State_Dead))
		{
			return false;
		}
	}

	return true;
}

AGSCharacterBase* UBTService_AcquireTarget::FindNearestHostile(const AGSCharacterBase& Self,
	AActor* CurrentTarget) const
{
	UWorld* World = Self.GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FVector SelfLoc = Self.GetActorLocation();
	const float AcquireRadiusSq = AcquireRadius * AcquireRadius;

	AGSCharacterBase* Best = nullptr;
	float BestScoreSq = TNumericLimits<float>::Max();

	// TActorRange over AGSCharacterBase rather than a sphere overlap: every combatant in this game
	// is one, the counts are tens rather than thousands, and this runs at ReacquireIntervalSeconds
	// (0.5s) per agent rather than per frame. An overlap would also need a channel that reliably
	// catches pawns of both factions, which is a second thing to keep in sync with no benefit here.
	for (TActorIterator<AGSCharacterBase> It(World); It; ++It)
	{
		AGSCharacterBase* Candidate = *It;
		if (!IsEngageable(Self, Candidate))
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(SelfLoc, Candidate->GetActorLocation());
		if (DistSq > AcquireRadiusSq)
		{
			continue;
		}

		// CAPACITY, same rule the horde uses. Six knights all picking the nearest goblin is the
		// conga line pointed the other way, and it is worse for defenders because they are the side
		// the player watches. An agent already assigned always passes, so nobody drops and
		// re-acquires in a loop at the cap.
		if (const UGSEngagementComponent* Engagement =
				Candidate->FindComponentByClass<UGSEngagementComponent>())
		{
			if (!Engagement->HasEngagementRoom(&Self))
			{
				continue;
			}
		}

		// Hysteresis, applied as a discount on the INCUMBENT rather than a penalty on challengers,
		// so the comparison stays a single sorted scan. Squared because the whole scan is.
		float ScoreSq = DistSq;
		if (Candidate == CurrentTarget)
		{
			ScoreSq *= (TargetSwitchHysteresis * TargetSwitchHysteresis);
		}

		if (ScoreSq < BestScoreSq)
		{
			BestScoreSq = ScoreSq;
			Best = Candidate;
		}
	}

	return Best;
}

void UBTService_AcquireTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	const AAIController* Controller = OwnerComp.GetAIOwner();
	AGSCharacterBase* Self = Controller ? Cast<AGSCharacterBase>(Controller->GetPawn()) : nullptr;
	if (!BB || !Self)
	{
		return;
	}

	UWorld* World = OwnerComp.GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	FGSAcquireTargetMemory* Memory = CastInstanceNodeMemory<FGSAcquireTargetMemory>(NodeMemory);

	AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));

	// ---- selection -------------------------------------------------------------------------
	if (bSelectTarget)
	{
		// Drop a target that has died, been destroyed, or walked out of LoseRadius, BEFORE scanning -
		// otherwise the hysteresis discount above would keep defending an unreachable incumbent.
		if (Target)
		{
			const bool bStillLegal = IsEngageable(*Self, Target)
				&& FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation()) <= LoseRadius;

			if (!bStillLegal)
			{
				if (GSAIDebug::IsLogging())
				{
					GSAIDebug::Log(Self, FString::Printf(TEXT("dropped %s"), *GetNameSafe(Target)));
				}
				// Let go of the ledger before forgetting who it was, or the assignment and the ring
				// slot leak and the caps drift upward over a long fight.
				if (UGSEngagementComponent* Engagement =
						Target->FindComponentByClass<UGSEngagementComponent>())
				{
					Engagement->ReleaseAll(Self);
				}
				Target = nullptr;

				// Rescan NOW rather than waiting out the interval. A defender that has just killed
				// someone would otherwise stand over the body for up to half a second before noticing
				// the next goblin, which reads as losing interest at the exact moment it should not.
				if (Memory)
				{
					Memory->NextScanTime = 0.f;
				}
			}
		}

		if (Memory && Now >= Memory->NextScanTime)
		{
			// Jittered so a patrol that spawned on one frame does not scan on one frame forever.
			Memory->NextScanTime = Now + ReacquireIntervalSeconds
				+ FMath::FRandRange(0.f, ReacquireIntervalSeconds * 0.2f);

			AGSCharacterBase* Found = FindNearestHostile(*Self, Target);
			if (Found != Target)
			{
				// Switching: release the old victim's ledger first, same reason as the drop path.
				if (Target)
				{
					if (UGSEngagementComponent* Engagement =
							Target->FindComponentByClass<UGSEngagementComponent>())
					{
						Engagement->ReleaseAll(Self);
					}
				}
				if (GSAIDebug::IsLogging() && Found)
				{
					GSAIDebug::Log(Self, FString::Printf(TEXT("acquired %s (%s, %.0fuu)"),
						*GetNameSafe(Found),
						*Found->GetRaceTag().ToString(),
						FVector::Dist(Found->GetActorLocation(), Self->GetActorLocation())));
				}
				Target = Found;
			}
		}

		// Clearing rather than leaving a stale target is what lets the tree's Selector fall through
		// to its idle branch - a decorator testing "is set" cannot tell a stale value from a live one.
		BB->SetValueAsObject(TargetKey.SelectedKeyName, Target);
	}
	// With bSelectTarget off, whatever wrote TargetActor owns it (AGSHordeAIController writes it from
	// the horde's threat registry). Read it, never clear it - clearing here would fight that writer
	// every tick and the goblin would flicker between Frenzy and Follow.

	if (!Target)
	{
		BB->ClearValue(TargetLocationKey.SelectedKeyName);
		if (TargetIsAttackingKey.IsSet())
		{
			BB->SetValueAsBool(TargetIsAttackingKey.SelectedKeyName, false);
		}
		if (Memory)
		{
			Memory->TelegraphUntil = 0.f;
			Memory->LatchedTargetId = 0;
		}
		return;
	}

	// ---- the telegraph ---------------------------------------------------------------------
	// Latched, not sampled. A 0.22s windup against a 0.15s tick would otherwise be missed roughly a
	// third of the time, and "the AI blocks sometimes" is indistinguishable from a bad probability
	// roll - which is exactly the bug this whole node is meant to make diagnosable.
	if (Memory && TargetIsAttackingKey.IsSet())
	{
		const uint32 TargetId = Target->GetUniqueID();
		if (Memory->LatchedTargetId != TargetId)
		{
			// A latch belongs to the target that earned it. Carrying it across a target switch would
			// have a defender guarding against a swing the new target never made.
			Memory->LatchedTargetId = TargetId;
			Memory->TelegraphUntil = 0.f;
		}

		if (const UAbilitySystemComponent* TargetASC =
				UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target))
		{
			if (TargetASC->HasMatchingGameplayTag(GSTags::State_Attacking_Windup))
			{
				// Log the EDGE, not the state - one line per windup seen rather than one per tick.
				// Without this, "the AI never blocks" and "the AI never sees the windup" are the same
				// observation, and they have completely different causes.
				if (GSAIDebug::IsLogging() && Now >= Memory->TelegraphUntil)
				{
					GSAIDebug::Log(Self, FString::Printf(TEXT("telegraph SEEN from %s"),
						*GetNameSafe(Target)));
				}
				Memory->TelegraphUntil = Now + TelegraphLatchSeconds;
			}
		}

		BB->SetValueAsBool(TargetIsAttackingKey.SelectedKeyName, Now < Memory->TelegraphUntil);
	}

	// ---- the stand-off slot ----------------------------------------------------------------
	// CLAIMED, not computed. The previous version snapped this agent's own bearing to one of eight
	// world angles, which spread a pack out but could not stop two agents on a similar bearing
	// choosing the SAME angle and shoving. The ring now lives on the target and hands out exclusive
	// claims; it still prefers the slot nearest the claimant's current bearing, so the property that
	// made the old version work - approach from the side you are already on, never cross the pack -
	// is preserved.
	const FVector TargetLoc = Target->GetActorLocation();

	if (UGSEngagementComponent* Engagement = Target->FindComponentByClass<UGSEngagementComponent>())
	{
		Engagement->RegisterEngaged(Self);

		const int32 SlotIndex = Engagement->ClaimRingSlot(Self);
		if (SlotIndex != INDEX_NONE)
		{
			FVector SlotLoc = Engagement->GetRingSlotLocation(SlotIndex);

			// The slot chose the DIRECTION; this agent's own tree chooses the DISTANCE along it.
			// An archer keeps its exclusive place in the ring - and its seat against capacity - but
			// stands at bow range rather than sword range. See StandoffRadiusOverride.
			if (StandoffRadiusOverride > 0.f)
			{
				const FVector SlotDir = (SlotLoc - TargetLoc).GetSafeNormal2D();
				if (!SlotDir.IsNearlyZero())
				{
					SlotLoc = TargetLoc + SlotDir * StandoffRadiusOverride;
				}
			}

			BB->SetValueAsVector(TargetLocationKey.SelectedKeyName, SlotLoc);
			return;
		}

		// Ring full: hold outside it rather than pushing in. The melee branch will fail on range and
		// the tree falls through to the menace orbit, which is where an agent without a place
		// belongs - circling, not queuing motionless against someone else's back.
		FVector OutwardBearing = Self->GetActorLocation() - TargetLoc;
		OutwardBearing.Z = 0.f;
		if (OutwardBearing.IsNearlyZero())
		{
			OutwardBearing = FVector::ForwardVector;
		}
		BB->SetValueAsVector(TargetLocationKey.SelectedKeyName,
			TargetLoc + OutwardBearing.GetSafeNormal() * (StandoffRadius * 1.8f));
		return;
	}

	// No engagement component - a target dummy, a breakable. Fall back to the old computed lattice
	// so these stay attackable rather than becoming unreachable for want of a ledger.
	FVector Bearing = Self->GetActorLocation() - TargetLoc;
	Bearing.Z = 0.f;
	if (Bearing.IsNearlyZero())
	{
		Bearing = -Self->GetActorForwardVector();
		Bearing.Z = 0.f;
		if (Bearing.IsNearlyZero())
		{
			Bearing = FVector::ForwardVector;
		}
	}

	const int32 SlotCount = FMath::Max(1, ApproachSlotCount);
	const float StepRadians = 2.f * PI / static_cast<float>(SlotCount);
	const float SnappedAngle = FMath::RoundToFloat(FMath::Atan2(Bearing.Y, Bearing.X) / StepRadians) * StepRadians;
	const FVector SlotDir(FMath::Cos(SnappedAngle), FMath::Sin(SnappedAngle), 0.f);

	BB->SetValueAsVector(TargetLocationKey.SelectedKeyName, TargetLoc + SlotDir * StandoffRadius);
}
