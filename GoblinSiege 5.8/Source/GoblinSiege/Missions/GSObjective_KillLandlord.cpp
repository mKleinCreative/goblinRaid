#include "Missions/GSObjective_KillLandlord.h"
#include "Characters/GSCharacterBase.h"
#include "Attributes/GSAttributeSetBase.h"
#include "ACFStatisticsSet.h"
#include "Core/GSGameState.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"

void AGSObjective_KillLandlord::BeginObjective()
{
	Super::BeginObjective();

	// Tagged-actor lookup, matching AGSObjective_ToppleStatue's pattern (see header comment).
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(this, LandlordActorTag, FoundActors);
	if (FoundActors.Num() > 0)
	{
		LandlordPawn = Cast<AGSCharacterBase>(FoundActors[0]);
	}

	if (LandlordPawn && LandlordPawn->GetAbilitySystemComponent())
	{
		// AddUObject, not AddDynamic - see header note on the non-dynamic multicast.
		LandlordPawn->GetAbilitySystemComponent()
			->GetGameplayAttributeValueChangeDelegate(GetDefault<UACFStatisticsSet>()->HealthAttribute())   // ARS owns health (#228)
			.AddUObject(this, &AGSObjective_KillLandlord::HandleLandlordHealthChanged);
	}

	if (AGSGameState* GS = Cast<AGSGameState>(UGameplayStatics::GetGameState(this)))
	{
		GS->OnAlarmChanged.AddDynamic(this, &AGSObjective_KillLandlord::HandleAlarmChanged);
	}
}

void AGSObjective_KillLandlord::HandleAlarmChanged(float NewAlarm01)
{
	if (bHasTriggeredFlee || NewAlarm01 < FleeOnAlarmThreshold01 || !LandlordPawn)
	{
		return;
	}
	bHasTriggeredFlee = true;

	// "He should react to the raid: flee toward a safehouse... under escort once alarmed"
	// (design doc §6). The BT handles the how; this objective only flips the switch.
	if (AAIController* AIController = Cast<AAIController>(LandlordPawn->GetController()))
	{
		if (UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent())
		{
			Blackboard->SetValueAsBool(TEXT("IsFleeing"), true);
		}
	}
}

void AGSObjective_KillLandlord::HandleLandlordHealthChanged(const FOnAttributeChangeData& Data)
{
	if (!bCompleted && Data.NewValue <= 0.f)
	{
		CompleteObjective();
	}
}
