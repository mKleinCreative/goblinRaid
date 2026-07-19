#include "AI/GSAIControllerBase.h"
#include "Characters/GSEnemyCharacter.h"
#include "Combat/GSRaceDataAsset.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"

AGSAIControllerBase::AGSAIControllerBase()
{
	AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComponent"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngleDegrees;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

	AIPerceptionComponent->ConfigureSense(*SightConfig);
	AIPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
	SetPerceptionComponent(*AIPerceptionComponent);
}

void AGSAIControllerBase::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	AIPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AGSAIControllerBase::HandlePerceptionUpdated);

	if (const AGSEnemyCharacter* Enemy = Cast<AGSEnemyCharacter>(InPawn))
	{
		if (const UGSRaceDataAsset* RaceData = Enemy->GetRaceData())
		{
			if (const FGSArchetypeDefinition* Archetype = RaceData->FindArchetype(Enemy->GetArchetypeRowName()))
			{
				if (UBehaviorTree* BT = Archetype->BehaviorTree.LoadSynchronous())
				{
					RunBehaviorTree(BT);
					if (UBlackboardComponent* BB = GetBlackboardComponent())
					{
						BB->SetValueAsObject(TEXT("SelfArchetypeOwner"), InPawn);
					}
				}
			}
		}
	}

	// Goblin AI companions (design doc §8: "deliberately simple - follow, engage nearby enemies,
	// respawn on their own lives") use a single fixed companion BT assigned in a
	// AGSAIControllerBase Blueprint subclass rather than the archetype-lookup path above, since
	// companions aren't AGSEnemyCharacter and don't have race data.
}

void AGSAIControllerBase::OnUnPossess()
{
	if (AIPerceptionComponent)
	{
		AIPerceptionComponent->OnTargetPerceptionUpdated.RemoveDynamic(this, &AGSAIControllerBase::HandlePerceptionUpdated);
	}
	Super::OnUnPossess();
}

void AGSAIControllerBase::HandlePerceptionUpdated(AActor* UpdatedActor, FAIStimulus Stimulus)
{
	// Target selection (nearest, lowest-HP, highest-threat) is intentionally left to an EQS query
	// run from the Behavior Tree rather than decided here, so different archetypes (Archer:
	// prioritize-range vs Knight: prioritize-nearest) can use different EQS generators/tests
	// against the same perceived-actor list without controller code branching per archetype.
}
