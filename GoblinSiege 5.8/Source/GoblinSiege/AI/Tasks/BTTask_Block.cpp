#include "AI/Tasks/BTTask_Block.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AI/GSAIControllerBase.h"
#include "AI/GSAIDebug.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Characters/GSCharacterBase.h"
#include "Combat/GSGameplayTags.h"
#include "Engine/World.h"

UBTTask_Block::UBTTask_Block()
{
	NodeName = TEXT("Block (StartBlocking)");

	// Latent: the guard is held across frames and must be dropped on abort, not just on completion.
	bNotifyTick = true;
	bNotifyTaskFinished = true;

	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_Block, TargetKey),
		AActor::StaticClass());
	TargetKey.SelectedKeyName = TEXT("TargetActor");

	TargetIsAttackingKey.AddBoolFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTTask_Block, TargetIsAttackingKey));
	TargetIsAttackingKey.SelectedKeyName = TEXT("TargetIsAttacking");
}

void UBTTask_Block::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		TargetKey.ResolveSelectedKey(*BBAsset);
		TargetIsAttackingKey.ResolveSelectedKey(*BBAsset);
	}
}

EBTNodeResult::Type UBTTask_Block::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();
	AGSCharacterBase* Self = Controller ? Cast<AGSCharacterBase>(Controller->GetPawn()) : nullptr;
	if (!BB || !Self)
	{
		return EBTNodeResult::Failed;
	}

	FGSBlockMemory* Memory = CastInstanceNodeMemory<FGSBlockMemory>(NodeMemory);
	const UWorld* World = OwnerComp.GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	// Every rejection below returns Failed rather than stalling, so the Selector falls straight
	// through to the attack and chase branches on the SAME tick - exactly how UBTTask_MeleeAttack
	// already treats its own cooldown. A node that returned InProgress while declining would make a
	// defender stand still deciding not to block, which is the worst of both behaviours.
	const AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!IsValid(Target))
	{
		return EBTNodeResult::Failed;
	}

	const float Dist = FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation());
	if (Dist > BlockRange)
	{
		return EBTNodeResult::Failed;
	}

	if (Memory && Now < Memory->NextAllowedBlockTime)
	{
		return EBTNodeResult::Failed;
	}

	// Do not raise a guard in the middle of your own swing. State.Attacking spans the whole ability
	// including recovery, which is precisely the window in which committing to a block would look
	// like the animation glitching rather than a decision.
	if (const UAbilitySystemComponent* SelfASC =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Self))
	{
		if (SelfASC->HasMatchingGameplayTag(GSTags::State_Attacking))
		{
			// Logged, because a silent decline here is indistinguishable from the node never running.
			// That ambiguity cost an hour on 2026-08-08: one combatant showed `telegraph SEEN` with
			// no roll beside it and looked like a second bug rather than a fighter mid-swing.
			if (GSAIDebug::IsLogging())
			{
				GSAIDebug::Log(Self, TEXT("block SKIPPED (already mid-swing)"));
			}
			return EBTNodeResult::Failed;
		}
	}

	const bool bTelegraphed = TargetIsAttackingKey.IsSet()
		&& BB->GetValueAsBool(TargetIsAttackingKey.SelectedKeyName);

	const float Chance = bTelegraphed ? TelegraphBlockChance : IdleBlockChance;
	if (FMath::FRand() >= Chance)
	{
		// Stamp a cooldown on the DECLINE too, or the tree re-rolls this several times inside one
		// windup and the effective chance compounds to near-certainty. See DeclineCooldownSeconds.
		if (Memory)
		{
			Memory->NextAllowedBlockTime = Now + DeclineCooldownSeconds;
		}
		if (GSAIDebug::IsLogging())
		{
			GSAIDebug::Log(Self, FString::Printf(TEXT("block DECLINED (%s, p=%.2f)"),
				bTelegraphed ? TEXT("telegraph") : TEXT("idle"), Chance));
		}
		return EBTNodeResult::Failed;
	}

	// A refusal here is meaningful, not an error: UGSGA_Block blocks on State.GuardBroken and
	// State.Carrying, so "the guard was just kicked open" arrives as a false return. That is the
	// guard break doing its job and the correct response is to fall through and fight instead.
	if (!Self->StartBlocking())
	{
		if (Memory)
		{
			Memory->NextAllowedBlockTime = Now + DeclineCooldownSeconds;
		}
		if (GSAIDebug::IsLogging())
		{
			GSAIDebug::Log(Self, TEXT("block REFUSED by ability (guard broken, carrying or unset)"));
		}
		return EBTNodeResult::Failed;
	}

	if (Memory)
	{
		Memory->BlockUntilTime = Now + BlockHoldSeconds + FMath::FRandRange(0.f, BlockHoldJitter);
	}

	if (GSAIDebug::IsLogging())
	{
		GSAIDebug::Log(Self, FString::Printf(TEXT("block ACCEPTED (%s, p=%.2f) vs %s"),
			bTelegraphed ? TEXT("telegraph") : TEXT("idle"), Chance, *GetNameSafe(Target)));
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_Block::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	const UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();
	AGSCharacterBase* Self = Controller ? Cast<AGSCharacterBase>(Controller->GetPawn()) : nullptr;
	if (!BB || !Self)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	FGSBlockMemory* Memory = CastInstanceNodeMemory<FGSBlockMemory>(NodeMemory);
	const UWorld* World = OwnerComp.GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	const AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!IsValid(Target))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	// The guard break landed. Holding the task open would keep re-facing a target while the
	// character is staggered and cannot actually block, which reads as an invulnerable turtle.
	if (const UAbilitySystemComponent* SelfASC =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Self))
	{
		if (SelfASC->HasMatchingGameplayTag(GSTags::State_GuardBroken))
		{
			if (GSAIDebug::IsLogging())
			{
				GSAIDebug::Log(Self, TEXT("guard BROKEN - dropping block"));
			}
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
			return;
		}
	}

	// The block landed and turned their swing aside - stop guarding and go and hit them. Holding the
	// shield up through the punish window would waste the entire reward for having read the attack,
	// and the whole point of the recoil is that a blocked swing costs the attacker a beat.
	if (const AGSCharacterBase* TargetCharacter = Cast<AGSCharacterBase>(Target))
	{
		if (TargetCharacter->IsRecoiling())
		{
			if (GSAIDebug::IsLogging())
			{
				GSAIDebug::Log(Self, FString::Printf(TEXT("block LANDED - %s is open, dropping guard"),
					*GetNameSafe(Target)));
			}
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
			return;
		}
	}

	if (FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation()) > BlockBreakRange)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	// Skipped entirely when the controller owns yaw (#133): AGSAIControllerBase::TickFacing is already
	// holding this pawn on its target every frame, including while it stands still to block, which is
	// the case the header below says nothing else covers. Retained behind the switch so
	// GS.Combat.FaceTarget 0 reproduces pre-#133 blocking exactly.
	if (bFaceTargetWhileBlocking && !AGSAIControllerBase::IsFacingAuthorityEnabled())
	{
		// Turn at the character's own rate rather than snapping. A snap would guarantee the frontal
		// arc but also make a blocker track a circling attacker perfectly, which removes flanking
		// from the game entirely - and flanking is the only answer an allied goblin has to a guard,
		// since goblins carry no guard break.
		FVector ToTarget = Target->GetActorLocation() - Self->GetActorLocation();
		ToTarget.Z = 0.f;
		if (!ToTarget.IsNearlyZero())
		{
			const float DesiredYaw = ToTarget.Rotation().Yaw;
			const float CurrentYaw = Self->GetActorRotation().Yaw;
			// FindDeltaAngleDegrees already returns the shortest signed turn in [-180, 180], so a plain
			// Clamp is correct here. FMath::ClampAngle would re-normalise and is the wrong tool.
			const float MaxStepDeg = FMath::RadiansToDegrees(Self->GetTurnRateRadPerSec()) * DeltaSeconds;
			const float DeltaYaw = FMath::Clamp(
				FMath::FindDeltaAngleDegrees(CurrentYaw, DesiredYaw), -MaxStepDeg, MaxStepDeg);

			Self->SetActorRotation(FRotator(0.f, CurrentYaw + DeltaYaw, 0.f));
		}
	}

	if (Memory && Now >= Memory->BlockUntilTime)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

void UBTTask_Block::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory,
	EBTNodeResult::Type TaskResult)
{
	// The single release point for the guard, and the reason this node is latent. Runs on abort as
	// well as on completion, so a higher-priority branch stealing control cannot strand a raised
	// shield on a character for the rest of the raid.
	if (const AAIController* Controller = OwnerComp.GetAIOwner())
	{
		if (AGSCharacterBase* Self = Cast<AGSCharacterBase>(Controller->GetPawn()))
		{
			Self->StopBlocking();

			if (FGSBlockMemory* Memory = CastInstanceNodeMemory<FGSBlockMemory>(NodeMemory))
			{
				const UWorld* World = OwnerComp.GetWorld();
				const float Now = World ? World->GetTimeSeconds() : 0.f;
				Memory->NextAllowedBlockTime = Now + BlockCooldownSeconds
					+ FMath::FRandRange(0.f, BlockCooldownJitter);
			}
		}
	}

	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}
