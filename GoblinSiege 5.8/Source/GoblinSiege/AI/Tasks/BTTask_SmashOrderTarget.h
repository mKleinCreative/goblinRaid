// Break the ordered prop. The "aim at a stall, a fence, a pen gate - smash it" half of GDD §2.5's
// point command. Written 2026-08-12, ticket #141.
//
// WHY THIS NODE HAS TO EXIST AT ALL, when the goblins already have swords. A melee swing resolves
// its victim's AbilitySystemComponent and applies a gameplay effect to it (GSGA_SwordLight.cpp:363),
// behind an IsHostileTo check that a StaticMeshActor cannot answer. A crate has no ASC and no race
// tag, so a goblin ordered to attack one would walk up, swing, and achieve precisely nothing - and
// it would look exactly like a bug. The ONLY caller of UGSBreakableComponent::Break anywhere in this
// module is GSTorchProjectile.cpp:218; until now, fire was the only thing in the game that could
// break a prop.
//
// This node does not make melee damage props in general. It resolves one ordered target, plays the
// swing so the break reads as a goblin doing it, and breaks it.
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_SmashOrderTarget.generated.h"

UCLASS()
class GOBLINSIEGE_API UBTTask_SmashOrderTarget : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SmashOrderTarget();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
	/** The order's subject. Anything without a UGSBreakableComponent fails out of this branch and
	 *  falls through to the ordinary combat branches, which is correct: Attack on a living guard is
	 *  not this node's business. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	FBlackboardKeySelector TargetKey;

	/** Matches UBTTask_MeleeAttack's AttackRange, measured the same way (Dist2D). */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float SmashRange = 250.f;

	/** Impulse handed to the debris so shards fly away from the goblin rather than through it. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float DebrisImpulse = 300.f;
};
