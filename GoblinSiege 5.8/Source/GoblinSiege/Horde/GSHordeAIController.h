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
//
// CAREFUL WITH THAT PARAGRAPH - it is about the blackboard refresh, and it is NOT a blanket ban on
// ticking (#135). Read literally, it was: this class set bCanEverTick = false in its constructor,
// and when AGSAIControllerBase gained per-frame work the horde silently opted out of it. #132's
// separation steer and #133's facing authority were both shipped believing they covered the horde,
// and neither had ever executed on a summoned goblin. Nothing failed loudly - defenders possess
// AGSAIControllerBase directly and behaved correctly the whole time.
//
// The controller ticks now. The rule that survives is the real one: NO PER-AGENT SEARCH on the
// frame. A broadphase overlap at 4Hz behind early-outs is fine; a world iteration, a subsystem
// query, or a re-issued MoveTo per goblin per frame is not. If you add work to
// AGSAIControllerBase::Tick, that is the bar it has to clear - and it now genuinely runs on every
// summoned goblin, so measure it against ten of them, not one defender.
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

	// ---- the order board (#141) ------------------------------------------------------------
	// Four more keys, written from the same refresh. The cost is four blackboard writes per goblin
	// at 5Hz, which clears this class's stated bar comfortably: no world iteration, no per-agent
	// search, no re-issued MoveTo. The expensive half (finding the delivery point) is resolved once
	// when the order is issued and cached on it - see FGSHordeOrder::DeliveryLocation.

	/** EGSHordeOrder as a byte. None (0) is the un-commanded default, so a tree with no order branch
	 *  behaves exactly as it did before this ticket. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Blackboard")
	FName OrderVerbKey = TEXT("OrderVerb");

	/** What to attack, smash or carry. Null for a Hold order, which is a location and nothing else. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Blackboard")
	FName OrderSubjectKey = TEXT("OrderSubject");

	/** Where the beacon is.
	 *
	 *  NOT TargetLocation, and that is not a naming preference. UBTService_AcquireTarget OWNS
	 *  TargetLocation and rewrites it every tick with the ring-slot standoff around TargetActor
	 *  (BTService_AcquireTarget.cpp:368), so a Hold point written there would survive about 0.2
	 *  seconds - and the header explains that key exists specifically to dodge the ragdoll-bounds
	 *  AlreadyAtGoal freeze, which is not a mechanism worth sharing a key with. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Blackboard")
	FName OrderLocationKey = TEXT("OrderLocation");

	/** Where a courier takes its cargo (§2.7). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Blackboard")
	FName DeliveryLocationKey = TEXT("DeliveryLocation");

private:
	void RefreshStimulus();

	FTimerHandle StimulusTimer;
};
