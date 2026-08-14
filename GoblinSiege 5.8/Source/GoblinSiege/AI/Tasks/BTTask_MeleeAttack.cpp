#include "AI/Tasks/BTTask_MeleeAttack.h"

#include "AIController.h"
#include "AI/GSAIControllerBase.h"
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

	// Dist2D, not Dist. A 355uu guard's actor origin sits ~57uu above a 240uu goblin's, and a 3D
	// distance spends that height out of the same 250uu budget: the real horizontal reach was
	// sqrt(250^2 - 57.5^2) = 243.3uu against a worst legal resting distance of 240. Three uu of
	// margin is why "they stand there and never swing" has been intermittent rather than theoretical,
	// and it gets worse the taller the attacker. Horizontally is the only way this range was ever
	// meant to be read - the sweep itself has a separate SweepHeightOffset for the vertical.
	//
	// The value stays 250. This only ever makes swinging MORE likely, which is what makes the
	// personal-space floor safe to ship alongside it.
	if (FVector::Dist2D(Target->GetActorLocation(), Self->GetActorLocation()) > AttackRange)
	{
		return EBTNodeResult::Failed;
	}

	// The victim's veto, checked here as well as in the decorator. The decorator reserved a token
	// possibly several ticks ago; between then and now the target may have started flinching or had
	// its guard broken. Swinging into either is the stunlock this whole system exists to prevent,
	// and holding a token is not a licence to ignore the state of the thing you are holding it
	// against.
	//
	// EXCEPT when the target is open from a blocked swing. CanBeAttacked() used to refuse on
	// State.Recoil unconditionally, which made THE PUNISH above unreachable: the two conditions are
	// exact opposites,
	// so every path that set bTargetIsOpen then failed here and the reward for reading an attack was
	// that the attacker became briefly un-hittable. Michael's ruling of 2026-08-08 (#087) is that a
	// blocked swing "opens the attacker up" - recoil is the opening, not a protection.
	//
	// Expressed as a parameter rather than by skipping the check, so Dead, HitReact and GuardBroken
	// still veto absolutely - a target killed or staggered by someone else DURING its recoil window
	// is not a free hit. And recoil stays a veto inside TryAcquireToken, so no NEW attacker can take
	// a token mid-window: the opening is punished by whoever was already engaged, not by a fresh
	// crowd. The anti-pile-on rule survives intact; only the punish becomes reachable.
	if (const UGSEngagementComponent* Engagement = Target->FindComponentByClass<UGSEngagementComponent>())
	{
		if (!Engagement->CanBeAttacked(/*bRecoilCountsAsOpening=*/ bTargetIsOpen))
		{
			if (GSAIDebug::IsLogging())
			{
				GSAIDebug::Log(Self, FString::Printf(TEXT("holding off - %s is staggered/open"),
					*GetNameSafe(Target)));
			}
			return EBTNodeResult::Failed;
		}
	}

	if (bTargetIsOpen && GSAIDebug::IsLogging())
	{
		GSAIDebug::Log(Self, FString::Printf(TEXT("PUNISH - %s is recoiling, swinging out of turn"),
			*GetNameSafe(Target)));
	}

	// THE CONTROLLER TURNS, THIS NODE ONLY JUDGES (#133). When GS.Combat.FaceTarget is on,
	// AGSAIControllerBase::TickFacing is holding this pawn on its target every frame - including
	// during the cooldown window, which is the hole the ticket found: the cooldown gate above returns
	// Failed long before the facing block below, so a defender spent every inter-swing beat with its
	// facing owned by nothing at all. Turning here as well would be the second authority #108 warns
	// about, so this branch checks and does not touch the pawn.
	if (bFaceTargetBeforeSwing && AGSAIControllerBase::IsFacingAuthorityEnabled())
	{
		const FRotator Look = UKismetMathLibrary::FindLookAtRotation(
			Self->GetActorLocation(), Target->GetActorLocation());
		const float OffBy = FMath::Abs(
			FMath::FindDeltaAngleDegrees(Self->GetActorRotation().Yaw, Look.Yaw));

		if (OffBy > FacingToleranceDegrees)
		{
			if (GSAIDebug::IsLogging())
			{
				GSAIDebug::Log(Self, FString::Printf(
					TEXT("not square on %s yet (%.0f deg off, tol %.0f) - controller is turning"),
					*GetNameSafe(Target), OffBy, FacingToleranceDegrees));
			}
			return EBTNodeResult::Failed;
		}
	}
	else if (bFaceTargetBeforeSwing)
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

		// RATE-LIMITED, not snapped. This used to apply MaxFacingSnapDegrees (120) in a single frame
		// with no time term at all, so a defender whipped up to 120 degrees instantaneously in the
		// frame before every swing - which is what reads as characters snap-rotating to face. The two
		// other places that turn a pawn, UBTTask_Block and UBTTask_MenaceOrbit, both step at
		// TurnRateRadPerSec * DeltaSeconds and look correct; this one was the odd one out.
		//
		// Measured against elapsed WALL CLOCK rather than frame delta, deliberately. ExecuteTask runs
		// when the tree re-activates this node, NOT once per frame, so scaling by DeltaSeconds would
		// tie the turn speed to how often the tree happens to come back around - and if that is
		// slower than the frame rate the agent turns in slow motion and may never face its target,
		// which is the deadlock the comment below was written about. Elapsed time gives the same
		// degrees-per-second whatever the activation rate.
		//
		// MaxFacingSnapDegrees survives as an absolute ceiling per activation: after a long gap the
		// elapsed term would otherwise permit an arbitrarily large step, which is the snap again.
		// Now/World are already resolved at the top of ExecuteTask - reuse them rather than asking
		// the pawn for its world a second time.
		const float SinceLastStep = (Memory && Memory->LastFacingStepTime > 0.f)
			? FMath::Clamp(Now - Memory->LastFacingStepTime, 0.f, 0.25f)
			: 0.f;
		if (Memory)
		{
			Memory->LastFacingStepTime = Now;
		}

		const float TurnRateDegPerSec = FMath::RadiansToDegrees(
			FMath::Max(Self->GetTurnRateRadPerSec(), 0.01f));
		// A first activation has no elapsed time to work with. One frame at 60fps is the smallest
		// honest step; without it the very first tick would turn by zero and the node would fail
		// forever against a target standing behind it.
		const float StepBudgetDeg = FMath::Min(
			TurnRateDegPerSec * FMath::Max(SinceLastStep, 1.f / 60.f), MaxFacingSnapDegrees);

		const float AppliedYaw = FMath::Clamp(NeededYaw, -StepBudgetDeg, StepBudgetDeg);
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
			// Dist2D for the same reason as the attack gate above - a height difference should not
			// eat the guard-break's reach budget. Value unchanged at 200.
			&& FVector::Dist2D(Target->GetActorLocation(), Self->GetActorLocation()) <= GuardBreakRange
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
