#include "Horde/GSHordeAIController.h"
#include "Kismet/GameplayStatics.h"

AGSHordeAIController::AGSHordeAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AGSHordeAIController::OnPossess(APawn* InPawn)
{
	// AGSAIControllerBase::OnPossess only runs its Behavior-Tree-from-archetype lookup for
	// AGSEnemyCharacter pawns; AGSHordeGoblin isn't one, so that block is a safe no-op here and we
	// still get the base class's AIPerceptionComponent wiring for free (useful once Frenzy reads
	// perception to auto-engage nearby enemies).
	Super::OnPossess(InPawn);

	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		FollowTarget = PlayerPawn;
	}
}

void AGSHordeAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!FollowTarget)
	{
		FollowTarget = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		return;
	}

	TimeSinceLastRepath += DeltaSeconds;
	if (TimeSinceLastRepath < RepathInterval)
	{
		return;
	}
	TimeSinceLastRepath = 0.f;

	const FVector TargetLoc = FollowTarget->GetActorLocation();

	// Loose scamper: only repath once the target has wandered away from where we last aimed, so
	// the goblin doesn't constantly re-plan while already sitting near the player.
	if (FVector::DistSquared(TargetLoc, LastMoveGoal) < FMath::Square(RepathThreshold))
	{
		return;
	}

	// Placeholder scatter offset, not final feel - just enough that a group of followers doesn't
	// stack on a single point. Not seeded on purpose.
	const float Angle = FMath::FRand() * 2.f * PI;
	const FVector Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * FollowRadius;
	LastMoveGoal = TargetLoc + Offset;

	MoveToLocation(LastMoveGoal, /*AcceptanceRadius=*/80.f);
}
