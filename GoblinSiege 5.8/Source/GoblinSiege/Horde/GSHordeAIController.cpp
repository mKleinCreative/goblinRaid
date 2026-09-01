#include "Horde/GSHordeAIController.h"

#include "Horde/GSHordeGoblin.h"
#include "Horde/GSHordeSubsystem.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSHordeAI, Log, All);

// See HandleMoveRequestFinished's header comment. Off by default - fires on every completed move for
// every summoned goblin, which is most goblins most of the time (Follow alone).
static TAutoConsoleVariable<bool> CVarGSLogMoveCompletion(
	TEXT("GS.AI.LogMoveCompletion"), false,
	TEXT("Log AGSHordeAIController's PathFollowingComponent::OnRequestFinished (2026-08-30 diagnostic ")
	TEXT("for the Loot-order MoveTo-completes-but-tree-never-advances bug)."),
	ECVF_Cheat);

AGSHordeAIController::AGSHordeAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// §3.4's "perception-less agents" is NOT actually implemented - this constructor used to pass
	// ObjectInitializer.DoNotCreateDefaultSubobject(TEXT("AIPerceptionComponent")) to Super(), but
	// GSAIControllerBase.cpp's own constructor already documents that Unreal 5.8 ignores that opt-out
	// and force-creates the component anyway (CPF_ObjectMustBeCreated wins). The call was pure dead
	// code: every summoned horde goblin has always carried its own perception component and sight
	// sense, exactly the per-agent cost §3.4 meant to avoid - this comment previously claimed
	// otherwise and was wrong. Removed here (2026-08-30, #387) because the engine logs an Error: line
	// every time the opt-out is ignored, and UAT's cook step hard-fails the whole package on ANY
	// Error:-level log line even though the cook itself completes - this was silently blocking every
	// attempt to package the game. Actually implementing perception-less horde agents (if the cost
	// is ever measured to matter) needs a different mechanism - e.g. a shared/pooled sense, or a
	// runtime toggle on the component after construction - not this constructor trick.
	// TICKS AGAIN AS OF #135, and the line it replaces was silently disabling two shipped features.
	//
	// This used to read `PrimaryActorTick.bCanEverTick = false;` with the note: "No Tick. The old
	// vertical slice ticked to re-issue MoveToLocation at the player; that whole path is gone,
	// replaced by the BT plus a shared refresh timer." That reasoning was correct when written and
	// became wrong the moment the BASE class started doing per-frame work:
	//
	//   #132 put its separation steer on AGSAIControllerBase::Tick and its Evaluate claimed "it covers
	//        the horde too (AGSHordeAIController derives from this)". It never did. Every summoned
	//        goblin walked through its neighbours for the whole life of that ticket.
	//   #133 put the combat facing authority on the same Tick. Same silent no-op, which is exactly
	//        what Michael reported: "the goblins still walk sideways towards an enemy instead of
	//        facing the proper direction."
	//
	// Neither ticket failed to compile, logged anything, or misbehaved in a duel - AGSEnemyCharacter
	// defenders run AGSAIControllerBase directly and tick fine, so both features looked correct
	// everywhere except on the one class that turned them off in its constructor.
	//
	// The original objection is still respected rather than overruled. It was about a per-frame
	// SEARCH - re-issuing MoveToLocation, or ten goblins asking the subsystem the same question every
	// frame (see the header, and GDD §3.4). Neither thing on this tick is that: TickFacing is a
	// blackboard read plus two bools behind an early-out, and TickSeparation's world query is a
	// broadphase overlap throttled to 4Hz with the per-frame half looping over at most four cached
	// pointers. An idle goblin standing in a field costs a switch check.
	//
	// If a big raid ever does regress on frame time, `GS.Combat.Separation 0` and
	// `GS.Combat.FaceTarget 0` isolate the two halves independently without touching this line.
	PrimaryActorTick.bCanEverTick = true;
}

void AGSHordeAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!Cast<AGSHordeGoblin>(InPawn))
	{
		UE_LOG(LogGSHordeAI, Warning, TEXT("Possessed %s, which is not an AGSHordeGoblin."),
			*GetNameSafe(InPawn));
		return;
	}

	UBehaviorTree* Tree = CompanionBehaviorTree.LoadSynchronous();
	if (!Tree)
	{
		// Loud: an unassigned tree is the difference between "the horde stands still" and a
		// diagnosable problem, and standing still is exactly what the old tick-follow looked like
		// when the navmesh refused the move.
		UE_LOG(LogGSHordeAI, Error,
			TEXT("CompanionBehaviorTree is unset on %s - this goblin has no brain. Assign "
			     "BT_HordeGoblin on the controller Blueprint."), *GetNameSafe(this));
		return;
	}

	RunBehaviorTree(Tree);
	RefreshStimulus();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(StimulusTimer, this,
			&AGSHordeAIController::RefreshStimulus, StimulusRefreshInterval, true);
	}

	// DIAGNOSTIC (2026-08-30) - see HandleMoveRequestFinished's header comment. Bound once per
	// possession; OnUnPossess does not need to unbind it explicitly, since the delegate lives on this
	// controller's own PathFollowingComponent, not on something with a longer lifetime than us.
	if (UPathFollowingComponent* PFC = GetPathFollowingComponent())
	{
		PFC->OnRequestFinished.AddUObject(this, &AGSHordeAIController::HandleMoveRequestFinished);
	}
}

