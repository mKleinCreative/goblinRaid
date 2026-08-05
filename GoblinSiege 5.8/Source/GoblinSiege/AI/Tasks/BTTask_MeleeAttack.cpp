#include "AI/Tasks/BTTask_MeleeAttack.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/GSEnemyCharacter.h"
#include "Combat/GSRaceDataAsset.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"

UBTTask_MeleeAttack::UBTTask_MeleeAttack()
{
	NodeName = TEXT("Melee Attack (TryLightAttack)");

	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MeleeAttack, TargetKey),
		AActor::StaticClass());
	TargetKey.SelectedKeyName = TEXT("TargetActor");
}

EBTNodeResult::Type UBTTask_MeleeAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!BB || !Controller)
	{
		return EBTNodeResult::Failed;
	}

	FGSMeleeAttackMemory* Memory = CastInstanceNodeMemory<FGSMeleeAttackMemory>(NodeMemory);
	const UWorld* World = OwnerComp.GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	if (Memory && Now < Memory->NextAllowedAttackTime)
	{
		// Still on cooldown. Failing lets the Selector fall through to the chase branch, which keeps
		// the defender repositioning between swings instead of standing frozen mid-cooldown.
		return EBTNodeResult::Failed;
	}

	AGSEnemyCharacter* Self = Cast<AGSEnemyCharacter>(Controller->GetPawn());
	AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!Self || !IsValid(Target))
	{
		return EBTNodeResult::Failed;
	}

	if (FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation()) > AttackRange)
	{
		return EBTNodeResult::Failed;
	}

	if (bFaceTargetBeforeSwing)
	{
		const FRotator Look = UKismetMathLibrary::FindLookAtRotation(
			Self->GetActorLocation(), Target->GetActorLocation());
		Self->SetActorRotation(FRotator(0.f, Look.Yaw, 0.f));
	}

	// Failing when the swing is refused is deliberate: GAS refuses re-activation while a swing is
	// already running, and UGSGA_SwordLight treats that refusal as its combo buffer. Reporting
	// Failed lets the Selector fall through to the chase branch instead of the tree stalling on a
	// task waiting for an ability that will never start.
	if (!Self->TryLightAttack())
	{
		return EBTNodeResult::Failed;
	}

	// Pace the NEXT swing, and only after one actually started - a refused activation must not
	// silently start a cooldown. The archetype owns the rate where it has an opinion; the jitter is
	// what breaks a patrol that arrived together out of lockstep.
	if (Memory)
	{
		float Cooldown = DefaultAttackCooldownSeconds;
		if (const UGSRaceDataAsset* Race = Self->GetRaceData())
		{
			if (const FGSArchetypeDefinition* Archetype = Race->FindArchetype(Self->GetArchetypeRowName()))
			{
				if (Archetype->AttackCooldownSeconds > 0.f)
				{
					Cooldown = Archetype->AttackCooldownSeconds;
				}
			}
		}
		Memory->NextAllowedAttackTime = Now + Cooldown + FMath::FRandRange(0.f, AttackCooldownJitter);
	}

	return EBTNodeResult::Succeeded;
}
