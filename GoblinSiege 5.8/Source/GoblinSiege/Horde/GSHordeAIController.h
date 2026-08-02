// Minimal vertical slice of the horde AI controller (tech design doc §18, AGSHordeAIController).
// Possesses a AGSHordeGoblin and makes it follow the local player pawn in a loose scamper - the
// "Follow" state from design doc §5's Follow/Frenzy/Commanded model. Frenzy (auto-engage nearby
// enemies), Commanded (point command), Courier, the horn-summon reserve pool
// (UGSHordeSubsystem), and shared DetourCrowd avoidance are NOT implemented here yet - this is
// deliberately just enough to see one follower on screen moving with the player. Multiple
// followers will currently each run their own MoveToLocation rather than a shared crowd solve.
#pragma once

#include "CoreMinimal.h"
#include "AI/GSAIControllerBase.h"
#include "GSHordeAIController.generated.h"

UCLASS()
class GOBLINSIEGE_API AGSHordeAIController : public AGSAIControllerBase
{
	GENERATED_BODY()

public:
	AGSHordeAIController();

	virtual void OnPossess(APawn* InPawn) override;

protected:
	virtual void Tick(float DeltaSeconds) override;

	/** Local player pawn this companion trails. Set on possess; re-resolved if null (e.g. player
	 *  respawn). TODO: replace with an explicit summoner reference once UGSHordeSubsystem exists -
	 *  "always follow player 0" doesn't hold up in co-op. */
	UPROPERTY(Transient)
	TObjectPtr<APawn> FollowTarget;

	/** How far around the target the goblin tries to sit - "loose scamper" per design doc §5. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde")
	float FollowRadius = 220.f;

	/** Only re-issue MoveTo once the target has wandered this far from our last aim point, so we
	 *  don't re-path every tick while the goblin is already sitting near the player. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde")
	float RepathThreshold = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde")
	float RepathInterval = 0.5f;

	FVector LastMoveGoal = FVector::ZeroVector;
	float TimeSinceLastRepath = 0.f;
};
