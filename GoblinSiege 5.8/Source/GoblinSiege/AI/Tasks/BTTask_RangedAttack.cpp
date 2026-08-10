#include "AI/Tasks/BTTask_RangedAttack.h"

#include "AIController.h"
#include "AI/GSAIDebug.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Characters/GSCharacterBase.h"
#include "Engine/World.h"

UBTTask_RangedAttack::UBTTask_RangedAttack()
{
	NodeName = TEXT("Ranged Attack (bow)");

	// Latent: the draw is the point. A task that fired on the frame it started would make archers
	// hitscan turrets with no tell.
	bNotifyTick = true;
	bNotifyTaskFinished = true;

	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_RangedAttack, TargetKey),
		AActor::StaticClass());
	TargetKey.SelectedKeyName = TEXT("TargetActor");
}

void UBTTask_RangedAttack::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);
	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		TargetKey.ResolveSelectedKey(*BBAsset);
	}
}

EBTNodeResult::Type UBTTask_RangedAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();
	AGSCharacterBase* Self = Controller ? Cast<AGSCharacterBase>(Controller->GetPawn()) : nullptr;
	if (!BB || !Self || !Self->CanRangedAttack())
	{
		// No bow. A militiaman shares this tree in principle; refusing here rather than in the tree
		// keeps "which verbs a character has" as data.
		return EBTNodeResult::Failed;
	}

	AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!IsValid(Target))
	{
		return EBTNodeResult::Failed;
	}

	FGSRangedAttackMemory* Memory = CastInstanceNodeMemory<FGSRangedAttackMemory>(NodeMemory);
	const UWorld* World = OwnerComp.GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	if (Memory && Now < Memory->NextAllowedShotTime)
	{
		return EBTNodeResult::Failed;
	}

	const float Distance = FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation());
	if (Distance < MinRange || Distance > MaxRange)
	{
		// Too close or too far. Failing hands the tick to the chase branch, which walks this archer
		// to a stand-off slot placed at its own preferred range - that is what does the kiting.
		return EBTNodeResult::Failed;
	}

	if (bRequireLineOfSight && !Controller->LineOfSightTo(Target))
	{
		return EBTNodeResult::Failed;
	}

	// THE AIM. Control rotation is what UGSGA_BowShot::FireArrow reads for the muzzle when there is
	// no aim component, and an AI controller's control rotation does not point at anything until it
	// is told to. SetFocus makes it track the target, pitch included.
	Controller->SetFocus(Target, EAIFocusPriority::Gameplay);

	if (Memory)
	{
		Memory->ShotAtTime = Now + DrawSeconds;
	}

	if (GSAIDebug::IsLogging())
	{
		GSAIDebug::Log(Self, FString::Printf(TEXT("drawing on %s (%.0fuu)"),
			*GetNameSafe(Target), Distance));
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_RangedAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	const UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();
	AGSCharacterBase* Self = Controller ? Cast<AGSCharacterBase>(Controller->GetPawn()) : nullptr;
	FGSRangedAttackMemory* Memory = CastInstanceNodeMemory<FGSRangedAttackMemory>(NodeMemory);
	if (!BB || !Self || !Memory)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!IsValid(Target))
	{
		// Died or was dropped mid-draw. Abandoning the shot is right: an arrow at a corpse is a
		// wasted beat the player would read as the archer being stupid.
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	const UWorld* World = OwnerComp.GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	// Keep tracking through the draw, so a moving target is followed rather than led by a stale
	// rotation taken when the draw began.
	Controller->SetFocus(Target, EAIFocusPriority::Gameplay);

	if (Now < Memory->ShotAtTime)
	{
		return;
	}

	// LOFT. Applied to the CONTROL rotation, which is the thing the ability reads. Arrows carry 0.2
	// gravity, so a perfectly flat shot lands short at range; a few degrees up is a lob rather than
	// a ballistic solution, and it is deliberately not distance-scaled - an archer whose arrows all
	// landed dead centre would be far worse to play against than one who mostly does.
	if (AimLoftDegrees > 0.f)
	{
		FRotator Aim = Controller->GetControlRotation();
		Aim.Pitch += AimLoftDegrees;
		Controller->SetControlRotation(Aim);
	}

	const bool bFired = Self->TryRangedAttack();

	if (GSAIDebug::IsLogging())
	{
		GSAIDebug::Log(Self, FString::Printf(TEXT("loosed at %s -> %s"),
			*GetNameSafe(Target), bFired ? TEXT("away") : TEXT("REFUSED (rate limit or blocked)")));
	}

	Memory->NextAllowedShotTime = Now + ShotCooldownSeconds + FMath::FRandRange(0.f, ShotCooldownJitter);

	FinishLatentTask(OwnerComp, bFired ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
}

void UBTTask_RangedAttack::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory,
	EBTNodeResult::Type TaskResult)
{
	// Release the focus on EVERY exit, including abort. A stranded focus pins the control rotation
	// to a target the tree has moved on from, so the archer would keep staring at someone it is no
	// longer fighting - and worse, keep shooting in that direction if the branch ran again.
	if (AAIController* Controller = OwnerComp.GetAIOwner())
	{
		Controller->ClearFocus(EAIFocusPriority::Gameplay);
	}

	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}
