// Writes something worth attacking into the "TargetActor" blackboard key (the key name
// AGSAIControllerBase's header nominates as canonical for every archetype's tree).
//
// Deliberately the crude version: the player pawn, if it is within AcquireRadius. Real selection
// belongs in an EQS query run from the tree - AGSAIControllerBase::HandlePerceptionUpdated says so
// and does nothing on purpose, so that an Archer can prioritise range while a Knight prioritises
// nearest without controller code branching per archetype. This service exists so defenders can be
// seen to move and fight TODAY; it is the piece expected to be replaced first.
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTService_AcquireTarget.generated.h"

UCLASS()
class GOBLINSIEGE_API UBTService_AcquireTarget : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_AcquireTarget();

	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

protected:
	/** Blackboard key to write. Defaults to TargetActor to match the controller's documented set. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	FBlackboardKeySelector TargetKey;

	/** The same target as a plain location, written alongside TargetActor.
	 *
	 *  Not redundant: MoveTo against an ACTOR goal resolves "have I arrived" against that actor's
	 *  collision extent, and BP_GSPlayerCharacter's colliding bounds are ~5385x4748 units (its
	 *  CollisionCylinder is NO_COLLISION and something attached is enormous). Every defender inside
	 *  ~50m therefore gets PathFollowingRequestResult::AlreadyAtGoal and never takes a step.
	 *  Chasing the location sidesteps that entirely, and keeps working if the bounds are ever fixed. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	FBlackboardKeySelector TargetLocationKey;

	/** Beyond this the key is cleared, so the tree falls back to its idle branch instead of a
	 *  defender jogging at a player on the far side of the village. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	float AcquireRadius = 3000.f;
};
