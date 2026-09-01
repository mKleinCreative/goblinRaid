// Crack the ordered container open and empty it - the second half of "attack the crate then interact
// and steal what's inside" (Michael, 2026-08-30). UBTTask_SmashOrderTarget already breaks the prop;
// this is what turns a BROKEN, now-available interactable into banked loot for an order that has no
// carryable subject to shoulder home. Sibling to UBTTask_PickUpCargo, not a replacement for it - a
// pig still gets carried, a crate gets looted on the spot.
//
// INSTANT, not latent, same reasoning as UBTTask_PickUpCargo: the walk is a MoveTo ahead of this node,
// and a real per-goblin channel timer would fight that node for the same position every frame (#108).
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_LootInPlace.generated.h"

UCLASS()
class GOBLINSIEGE_API UBTTask_LootInPlace : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_LootInPlace();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
	/** The order's subject. Anything without a UGSInteractableComponent, or one that is still locked
	 *  (an unbroken crate - smash it first) or already-carryable (a pig - UBTTask_PickUpCargo's job,
	 *  not this node's), fails out. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	FBlackboardKeySelector TargetKey;

	/**
	 * Started as "matches UBTTask_PickUpCargo's PickUpRange" (180, same "walk up and use your hands"
	 * distance) - correct for the Carry sequence this shares a Sequence with MoveTo_1's acceptance
	 * radius, but wrong for the Smash-and-Loot sequence: UBTTask_SmashOrderTarget's SmashRange is
	 * 250uu (matched to UBTTask_MeleeAttack's own reach) and, unlike a MoveTo, the smash never moves
	 * the goblin at all - a swing that lands from near the edge of SmashRange, or the crowd-following
	 * agent's own avoidance slop while several goblins converge on one prop, can easily leave the
	 * goblin farther from the target than 180uu with no further MoveTo in the sequence to close the
	 * gap. Michael, watching it live: "it smashed, then wandered" - the goblin was correctly stuck
	 * unable to loot what it had just broken open. Raised past SmashRange with margin, the same
	 * "generous against what comes before it" reasoning PickUpRange's own comment already gives,
	 * rather than lowering SmashRange and losing its own intentional match to melee reach.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float InteractRange = 300.f;
};
