// Gates the melee branch on holding an attack token from the TARGET's engagement component.
//
// ---- why a decorator and not a check inside the attack task ----------------------------------
// Because of OnCeaseRelevant. A token acquired inside a task is released on the paths the task
// author remembered; a token acquired by a decorator is released on every path the tree can take
// out of that branch, including aborts nobody anticipated - a higher-priority branch winning, the
// target dying mid-swing, the tree restarting. Every implementation of this pattern that leaks
// tokens leaked them by releasing in the task, and a leaked token does not throw: it silently
// removes one attacker's worth of budget from a victim for the rest of the fight, and the symptom
// is a crowd that gradually stops attacking for no visible reason.
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTDecorator_HasAttackToken.generated.h"

UCLASS()
class GOBLINSIEGE_API UBTDecorator_HasAttackToken : public UBTDecorator
{
	GENERATED_BODY()

public:
	UBTDecorator_HasAttackToken();

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	FBlackboardKeySelector TargetKey;

	/**
	 * Weight this agent reserves while its melee branch runs.
	 *
	 * The cost belongs to the ATTACK, not the attacker - a knight's overhead should crowd out two
	 * goblin jabs. It sits here rather than on the ability because the decorator has to know the
	 * price before the ability exists, and because a designer tuning crowd density wants one number
	 * per tree branch rather than a hunt through Blueprint defaults.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "1"))
	int32 TokenCost = 1;
};
