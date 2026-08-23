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
#include "HAL/IConsoleManager.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

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

bool AGSAIControllerBase::IsEntityAlive_Implementation() const
{
	// The CONTROLLER answers for its pawn - ACF asks the entity, and for an AI the entity that
	// matters is the body, not the brain. An unpossessed controller is not alive by any useful
	// reading, so a null pawn is false rather than true-by-default.
	const AGSCharacterBase* Body = Cast<AGSCharacterBase>(GetPawn());
	return Body && Body->IsAlive();
}

float AGSAIControllerBase::GetEntityExtentRadius_Implementation() const
{
	// The CAPSULE, deliberately, not the mesh bounds. #094 measured the goblin meshes at a fraction
	// of their capsules - the player goblin's head sits +40 in a 240 capsule - so mesh bounds would
	// hand ACF a radius that has nothing to do with what actually blocks, traces or collides.
	// Everything else in this project already reasons about the capsule; this stays consistent with
	// it rather than introducing a second notion of how big a goblin is.
	if (const ACharacter* Body = Cast<ACharacter>(GetPawn()))
	{
		if (const UCapsuleComponent* Capsule = Body->GetCapsuleComponent())
		{
			return Capsule->GetScaledCapsuleRadius();
		}
	}
	return 0.f;
}

// How long an AI takes to act on a target it has just noticed. 0 restores the old instant behaviour.
//
// A cvar because this is a FEEL number and the person who can judge it is the one watching the
// fight, not the one who wrote it. 0.35 is a starting guess, not a tuned value.
static float GSAIReactionSeconds = 0.35f;
static FAutoConsoleVariableRef CVarGSAIReactionSeconds(
	TEXT("GS.AI.ReactionSeconds"),
	GSAIReactionSeconds,
	TEXT("Seconds between an AI acquiring a target and being allowed to swing at it. 0 = instant."),
	ECVF_Default);

bool AGSAIControllerBase::HasReactedTo(AActor* Target)
{
	if (!Target || GSAIReactionSeconds <= 0.f)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}

	const float Now = World->GetTimeSeconds();

	// A NEW target restarts the beat. Switching mid-fight should cost the same moment of
	// re-orientation as noticing someone for the first time - otherwise an AI that flicks between
	// two enemies swings instantly at the second one, which is the twitch wearing a different hat.
	if (ReactionTarget.Get() != Target)
	{
		ReactionTarget = Target;
		ReactionStartedTime = Now;
		return false;
	}

	return (Now - ReactionStartedTime) >= GSAIReactionSeconds;
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
