#include "Horde/GSHordeAIController.h"

#include "Horde/GSHordeGoblin.h"
#include "Horde/GSHordeSubsystem.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSHordeAI, Log, All);

AGSHordeAIController::AGSHordeAIController(const FObjectInitializer& ObjectInitializer)
	// Decline the perception component. This is the line that implements §3.4's "perception-less
	// agents" - without it every summoned goblin carries its own 1200uu sight sense feeding a
	// handler that does nothing, ten times over, which is precisely the cost the horde design is
	// built to avoid.
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(TEXT("AIPerceptionComponent")))
{
	// No Tick. The old vertical slice ticked to re-issue MoveToLocation at the player; that whole
	// path is gone, replaced by the BT plus a shared refresh timer.
	PrimaryActorTick.bCanEverTick = false;
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

	BB->SetValueAsObject(TargetActorKey, Threat);
	BB->SetValueAsObject(FollowTargetKey, FollowTarget);

	// A deterministic slot, not a random angle. The slice this replaces rolled FMath::FRand() on
	// every repath, so a goblin standing still still changed its mind about where to stand - which
	// in playtest reads as a pathing bug rather than as scatter. The ring maths itself belongs in
	// the BT's move task; all the controller owes it is a stable index.
	BB->SetValueAsInt(FollowSlotKey, Horde->GetFollowSlotFor(Goblin));

	// Commanded, Stranded and PanicStranded are set by the point command and the reachability
	// checks, not here - this only chooses between the two states the subsystem can see.
	const EGSHordeState State = Threat
		? EGSHordeState::Frenzy
		: (FollowTarget ? EGSHordeState::Follow : EGSHordeState::Idle);
	BB->SetValueAsEnum(HordeStateKey, static_cast<uint8>(State));
}
