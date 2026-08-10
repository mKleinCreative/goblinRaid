#include "AI/Tasks/BTTask_MeleeAttack.h"

#include "AIController.h"
#include "AI/GSAIDebug.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Combat/GSEngagementComponent.h"
#include "Characters/GSCharacterBase.h"
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

	// THE PUNISH. A target left open by having its swing blocked is worth breaking cadence for -
	// an opening nobody exploits is not an opening, and the whole reason to block is that it earns
	// this. Read off the TARGET rather than off self, so the rule is "everyone punishes an opening"
	// rather than something each side has to remember about itself.
	//
	// Deliberately checked before the cooldown gate and not folded into it: the cooldown still
	// stamps normally afterwards, so this buys one swing out of turn, not a free rate of fire.
	bool bTargetIsOpen = false;
	if (const AGSCharacterBase* TargetCharacter =
			Cast<AGSCharacterBase>(BB->GetValueAsObject(TargetKey.SelectedKeyName)))
	{
		bTargetIsOpen = TargetCharacter->IsRecoiling();
	}

	if (!bTargetIsOpen && Memory && Now < Memory->NextAllowedAttackTime)
	{
		// Still on cooldown. Failing lets the Selector fall through to the chase branch, which keeps
		// the defender repositioning between swings instead of standing frozen mid-cooldown.
		return EBTNodeResult::Failed;
	}

	// AGSCharacterBase, not AGSEnemyCharacter, since #069: the combat verbs live on the base now so
	// an allied horde goblin can swing with the same task. Narrowing this to the defender class was
	// the only thing that would have stopped BT_HordeGoblin reusing this node verbatim.
	AGSCharacterBase* Self = Cast<AGSCharacterBase>(Controller->GetPawn());
	AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!Self || !IsValid(Target))
	{
		return EBTNodeResult::Failed;
	}

	if (FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation()) > AttackRange)
	{
		return EBTNodeResult::Failed;
	}

	// The victim's veto, checked here as well as in the decorator. The decorator reserved a token
	// possibly several ticks ago; between then and now the target may have started flinching,
	// had its guard broken, or been left recoiling by a blocked swing. Swinging into any of those
	// is the stunlock this whole system exists to prevent, and holding a token is not a licence to
	// ignore the state of the thing you are holding it against.
	if (const UGSEngagementComponent* Engagement = Target->FindComponentByClass<UGSEngagementComponent>())
	{
		if (!Engagement->CanBeAttacked())
		{
			if (GSAIDebug::IsLogging())
			{
				GSAIDebug::Log(Self, FString::Printf(TEXT("holding off - %s is staggered/open"),
					*GetNameSafe(Target)));
			}
			return EBTNodeResult::Failed;
		}
	}

	if (bFaceTargetBeforeSwing)
	{
		const FRotator Look = UKismetMathLibrary::FindLookAtRotation(
			Self->GetActorLocation(), Target->GetActorLocation());

		// Capped, where this used to be an unconditional snap. An uncapped correction turns every
		// defender into a turret that cannot be flanked and an attack that cannot be made to miss.
		//
		// TURN, then fail - do NOT just refuse. Refusing was a deadlock: an agent already standing on
		// its stand-off slot fails the facing test, falls through to the chase branch, MoveTo reports
		// success instantly because it is already there, the tree restarts, and it fails the facing
		// test again forever. Nothing else in the tree rotates a stationary pawn
		// (bOrientRotationToMovement needs velocity), so it stood next to its target at 130uu with
		// zero velocity and never swung. Observed on summoned goblins, 2026-08-09.
		//
		// Clamping the correction instead keeps everything the cap was for - flanking still costs the
		// attacker real time, and a swing still cannot be whipped round 180 degrees - while
		// guaranteeing that a badly-facing agent comes round within a tick or two and recovers.
		const float CurrentYaw = Self->GetActorRotation().Yaw;
		const float NeededYaw = FMath::FindDeltaAngleDegrees(CurrentYaw, Look.Yaw);
		const float AppliedYaw = FMath::Clamp(NeededYaw, -MaxFacingSnapDegrees, MaxFacingSnapDegrees);
		Self->SetActorRotation(FRotator(0.f, CurrentYaw + AppliedYaw, 0.f));

		if (!FMath::IsNearlyEqual(AppliedYaw, NeededYaw))
		{
			// Turned as far as the cap allows, but still not facing the target. Swinging now would
			// whiff into empty air; the next tick starts from the new facing and will connect.
			if (GSAIDebug::IsLogging())
			{
				GSAIDebug::Log(Self, FString::Printf(
					TEXT("turning to face %s (%.0f deg to go)"), *GetNameSafe(Target), NeededYaw - AppliedYaw));
			}
			return EBTNodeResult::Failed;
		}
	}

	// ---- guard break -----------------------------------------------------------------------
	// Answered BEFORE the light attack, because swinging into a raised guard is the thing this
	// branch exists to stop. CanGuardBreak() is a null check on the ability class, so allied goblins
	// (which deliberately have none) skip the whole branch at zero cost - see the header.
	if (Self->CanGuardBreak() && Memory && Now >= Memory->NextAllowedGuardBreakTime)
	{
		const AGSCharacterBase* TargetChar = Cast<AGSCharacterBase>(Target);
		const bool bTargetTurtling = TargetChar && TargetChar->IsBlocking();

		if (bTargetTurtling
			&& FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation()) <= GuardBreakRange
			&& FMath::FRand() < GuardBreakChance
			&& Self->TryGuardBreak())
		{
			// Both clocks, not just the guard-break one: the kick IS this agent's action for the
			// beat. Stamping only its own cooldown would let a defender kick and immediately swing.
			Memory->NextAllowedGuardBreakTime = Now + GuardBreakCooldownSeconds;
			Memory->NextAllowedAttackTime = Now + DefaultAttackCooldownSeconds
				+ FMath::FRandRange(0.f, AttackCooldownJitter);

			if (GSAIDebug::IsLogging())
			{
				GSAIDebug::Log(Self, FString::Printf(TEXT("guard break FIRED at %s (target blocking)"),
					*GetNameSafe(Target)));
			}
			return EBTNodeResult::Succeeded;
		}
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
		// The archetype row is a defender concept - only AGSEnemyCharacter carries RaceData and a row
		// name. Anything else (a horde goblin) simply takes DefaultAttackCooldownSeconds, which is
		// why this is a soft cast and not a requirement.
		float Cooldown = DefaultAttackCooldownSeconds;
		if (const AGSEnemyCharacter* Defender = Cast<AGSEnemyCharacter>(Self))
		{
			if (const UGSRaceDataAsset* Race = Defender->GetRaceData())
			{
				if (const FGSArchetypeDefinition* Archetype = Race->FindArchetype(Defender->GetArchetypeRowName()))
				{
					if (Archetype->AttackCooldownSeconds > 0.f)
					{
						Cooldown = Archetype->AttackCooldownSeconds;
					}
				}
			}
		}
		Memory->NextAllowedAttackTime = Now + Cooldown + FMath::FRandRange(0.f, AttackCooldownJitter);
	}

	return EBTNodeResult::Succeeded;
}
