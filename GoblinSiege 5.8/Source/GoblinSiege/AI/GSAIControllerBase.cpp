#include "AI/GSAIControllerBase.h"

#include "Components/ACFAIPatrolComponent.h"
#include "AI/GSAISteeringComponent.h"
#include "Characters/GSCharacterBase.h"
#include "Characters/GSEnemyCharacter.h"
#include "Combat/GSRaceDataAsset.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "AI/GSAIDebug.h"
#include "EngineUtils.h"
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
					// ---- DO NOT SWAP ACF'S BLACKBOARD OUT FROM UNDER IT (#346/#347) ------------
					//
					// Super::OnPossess is AACFAIController::OnPossess, and it caches blackboard key
					// INDICES - targetActorKey, homeDistanceKey and eight more - by name against
					// whatever blackboard its own BehaviorTree declares (ACFAIController.cpp:88-97,
					// against ACFAIBB, 13 keys). Those cached indices are integers, not names.
					//
					// RunBehaviorTree() here re-initialises the blackboard component with a
					// DIFFERENT asset. If that asset is smaller, every cached index above its size
					// becomes a read off the end of the array, and ACF's own perception handler
					// dereferences one on the very first stimulus:
					//
					//   Array index out of bounds: 11 into an array of size 5
					//   UBlackboardComponent::GetValue<UBlackboardKeyType_Float>()
					//   AACFAIController::HandlePerceptionUpdated_Implementation()  [:169]
					//
					// Index 11 in ACFAIBB is HomeDistance, a Float - which is exactly the type in
					// that callstack. BB_Human has 5 keys. So a Knight or Archer row pointing at
					// BT_Militia / BT_Archer takes the editor down the moment anything sees anything.
					//
					// This crashed the editor twice on 2026-08-28 and the data has already drifted
					// back once after being cleared by hand, which is why the check lives HERE and
					// not in the data. A mismatched tree is refused and named; it is never run.
					const UBlackboardData* ArchetypeBB = BT->BlackboardAsset;
					const UBlackboardComponent* ExistingBB = GetBlackboardComponent();
					const UBlackboardData* ACFBB = ExistingBB ? ExistingBB->GetBlackboardAsset() : nullptr;

					if (ACFBB && ArchetypeBB && ArchetypeBB != ACFBB)
					{
						UE_LOG(LogTemp, Error,
							TEXT("[GoblinSiege] REFUSED to run '%s' on %s (archetype row '%s'): it "
								 "declares blackboard '%s' but this controller is already running "
								 "'%s'. Swapping it would invalidate the blackboard key indices "
								 "AACFAIController cached in OnPossess and crash on the first "
								 "perception update. Clear BehaviorTree on that archetype row, or "
								 "give the tree ACF's blackboard."),
							*BT->GetName(), *InPawn->GetName(), *Enemy->GetArchetypeRowName().ToString(),
							*GetNameSafe(ArchetypeBB), *GetNameSafe(ACFBB));
					}
					else
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
	}

	// The archetype lookup above is a DEFENDER path - it needs AGSEnemyCharacter's race data, which
	// a horde goblin does not have. Allied goblins run a single fixed companion BT instead, and
	// since #069 that is owned by AGSHordeAIController::CompanionBehaviorTree rather than by a
	// Blueprint subclass of this class. Nothing here needs to know about them.

	// #331 - SEED THE PATROL. ACF's patrol loop is not self-starting. UACFAIPatrolComponent's own
	// header states the contract: "Call StartPatrolLoop() once the owning pawn has a valid
	// AACFAIController", and the only callers ACF ships are its ROUTINE tasks
	// (UACFPatrolSplinePathTask, UACFFollowSplinePathTask, UACFRandomPatrolAroundPointTask), each of
	// which sets the path, sets the AI state and starts the loop together.
	//
	// Our guards are PLACED IN THE LEVEL with PathToFollow authored on the component, so they never
	// run a routine and nothing ever made that call. That - not the behaviour tree - is why every
	// defender stood still. Do NOT "fix" this by rewriting BT_Defender's patrol branch: that branch
	// is a faithful copy of ACF's own ACFBT (verified 2026-08-27 by walking both trees; ACF_HorseBT
	// has the same shape), so changing it would be diverging from ACF, not adopting it.
	//
	// Measured in PIE on L_Tutorial_Island, 2026-08-27, before this call existed: all 7 placed
	// guards reported IsPatrolLoopActive() == false; one StartPatrolLoop(true) each and 5 of 7 were
	// walking their GS_Road splines on the next sample. StartPatrolLoop also binds
	// HandleMoveCompleted, which is what re-requests a waypoint after every completed move.
	// A fight next to a guard was invisible to it: ACF's alerting is group-gated and our defenders
	// have no group, and nothing in this project makes a noise. See the header for the measurement.
	if (AGSCharacterBase* Damageable = Cast<AGSCharacterBase>(InPawn))
	{
		if (!Damageable->OnDamaged.IsAlreadyBound(this, &AGSAIControllerBase::HandlePawnDamagedAlertAllies))
		{
			Damageable->OnDamaged.AddDynamic(this, &AGSAIControllerBase::HandlePawnDamagedAlertAllies);
		}
	}

	if (InPawn)
	{
		if (UACFAIPatrolComponent* Patrol = InPawn->FindComponentByClass<UACFAIPatrolComponent>())
		{
			// A spline patroller with no path would just fail TryGetNextWaypoint forever. Random
			// patrollers need no path - they read the controller's home location instead.
			const bool bHasRoute = Patrol->GetPatrolType() != EPatrolType::EFollowSpline
				|| Patrol->GetPathToFollow() != nullptr;

			if (bHasRoute)
			{
				Patrol->StartPatrolLoop(true);
			}
		}
	}
}

