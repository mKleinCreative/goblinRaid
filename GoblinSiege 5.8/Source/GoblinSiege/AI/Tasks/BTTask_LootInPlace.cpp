#include "AI/Tasks/BTTask_LootInPlace.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "GameFramework/Pawn.h"
#include "Horde/GSHordeGoblin.h"
#include "Interaction/GSInteractableComponent.h"
#include "Raid/GSScoreSubsystem.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSLootInPlace, Log, All);

UBTTask_LootInPlace::UBTTask_LootInPlace()
{
	NodeName = TEXT("Loot In Place (CompleteInteraction)");

	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_LootInPlace, TargetKey),
		AActor::StaticClass());
	TargetKey.SelectedKeyName = TEXT("OrderSubject");
}

void UBTTask_LootInPlace::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		TargetKey.ResolveSelectedKey(*BBAsset);
	}
}

EBTNodeResult::Type UBTTask_LootInPlace::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!BB || !Controller)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Self = Controller->GetPawn();
	AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!Self || !IsValid(Target))
	{
		return EBTNodeResult::Failed;
	}

	const float Dist = FVector::Dist2D(Target->GetActorLocation(), Self->GetActorLocation());
	if (Dist > InteractRange)
	{
		UE_LOG(LogGSLootInPlace, Log, TEXT("[GS.Loot] %s: '%s' is %.0fuu away, need <= %.0fuu - not in range yet."),
			*Self->GetName(), *Target->GetName(), Dist, InteractRange);
		return EBTNodeResult::Failed;
	}

	UGSInteractableComponent* Interactable = Target->FindComponentByClass<UGSInteractableComponent>();
	if (!Interactable)
	{
		UE_LOG(LogGSLootInPlace, Verbose, TEXT("[GS.Loot] %s: '%s' has no UGSInteractableComponent."),
			*Self->GetName(), *Target->GetName());
		return EBTNodeResult::Failed;
	}

	// A carryable subject (a pig, a sack) is UBTTask_PickUpCargo's job, not this node's - failing here
	// lets the tree's other Loot branch have it. A still-locked container (unbroken crate) also fails
	// here rather than looting nothing: UBTTask_SmashOrderTarget is meant to run first in the sequence
	// and CanInteract only opens once bIsAvailable flips, which is what breaking it does.
	if (Interactable->IsCarryable() || !Interactable->CanInteract(Self))
	{
		UE_LOG(LogGSLootInPlace, Log, TEXT("[GS.Loot] %s: '%s' not ready to loot (carryable=%d, CanInteract=%d) - still locked, or somebody else is on it."),
			*Self->GetName(), *Target->GetName(), Interactable->IsCarryable(), Interactable->CanInteract(Self));
		return EBTNodeResult::Failed;
	}

	const int32 Value = Interactable->GetLootValue();

	Interactable->NotifyChannelStarted(Self);
	Interactable->CompleteInteraction(Self);

	if (Value > 0)
	{
		// Gold looted in place banks onto the LOOTING GOBLIN, not the shared score subsystem
		// (2026-08-30, Michael: "they only return with loot if it's livestock, otherwise accumulate
		// the gold on themselves as their personal purse, and when they die, they drop that personal
		// purse"). Livestock never reaches this task - it is carryable, so it goes through
		// PickUpCargo/DeliverCargo and GSLootBankComponent instead - so every successful loot here is
		// gold by construction. The AGSHordeGoblin cast is defensive, not load-bearing: this task only
		// ever runs from BT_HordeGoblin, but a stray future caller falling back to the old direct
		// score-subsystem behaviour is better than silently dropping the points.
		if (AGSHordeGoblin* Looter = Cast<AGSHordeGoblin>(Self))
		{
			Looter->AddToPersonalPurse(Value);
		}
		else if (UWorld* World = Self->GetWorld())
		{
			if (UGSScoreSubsystem* Score = World->GetSubsystem<UGSScoreSubsystem>())
			{
				Score->AddLoot(Value);
			}
			else
			{
				UE_LOG(LogGSLootInPlace, Error,
					TEXT("[GS.Loot] %s looted '%s' worth %d but found no UGSScoreSubsystem - the points are gone."),
					*Self->GetName(), *Target->GetName(), Value);
			}
		}
	}

	UE_LOG(LogGSLootInPlace, Log, TEXT("[GS.Loot] %s looted '%s' in place for %d."),
		*Self->GetName(), *Target->GetName(), Value);

	return EBTNodeResult::Succeeded;
}
