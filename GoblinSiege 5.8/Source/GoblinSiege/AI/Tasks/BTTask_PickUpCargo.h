// Shoulder the ordered cargo. Half one of the courier verb (GDD §2.7); UBTTask_DeliverCargo is the
// other. Written 2026-08-12, ticket #141.
//
// INSTANT, not latent. The walk to the sack is a stock UBTTask_MoveTo ahead of this node in the
// sequence, which keeps the split AGSAIControllerBase's header sets out - decisions in the tree,
// steering on the controller - and avoids re-creating #108, where two nodes owned a position and
// fought over it every frame.
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_PickUpCargo.generated.h"

UCLASS()
class GOBLINSIEGE_API UBTTask_PickUpCargo : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_PickUpCargo();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	/** MUST exist. Writing a key goes through SelectedKeyName and needs no resolution, so a node that
	 *  only writes works without this - but the instant anything asks IsSet(), the answer is a
	 *  permanent false and the feature silently never runs. That cost #086 an evening. */
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
	/** What to pick up. The order's subject - a sack, a pig, a rope chain. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	FBlackboardKeySelector CargoSourceKey;

	/** What we ended up carrying. Written here, cleared by UBTTask_DeliverCargo, and the key the
	 *  courier sub-tree branches on to decide between "go and fetch" and "haul it home". */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	FBlackboardKeySelector CargoKey;

	/** How close is close enough to lift. Generous against the MoveTo acceptance radius ahead of it,
	 *  so arrival and pick-up cannot disagree and leave the goblin bouncing off the sack forever. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float PickUpRange = 180.f;
};