void AGSAIControllerBase::HandlePawnDamagedAlertAllies(AActor* Attacker, float Damage)
{
	APawn* Self = GetPawn();
	if (!IsValid(Attacker) || !Self || AllyAlertRadius <= 0.f)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin = Self->GetActorLocation();
	const float RadiusSq = AllyAlertRadius * AllyAlertRadius;
	int32 Woken = 0;

	// O(controllers), not O(actors): the iterator walks the controller list, which is the handful of
	// AI in the level rather than the nine thousand actors on Tutorial Island.
	for (TActorIterator<AGSAIControllerBase> It(const_cast<UWorld*>(World)); It; ++It)
	{
		AGSAIControllerBase* Other = *It;
		if (!Other || Other == this)
		{
			continue;
		}

		APawn* OtherPawn = Other->GetPawn();
		AGSCharacterBase* OtherChar = Cast<AGSCharacterBase>(OtherPawn);
		if (!OtherChar || !OtherChar->IsAlive())
		{
			continue;
		}

		// Only wake our own side, and only for something they would actually fight. Without both
		// tests a wounded guard would point his neighbours at another guard.
		const AGSCharacterBase* SelfChar = Cast<AGSCharacterBase>(Self);
		if (SelfChar && OtherChar->IsHostileTo(SelfChar))
		{
			continue; // hostile to us, so not an ally
		}
		if (!OtherChar->IsHostileTo(Attacker))
		{
			continue; // the attacker is not their enemy either
		}

		if (FVector::DistSquared(OtherPawn->GetActorLocation(), Origin) > RadiusSq)
		{
			continue;
		}

		// Already busy with someone: do not yank an engaged guard off his own fight.
		if (Other->GetTarget())
		{
			continue;
		}

		Other->SetTarget(Attacker);
		++Woken;
	}

	if (Woken > 0 && GSAIDebug::IsLogging())
	{
		GSAIDebug::Log(Self, FString::Printf(
			TEXT("hit by %s - woke %d ally/allies within %.0fuu"),
			*GetNameSafe(Attacker), Woken, AllyAlertRadius));
	}
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
