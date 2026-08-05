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

	if (bInRange)
	{
		BB->SetValueAsVector(TargetLocationKey.SelectedKeyName, Player->GetActorLocation());
	}
	else
	{
		BB->ClearValue(TargetLocationKey.SelectedKeyName);
	}
}
