#include "AI/Tasks/BTService_AcquireTarget.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Combat/GSGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

UBTService_AcquireTarget::UBTService_AcquireTarget()
{
	NodeName = TEXT("Acquire Target (player)");

	// 0.5s is plenty for "is the player still nearby" and keeps six defenders off the tick budget.
	Interval = 0.5f;
	RandomDeviation = 0.1f;

	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_AcquireTarget, TargetKey),
		AActor::StaticClass());
	TargetKey.SelectedKeyName = TEXT("TargetActor");

	TargetLocationKey.AddVectorFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTService_AcquireTarget, TargetLocationKey));
	TargetLocationKey.SelectedKeyName = TEXT("TargetLocation");
}

void UBTService_AcquireTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	const AAIController* Controller = OwnerComp.GetAIOwner();
	const APawn* Self = Controller ? Controller->GetPawn() : nullptr;
	if (!BB || !Self)
	{
		return;
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);

	// A corpse is not a target. Without this the whole patrol stands over the body swinging, which
	// also parks them on the PlayerStart and blocks the respawn that would end the situation.
	bool bAlive = Player != nullptr;
	if (Player)
	{
		if (const UAbilitySystemComponent* TargetASC =
				UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player))
		{
			bAlive = !TargetASC->HasMatchingGameplayTag(GSTags::State_Dead);
		}
	}

	const bool bInRange = bAlive
		&& FVector::Dist(Player->GetActorLocation(), Self->GetActorLocation()) <= AcquireRadius;

	// Clearing rather than leaving a stale target is what lets the tree's Selector fall through to
	// its idle branch - a decorator testing "is set" cannot tell a stale value from a live one.
	BB->SetValueAsObject(TargetKey.SelectedKeyName, bInRange ? Player : nullptr);

	if (!bInRange)
	{
		BB->ClearValue(TargetLocationKey.SelectedKeyName);
		return;
	}

	// Walk to a slot on the ring around the target, not to the target itself. Approaching from the
	// bearing this defender is ALREADY on means nobody crosses the pack to reach their slot, and
	// three defenders coming from three sides naturally end up spread around it.
	const FVector TargetLoc = Player->GetActorLocation();
	FVector Bearing = Self->GetActorLocation() - TargetLoc;
	Bearing.Z = 0.f;

	if (Bearing.IsNearlyZero())
	{
		// Standing exactly on the target - any direction beats a zero vector.
		Bearing = -Self->GetActorForwardVector();
		Bearing.Z = 0.f;
	}
	Bearing = Bearing.GetSafeNormal();

	// Same bearing, same slot, same pile. A stable per-pawn offset separates them; FName hashing
	// keeps it deterministic, so a defender does not wander around the ring every tick.
	if (SlotAngleJitterDegrees > 0.f)
	{
		const uint32 Hash = GetTypeHash(Self->GetFName());
		const float Frac = static_cast<float>(Hash % 1024) / 1024.f;   // 0..1, stable per pawn
		Bearing = Bearing.RotateAngleAxis((Frac * 2.f - 1.f) * SlotAngleJitterDegrees,
			FVector::UpVector);
	}

	BB->SetValueAsVector(TargetLocationKey.SelectedKeyName,
		TargetLoc + Bearing * StandoffRadius);
}
