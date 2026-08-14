// Put the cargo down at the delivery point and go home to the reserve. Half two of the courier verb
// (GDD §2.7). Written 2026-08-12, ticket #141.
//
// THIS IS THE FIRST CALLER UGSHordeSubsystem::NotifyCourierDelivered HAS EVER HAD. That function has
// been written, correct and unreachable since #069 - "a courier who delivers its cargo rejoins the
// pool of summonable goblins" was a rule the code knew and nothing could exercise. The observable
// consequence, and the thing to watch for in PIE, is GS.Horde.Status showing the reserve tick UP by
// one and the active count DOWN by one as the goblin vanishes.
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_DeliverCargo.generated.h"

UCLASS()
class GOBLINSIEGE_API UBTTask_DeliverCargo : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_DeliverCargo();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
	/** What we are hauling. Cleared on success. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	FBlackboardKeySelector CargoKey;

	/** The stones, the arrival marker, or the summoner - whichever
	 *  UGSHordeSubsystem::ResolveDeliveryLocation settled on when the order was issued. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege")
	FBlackboardKeySelector DeliveryLocationKey;

	/** Comfortably larger than the MoveTo acceptance radius ahead of it, so arriving and delivering
	 *  cannot disagree and leave a goblin circling the portal with a pig on its back. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege", meta = (ClampMin = "0.0"))
	float DeliverRange = 300.f;
};
