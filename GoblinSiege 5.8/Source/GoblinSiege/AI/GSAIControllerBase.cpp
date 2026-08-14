#include "AI/GSAIControllerBase.h"
#include "AI/GSAISteeringComponent.h"
#include "Characters/GSCharacterBase.h"
#include "Characters/GSEnemyCharacter.h"
#include "Combat/GSRaceDataAsset.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"

// GS.Combat.Separation and GS.Combat.FaceTarget moved to GSAISteeringComponent.cpp with the
// behaviours they switch (#143). They are still registered, still named the same, and still mean the
// same thing - only the translation unit changed.

bool AGSAIControllerBase::IsFacingAuthorityEnabled()
{
	// Forwarder. UBTTask_MeleeAttack, UBTTask_MenaceOrbit and UBTTask_Block all call this symbol; the
	// answer lives with the behaviour it governs.
	return UGSAISteeringComponent::IsFacingAuthorityEnabled();
}

AGSAIControllerBase::AGSAIControllerBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The actor tick is no longer load-bearing: the steering component registers its own tick
	// function and does not consult this flag. Left TRUE rather than flipped off, because turning it
	// off is precisely the edit that silently disabled #132 and #133 on the whole horde (#135), and a
	// future base class may want it. Nothing of ours reads it now.
	PrimaryActorTick.bCanEverTick = true;

	// Every AI controller gets one, defender and horde alike. Created unconditionally and NOT through
	// ObjectInitializer.CreateDefaultSubobject: unlike the perception component below, there is no
	// subclass that should ever be allowed to decline it - a goblin that does not separate or face its
	// target is the bug, not an optimisation.
	SteeringComponent = CreateDefaultSubobject<UGSAISteeringComponent>(TEXT("SteeringComponent"));

	// Null whenever a subclass passed DoNotCreateDefaultSubobject for this name - see the header,
	// including the note that 5.8 ignores that opt-out and creates one anyway. Everything below is
	// guarded on it regardless, since the guard is correct if the engine ever honours it again.
	AIPerceptionComponent = ObjectInitializer.CreateDefaultSubobject<UAIPerceptionComponent>(
		this, TEXT("AIPerceptionComponent"));

	if (AIPerceptionComponent)
	{
		SightConfig = ObjectInitializer.CreateDefaultSubobject<UAISenseConfig_Sight>(this, TEXT("SightConfig"));
		SightConfig->SightRadius = SightRadius;
		SightConfig->LoseSightRadius = LoseSightRadius;
		SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngleDegrees;
		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

		// Note this has never actually worked as written: affiliation is resolved through
		// IGenericTeamAgentInterface, which nothing in this project implements, so every actor reads
		// as neutral and bDetectFriendlies=false gates nothing. Left as-is deliberately - the
		// friend/foe rule this game actually enforces is AGSCharacterBase::IsHostileTo on RaceTag,
		// and adding a team interface now would give two competing sources of truth.
		SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

		AIPerceptionComponent->ConfigureSense(*SightConfig);
		AIPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
		SetPerceptionComponent(*AIPerceptionComponent);
	}
}

void AGSAIControllerBase::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

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

	// The archetype lookup above is a DEFENDER path - it needs AGSEnemyCharacter's race data, which
	// a horde goblin does not have. Allied goblins run a single fixed companion BT instead, and
	// since #069 that is owned by AGSHordeAIController::CompanionBehaviorTree rather than by a
	// Blueprint subclass of this class. Nothing here needs to know about them.
}

void AGSAIControllerBase::OnUnPossess()
{
	// BEFORE Super, while GetPawn() still answers. The component restores the pawn's rotation mode
	// and drops its cached neighbours - see UGSAISteeringComponent::HandleUnPossess for why both
	// matter to a pawn going back into the horde pool.
	if (SteeringComponent)
	{
		SteeringComponent->HandleUnPossess();
	}

	Super::OnUnPossess();
}
