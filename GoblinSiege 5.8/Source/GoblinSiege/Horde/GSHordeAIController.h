// Controller for one allied horde goblin (GDD §2.5, repo export §5).
//
// Runs a Behavior Tree and feeds its blackboard from UGSHordeSubsystem. It does NOT sense anything:
// the constructor declines AGSAIControllerBase's UAIPerceptionComponent outright, because §3.4 makes
// the horde's cheapness a design requirement - "a crowd of perception-less agents fed stimuli by a
// central subsystem, which is what makes ten concurrent goblins cheap." Before #069 this class
// inherited a 1200uu sight sense and a comment recommending it.
//
// The blackboard refresh is a timer, not Tick. Ten goblins ticking to ask the same subsystem the
// same question is the cost this design exists to avoid; a shared cadence answers it well enough
// for a follower, and BT_HordeGoblin re-evaluates on key change rather than on frame.
#pragma once

#include "CoreMinimal.h"
#include "AI/GSAIControllerBase.h"
#include "GSHordeAIController.generated.h"

class UBehaviorTree;

/** What a horde goblin is currently doing. Written to the blackboard as a byte so BT_HordeGoblin
 *  can branch on it without a C++ decorator per state. Ordered by priority, highest first, which
 *  is the order the tree's root Selector should test them in. */
UENUM(BlueprintType)
enum class EGSHordeState : uint8
{
	/** Fire blocks every exit, including vault points. Repo GDD §5's Panic-Stranded. */
	PanicStranded UMETA(DisplayName = "Panic-Stranded"),

	/** Cannot reach its summoner at all. Idles, is free to re-horn, and trudges home to reserve. */
	Stranded      UMETA(DisplayName = "Stranded"),

	/** Executing an explicit point command - swarm, smash, or courier. */
	Commanded     UMETA(DisplayName = "Commanded"),

	/** Auto-engaging a threat the subsystem published. */
	Frenzy        UMETA(DisplayName = "Frenzy"),

	/** Trailing the summoner in a loose scamper - the default. */
	Follow        UMETA(DisplayName = "Follow"),

	/** No summoner at all. Should be transient. */
	Idle          UMETA(DisplayName = "Idle")
};

UCLASS()
class GOBLINSIEGE_API AGSHordeAIController : public AGSAIControllerBase
{
	GENERATED_BODY()

public:
	AGSHordeAIController(const FObjectInitializer& ObjectInitializer);

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

protected:
	/** The companion tree. Soft so a goblin Blueprint with no tree assigned still spawns and stands
	 *  there visibly doing nothing, rather than failing to load. Lives on this class rather than on
	 *  AGSAIControllerBase because the defender path picks its tree from archetype data instead. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde")
	TSoftObjectPtr<UBehaviorTree> CompanionBehaviorTree;

	/** Seconds between blackboard refreshes. At 0.2 a goblin reacts within a fifth of a second,
	 *  which is well inside the time it takes one to cross the gap to a guard. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde", meta = (ClampMin = "0.05"))
	float StimulusRefreshInterval = 0.2f;

	/** Blackboard key names, matching BB_HordeGoblin. Exposed so a renamed key is a data fix
	 *  rather than a six-minute rebuild. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Blackboard")
	FName TargetActorKey = TEXT("TargetActor");

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Blackboard")
	FName FollowTargetKey = TEXT("FollowTarget");

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Blackboard")
	FName FollowSlotKey = TEXT("FollowSlot");

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Blackboard")
	FName HordeStateKey = TEXT("HordeState");

private:
	void RefreshStimulus();

	FTimerHandle StimulusTimer;
};
