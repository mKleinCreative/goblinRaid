#include "AI/Tasks/BTTask_MenaceOrbit.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Characters/GSCharacterBase.h"
#include "Engine/World.h"

UBTTask_MenaceOrbit::UBTTask_MenaceOrbit()
{
	NodeName = TEXT("Menace Orbit (no token)");

	// Latent: this runs until the tree takes it away, which happens the moment the decorator above
	// wins a token. That abort IS the transition from waiting to attacking.
	bNotifyTick = true;

	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MenaceOrbit, TargetKey),
		AActor::StaticClass());
	TargetKey.SelectedKeyName = TEXT("TargetActor");

	// Same key the MoveTo sibling reads, so the two nodes cannot disagree about where to stand.
	StationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MenaceOrbit, StationKey));
	StationKey.SelectedKeyName = TEXT("TargetLocation");
}

void UBTTask_MenaceOrbit::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);
	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		TargetKey.ResolveSelectedKey(*BBAsset);
		// Without this the selector's type is never populated and the key reads as unset - the
		// permanent-false trap from #086.
		StationKey.ResolveSelectedKey(*BBAsset);
	}
}

EBTNodeResult::Type UBTTask_MenaceOrbit::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	const AAIController* Controller = OwnerComp.GetAIOwner();
	if (!BB || !Controller || !Controller->GetPawn())
	{
		return EBTNodeResult::Failed;
	}

	const AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!IsValid(Target))
	{
		return EBTNodeResult::Failed;
	}

	// Too far to circle - this node steers, it does not path. Fail and let the chase branch below
	// bring the agent into orbit range using the navmesh.
	if (FVector::Dist(Target->GetActorLocation(), Controller->GetPawn()->GetActorLocation()) > MaxOrbitDistance)
	{
		return EBTNodeResult::Failed;
	}

	if (FGSMenaceOrbitMemory* Memory = CastInstanceNodeMemory<FGSMenaceOrbitMemory>(NodeMemory))
	{
		const UWorld* World = OwnerComp.GetWorld();
		const float Now = World ? World->GetTimeSeconds() : 0.f;

		// Direction is sticky across re-entries - see the note on StrafeSign.
		if (FMath::IsNearlyZero(Memory->StrafeSign))
		{
			Memory->StrafeSign = FMath::RandBool() ? 1.f : -1.f;
		}
		if (Memory->NextFeintTime <= 0.f)
		{
			Memory->NextFeintTime = Now + FMath::FRandRange(FeintIntervalMin, FeintIntervalMax);
		}
		Memory->FeintUntilTime = 0.f;
		Memory->ReevaluateAtTime = Now + OrbitReevaluateSeconds;
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_MenaceOrbit::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	const UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();
	AGSCharacterBase* Self = Controller ? Cast<AGSCharacterBase>(Controller->GetPawn()) : nullptr;
	if (!BB || !Self)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	const AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!IsValid(Target))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	FGSMenaceOrbitMemory* Memory = CastInstanceNodeMemory<FGSMenaceOrbitMemory>(NodeMemory);
	const UWorld* World = OwnerComp.GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	// Hand the branch back periodically so the Selector can notice a token has freed up. Without
	// this the agent circles forever, because a Selector only reconsiders when its running child
	// finishes.
	if (Memory && Now >= Memory->ReevaluateAtTime)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	const FVector TargetLoc = Target->GetActorLocation();
	const FVector SelfLoc = Self->GetActorLocation();
	FVector ToTarget = TargetLoc - SelfLoc;
	ToTarget.Z = 0.f;
	const float Distance = ToTarget.Size();
	if (Distance < KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Drifted out of orbit range (the target ran). Give the branch back so the chase can path.
	if (Distance > MaxOrbitDistance)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}
	const FVector Forward = ToTarget / Distance;

	// Always face the target. A circling enemy that looks where it is walking rather than at you is
	// patrolling, not menacing - and it also means the guard arc is pointing the right way the
	// instant a token arrives.
	//
	// TURNED AT A RATE, NOT SET. This used to assign the yaw outright every tick, which is fine while
	// the bearing drifts by a degree a frame and is a hard visual snap the moment it does not: a
	// victim running past an orbiter swings the bearing through 180 degrees, and the agent flipped to
	// face it in a single frame. Measured over 65 samples of a live patrol fight, single-step yaw
	// changes reached 121, 149 and 166.9 degrees - that IS the snapping, and it is not caused by
	// anything being wrong with the targeting: the goal jumps that accompany it are mostly a slot
	// legitimately following a guard sprinting at up to 1239uu/s.
	//
	// TurnRateRadPerSec is the character's own dial, already used for exactly this by UBTTask_Block,
	// so a goblin turns like a goblin rather than at a rate invented here.
	const float DesiredYaw = Forward.Rotation().Yaw;
	const float CurrentYaw = Self->GetActorRotation().Yaw;
	const float MaxStepDeg = FMath::RadiansToDegrees(FMath::Max(Self->GetTurnRateRadPerSec(), 0.01f))
		* DeltaSeconds;
	const float DeltaYaw = FMath::Clamp(FMath::FindDeltaAngleDegrees(CurrentYaw, DesiredYaw),
		-MaxStepDeg, MaxStepDeg);
	Self->SetActorRotation(FRotator(0.f, CurrentYaw + DeltaYaw, 0.f));

	// Feint: a committed step in, then back out. Started on a per-agent clock so a ring of waiting
	// enemies pulses raggedly rather than lunging in unison.
	if (Memory && Now >= Memory->NextFeintTime && Memory->FeintUntilTime <= 0.f)
	{
		Memory->FeintUntilTime = Now + FeintDurationSeconds;
		Memory->NextFeintTime = Now + FMath::FRandRange(FeintIntervalMin, FeintIntervalMax);
	}

	const bool bFeinting = Memory && Now < Memory->FeintUntilTime;
	if (Memory && !bFeinting && Memory->FeintUntilTime > 0.f)
	{
		Memory->FeintUntilTime = 0.f;
	}

	// EACH ORBITER GETS ITS OWN BEARING, and it must be the SAME station BTTask_MoveTo is using.
	//
	// The first version of this took the claimed slot's bearing but stood at OrbitRadius (300) along
	// it, while the service writes that same slot at RingRadius (180) into TargetLocation for the
	// MoveTo sibling below. Because this task hands the branch back every OrbitReevaluateSeconds, the
	// Selector then ran MoveTo, which dragged the agent to 180 - and on the next cycle this task
	// pushed it back out to 300. Measured as agents parked at 338/341/359 against a MoveTo goal at
	// 180: a 0.6-second ping-pong between two nodes that disagreed about where the agent belongs.
	// That is what read as jittering and snapping.
	//
	// One station, read from the blackboard the service already writes. The un-clumping survives -
	// it came from the slot being exclusive and per-agent, not from the extra radius.
	FVector Station = TargetLoc - Forward * OrbitRadius;   // fallback if the key is unset
	if (BB && StationKey.SelectedKeyName != NAME_None)
	{
		const FVector KeyStation = BB->GetValueAsVector(StationKey.SelectedKeyName);
		// The key is FLT_MAX-ish until the service has written it once.
		if (FMath::Abs(KeyStation.X) < 1.0e6f && !KeyStation.ContainsNaN())
		{
			Station = KeyStation;
		}
	}

	FVector ToStation = Station - SelfLoc;
	ToStation.Z = 0.f;
	const float StationError = ToStation.Size();

	FVector Input;
	if (bFeinting)
	{
		Input = Forward * FeintSpeedScale;
	}
	else if (StationError > OnStationTolerance)
	{
		// Walk to my own place in the ring. This is the line that un-clumps the crowd.
		Input = (ToStation / StationError) * StrafeSpeedScale;
	}
	else
	{
		// On station. Keep shuffling rather than standing in a rest pose - the research is explicit
		// that a motionless waiting attacker reads as broken - but the shuffle needs a restoring term
		// or it is just a slow walk away from the station until the tolerance trips and yanks the
		// agent back. That drift-and-snap is its own flavour of the jitter this ticket is fixing, so
		// the tangential shuffle is paired with a spring back toward the station, strengthening with
		// the error. The result circles ABOUT the station instead of leaving it.
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
		const float Pull = FMath::Clamp(StationError / FMath::Max(OnStationTolerance, 1.f), 0.f, 1.f);
		Input = Right * ((Memory && !FMath::IsNearlyZero(Memory->StrafeSign)) ? Memory->StrafeSign : 1.f)
			* StrafeSpeedScale * OnStationShuffleScale;
		if (StationError > KINDA_SMALL_NUMBER)
		{
			Input += (ToStation / StationError) * StrafeSpeedScale * OnStationShuffleScale * Pull;
		}
	}

	Self->AddMovementInput(Input.GetSafeNormal(), FMath::Min(Input.Size(), 1.f));
}