void AGSHordeAIController::OnUnPossess()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StimulusTimer);
	}
	Super::OnUnPossess();
}

void AGSHordeAIController::RefreshStimulus()
{
	UBlackboardComponent* BB = GetBlackboardComponent();
	AGSHordeGoblin* Goblin = Cast<AGSHordeGoblin>(GetPawn());
	if (!BB || !Goblin)
	{
		return;
	}

	UGSHordeSubsystem* Horde = UGSHordeSubsystem::Get(this);
	if (!Horde)
	{
		return;
	}

	AActor* Threat = Horde->GetAssignedTargetFor(Goblin);
	AActor* FollowTarget = Horde->GetFollowTargetFor(Goblin);

	// Read the standing order BEFORE publishing FollowTarget - the write below depends on it.
	const EGSHordeOrder RawOrderVerb = Horde->GetOrderVerbFor(Goblin);

	// Resolved once here (rather than again down in the "standing order" block below) because a Loot
	// order with nothing left for THIS goblin to claim - its own target already smashed and looted by
	// someone else, and the area search dry - degrades to no order at all for it specifically, and
	// that has to be decided before bHasStandingOrder gates FollowTarget/FollowLocation above, not
	// after: deciding it later would leave this goblin with FollowTargetKey already nulled out from
	// the RAW order, so "revert to Follow" would have nothing to follow. Attack/Hold/Follow are
	// unaffected - see GetOrderSubjectFor's own comment on why only Loot forages independently, and
	// IsStillLootable's for why a stale claim or a spent Subject no longer counts (2026-08-30, Michael:
	// "if they can't find anything to loot, go ahead and revert to follow").
	AActor* OrderSubject = Horde->GetOrderSubjectFor(Goblin);
	const bool bLootOrderIsEmpty = (RawOrderVerb == EGSHordeOrder::Loot) && !OrderSubject;
	const EGSHordeOrder OrderVerb = bLootOrderIsEmpty ? EGSHordeOrder::None : RawOrderVerb;
	const bool bHasStandingOrder = (OrderVerb != EGSHordeOrder::None);

	BB->SetValueAsObject(TargetActorKey, Threat);

	// AN ORDERED GOBLIN PUBLISHES NO FOLLOW TARGET (2026-08-20, Michael: "we need them to not worry
	// about following me if there's an attack order").
	//
	// BT_HordeGoblin has two branches that were both able to pass at once: "Follow Summoner" behind
	// a `Has A Follow Target` blackboard decorator, and "Chase Target" behind `Has A Target`. This
	// function used to write BOTH keys unconditionally, so a goblin under an Attack order had a live
	// TargetActor AND a live FollowTarget, the Selector had two valid children, and it flip-flopped
	// between them every re-evaluation. On screen that is a goblin that stares at you, breaks off,
	// stares again - which is exactly how it was reported.
	//
	// Clearing the KEY rather than reordering the tree is deliberate: the branch order in
	// BT_HordeGoblin is fine, and the tree cannot be the place this is decided because the
	// controller is the only thing that knows an order exists. A decorator cannot out-vote a key
	// that should never have been set.
	//
	// Note this is NOT the same as cancelling the follow: UGSHordeSubsystem still knows the
	// summoner and the follow slot, so the instant the order clears, the next refresh republishes
	// the target and the goblin falls straight back into the scamper.
	BB->SetValueAsObject(FollowTargetKey, bHasStandingOrder ? nullptr : FollowTarget);

	// A deterministic slot, not a random angle. The slice this replaces rolled FMath::FRand() on
	// every repath, so a goblin standing still still changed its mind about where to stand - which
	// in playtest reads as a pathing bug rather than as scatter. The ring maths itself belongs in
	// the BT's move task; all the controller owes it is a stable index.
	BB->SetValueAsInt(FollowSlotKey, Horde->GetFollowSlotFor(Goblin));

	// ---- AND NOW SOMETHING ACTUALLY READS IT (#263) --------------------------------------------
	//
	// The slot above has been published since the horde was written and consumed by nothing:
	// `Follow Summoner` is a stock BTTask_MoveTo pointed at the FollowTarget OBJECT, so every goblin
	// pathed to the same point - the player - and the band crowded him. The comment above says the
	// ring maths "belongs in the BT's move task"; no such task was ever written, so it lives in
	// UGSHordeSubsystem::GetFollowPostFor and the tree moves to a VECTOR instead.
	//
	// Publishing the goblin's own location while it is already in place is what keeps the band
	// still. A BTTask_MoveTo snapshots its goal at ExecuteTask and ignores later writes -
	// bObserveBlackboardValue is hard-false in UE 5.8 - and it re-executes constantly, so writing a
	// live post every tick would have them all micro-stepping forever. Same fix, same reason, as the
	// archers in #247.
	if (!bHasStandingOrder && FollowTarget)
	{
		const FVector Post = Horde->GetFollowPostFor(Goblin);
		const FVector Here = Goblin->GetActorLocation();
		const bool bInPlace = FVector::Dist2D(Here, Post) <= FollowPostTolerance;
		BB->SetValueAsVector(FollowLocationKey, bInPlace ? Here : Post);
	}

	// ---- the standing order (#141) ---------------------------------------------------------
	const EGSHordeOrder Verb = OrderVerb;   // the (possibly Loot-degraded) verb resolved above
	BB->SetValueAsEnum(OrderVerbKey, static_cast<uint8>(Verb));
	BB->SetValueAsObject(OrderSubjectKey, OrderSubject);   // same call as above; GetOrderSubjectFor
	                                                        // also mutates the area-forage claim, so
	                                                        // this must stay a single call per refresh.
	BB->SetValueAsVector(OrderLocationKey, Horde->GetOrderLocationFor(Goblin));
	BB->SetValueAsVector(DeliveryLocationKey, Horde->GetDeliveryLocationFor(Goblin));

	// THE FIRST WRITER OF Commanded, which has sat in EGSHordeState since #069 with nothing
	// anywhere setting it (#088 listed the point command as deliberately undone).
	//
	// Follow is absent from this test on purpose: UGSHordeSubsystem::IssueOrder clears rather than
	// stores it, so a Follow order arrives here as None and the goblin drops to the ordinary
	// Frenzy/Follow choice below - which IS what the recall means.
	//
	// Note that an Attack order yields Commanded AND a live TargetActor at the same time. That is
	// correct and it is why BT_HordeGoblin's combat branches must stay gated on `TargetActor Is Set`
	// rather than on `HordeState == Frenzy`: gating them on the state would make an ordered goblin
	// walk up to its victim and refuse to swing. They are already written that way; do not "tidy"
	// them onto the state key.
	const bool bCommanded = (Verb != EGSHordeOrder::None);

	// Stranded and PanicStranded remain unwritten - they belong to the reachability checks, which
	// are still nobody's work.
	const EGSHordeState State = bCommanded
		? EGSHordeState::Commanded
		: (Threat ? EGSHordeState::Frenzy
		          : (FollowTarget ? EGSHordeState::Follow : EGSHordeState::Idle));
	BB->SetValueAsEnum(HordeStateKey, static_cast<uint8>(State));
}

