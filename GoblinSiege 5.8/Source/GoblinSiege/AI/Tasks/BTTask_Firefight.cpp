#include "AI/Tasks/BTTask_Firefight.h"
#include "AIController.h"
#include "Destruction/GSFlammableComponent.h"
#include "Core/GSGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/OverlapResult.h"

UBTTask_Firefight::UBTTask_Firefight()
{
	NodeName = TEXT("Firefight (extinguish nearest fire)");
}

EBTNodeResult::Type UBTTask_Firefight::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	// NOTE (Part II §21): firefighting becomes well-sourced - defenders fetch water at AGSWell
	// before extinguishing. This immediate version is the Part I scaffold behavior.
	TArray<FOverlapResult> Overlaps;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(SearchRadius);
	Pawn->GetWorld()->OverlapMultiByObjectType(Overlaps, Pawn->GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams::AllObjects, Sphere);

	UGSFlammableComponent* NearestBurning = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		UGSFlammableComponent* Flammable = Actor ? Actor->FindComponentByClass<UGSFlammableComponent>() : nullptr;
		if (Flammable && Flammable->IsBurning())
		{
			const float DistSq = FVector::DistSquared(Actor->GetActorLocation(), Pawn->GetActorLocation());
			if (DistSq < NearestDistSq)
			{
				NearestDistSq = DistSq;
				NearestBurning = Flammable;
			}
		}
	}

	if (!NearestBurning)
	{
		return EBTNodeResult::Failed;
	}

	NearestBurning->Extinguish();

	if (AGSGameState* GS = Cast<AGSGameState>(UGameplayStatics::GetGameState(Pawn)))
	{
		GS->AddAlarm(-AlarmReductionOnExtinguish, EGSAlarmSource::FireExtinguishedByDefenders);
	}

	return EBTNodeResult::Succeeded;
}