FPathFollowingRequestResult AGSHordeAIController::MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath)
{
	const FPathFollowingRequestResult Result = Super::MoveTo(MoveRequest, OutPath);

	if (CVarGSLogMoveCompletion.GetValueOnGameThread())
	{
		UE_LOG(LogGSHordeAI, Warning,
			TEXT("[GS.MoveCompletion] %s: MoveTo() synchronous result Code=%d (0=Failed,1=AlreadyAtGoal,")
			TEXT("2=RequestSuccessful) MoveId=%u, GoalActor='%s', UsePathfinding=%d, ProjectGoalToNav=%d, ")
			TEXT("AcceptanceRadius=%.1f"),
			*GetNameSafe(this),
			static_cast<int32>(Result.Code),
			Result.MoveId.GetID(),
			*GetNameSafe(MoveRequest.GetGoalActor()),
			MoveRequest.IsUsingPathfinding() ? 1 : 0,
			MoveRequest.IsProjectingGoal() ? 1 : 0,
			MoveRequest.GetAcceptanceRadius());
	}

	return Result;
}

void AGSHordeAIController::HandleMoveRequestFinished(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (!CVarGSLogMoveCompletion.GetValueOnGameThread())
	{
		return;
	}

	const UPathFollowingComponent* PFC = GetPathFollowingComponent();
	const APawn* ControlledPawn = GetPawn();
	if (!PFC || !ControlledPawn)
	{
		return;
	}

	// Same key RefreshStimulus already writes OrderSubject to - reading it back here, rather than
	// caching it at request time, is deliberate: we want to know what the tree currently believes the
	// subject is at the moment completion fires, not what it was when the move started.
	const UBlackboardComponent* BB = GetBlackboardComponent();
	AActor* Subject = BB ? Cast<AActor>(BB->GetValueAsObject(OrderSubjectKey)) : nullptr;
	const float DistToSubject = IsValid(Subject)
		? FVector::Dist2D(Subject->GetActorLocation(), ControlledPawn->GetActorLocation())
		: -1.f;

	UE_LOG(LogGSHordeAI, Warning,
		TEXT("[GS.MoveCompletion] %s: OnRequestFinished(Request=%u, Success=%d, Code=%d) - ")
		TEXT("GetMoveStatus=%d, DidMoveReachGoal=%d, AcceptanceRadius=%.1f, OrderSubject='%s', DistToSubject=%.1f"),
		*GetNameSafe(this),
		RequestID.GetID(),
		Result.IsSuccess() ? 1 : 0,
		static_cast<int32>(Result.Code),
		static_cast<int32>(GetMoveStatus()),
		PFC->DidMoveReachGoal() ? 1 : 0,
		PFC->GetAcceptanceRadius(),
		*GetNameSafe(Subject),
		DistToSubject);
}
