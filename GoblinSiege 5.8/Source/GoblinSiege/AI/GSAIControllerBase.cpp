#include "AI/GSAIControllerBase.h"
#include "Characters/GSCharacterBase.h"
#include "Characters/GSEnemyCharacter.h"
#include "Combat/GSEngagementComponent.h"
#include "Combat/GSRaceDataAsset.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"

// A/B the separation steer inside ONE PIE session, exactly as GS.Combat.PersonalSpace does for the
// #131 floor. Every crowding ticket from #105 to #110 closed by comparing today's fight against a
// memory of yesterday's, and four of them reported success while the crowd still looked wrong.
static int32 GSSeparation = 1;
static FAutoConsoleVariableRef CVarGSSeparation(
	TEXT("GS.Combat.Separation"),
	GSSeparation,
	TEXT("1 = AI agents steer out of each other's capsules. 0 = pre-#132 behaviour (nothing does)."),
	ECVF_Cheat);

// The facing authority (#133), on the same A/B pattern. 0 restores the pre-#133 behaviour EXACTLY:
// the controller stops touching focus and rotation mode, and the three BT nodes go back to turning
// the pawn themselves. That is what makes "the goblins look at their enemy now" checkable in one PIE
// session instead of against a memory of yesterday's fight.
static int32 GSFaceTarget = 1;
static FAutoConsoleVariableRef CVarGSFaceTarget(
	TEXT("GS.Combat.FaceTarget"),
	GSFaceTarget,
	TEXT("1 = the AI controller owns combat facing (SetFocus + interpolated control rotation). "
	     "0 = pre-#133 behaviour (the BT nodes each set actor rotation and fight FaceRotation)."),
	ECVF_Cheat);

bool AGSAIControllerBase::IsFacingAuthorityEnabled()
{
	return GSFaceTarget > 0;
}

AGSAIControllerBase::AGSAIControllerBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The controller did not tick before this, and AGSHordeAIController's header argues against it in
	// as many words - "the blackboard refresh is a timer, not Tick", because ten goblins asking the
	// same question every frame is the cost that design exists to avoid. That objection is about a
	// per-agent SEARCH, and TickSeparation is written to not be one: the world query is a broadphase
	// overlap at 4Hz, and the per-frame work is a loop over at most four cached pointers behind three
	// early-outs (switch off, pawn dead, nobody nearby). An agent standing alone in a field costs a
	// bool and an array length.
	//
	// It ticks here rather than on the pawn because this is the only class every AI combatant shares
	// - defenders and the horde both - and because putting it on AGSCharacterBase would have made the
	// player tick for a steer he must never receive.
	PrimaryActorTick.bCanEverTick = true;

	// Zeroed explicitly: these are bitfields, and bRotationModeCaptured false is what forces the first
	// TickFacing to READ the pawn's real values before it changes any of them.
	bCapturedOrientToMovement = 0;
	bCapturedUseControllerYaw = 0;
	bCapturedDesiredRotation = 0;
	bRotationModeCaptured = 0;
	bCombatRotationApplied = 0;

	// Null whenever a subclass passed DoNotCreateDefaultSubobject for this name - see the header.
	// Everything below is guarded on that, including the sense config, which is pointless without
	// a component to host it.
	AIPerceptionComponent = ObjectInitializer.CreateDefaultSubobject<UAIPerceptionComponent>(
		this, TEXT("AIPerceptionComponent"));

	if (AIPerceptionComponent)
	{
		SightConfig = ObjectInitializer.CreateDefaultSubobject<UAISenseConfig_Sight>(this, TEXT("SightConfig"));
		SightConfig->SightRadius = SightRadius;
		SightConfig->LoseSightRadius = LoseSightRadius;
		SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngleDegrees;
		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

		// Note this has never actually worked as written: affiliation is resolved through
		// IGenericTeamAgentInterface, which nothing in this project implements, so every actor reads
		// as neutral and bDetectFriendlies=false gates nothing. Left as-is deliberately - the
		// friend/foe rule this game actually enforces is AGSCharacterBase::IsHostileTo on RaceTag,
		// and adding a team interface now would give two competing sources of truth.
		SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

		AIPerceptionComponent->ConfigureSense(*SightConfig);
		AIPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
		SetPerceptionComponent(*AIPerceptionComponent);
	}
}

void AGSAIControllerBase::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (const AGSEnemyCharacter* Enemy = Cast<AGSEnemyCharacter>(InPawn))
	{
		if (const UGSRaceDataAsset* RaceData = Enemy->GetRaceData())
		{
			if (const FGSArchetypeDefinition* Archetype = RaceData->FindArchetype(Enemy->GetArchetypeRowName()))
			{
				if (UBehaviorTree* BT = Archetype->BehaviorTree.LoadSynchronous())
				{
					RunBehaviorTree(BT);
					if (UBlackboardComponent* BB = GetBlackboardComponent())
					{
						BB->SetValueAsObject(TEXT("SelfArchetypeOwner"), InPawn);
					}
				}
			}
		}
	}

	// The archetype lookup above is a DEFENDER path - it needs AGSEnemyCharacter's race data, which
	// a horde goblin does not have. Allied goblins run a single fixed companion BT instead, and
	// since #069 that is owned by AGSHordeAIController::CompanionBehaviorTree rather than by a
	// Blueprint subclass of this class. Nothing here needs to know about them.
}

void AGSAIControllerBase::OnUnPossess()
{
	// Drop the cached neighbours with the pawn. Keeping them would have the next possession start by
	// steering away from whoever the LAST pawn was standing next to, for up to one scan interval.
	SeparationNeighbours.Reset();
	NextSeparationScanTime = 0.f;

	// Hand the pawn back in the rotation mode it arrived in. A pawn returned to the horde pool still
	// carrying bUseControllerDesiredRotation would come back out of the pool turning like an engaged
	// combatant while idle, and nothing downstream would explain why.
	if (AGSCharacterBase* Self = Cast<AGSCharacterBase>(GetPawn()))
	{
		RestoreDefaultRotationMode(Self);
	}
	ClearFocus(EAIFocusPriority::Gameplay);
	FocusedTarget.Reset();
	bRotationModeCaptured = false;

	Super::OnUnPossess();
}

void AGSAIControllerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickFacing();
	TickSeparation();
}

void AGSAIControllerBase::TickFacing()
{
	AGSCharacterBase* Self = Cast<AGSCharacterBase>(GetPawn());
	if (!IsValid(Self))
	{
		return;
	}

	// Capture BEFORE the first modification, and only once. Done here rather than in OnPossess because
	// OnPossess runs before a Blueprint-spawned pawn has necessarily finished applying its CDO, and
	// these three values are exactly the ones this ticket got wrong by assuming instead of reading.
	if (!bRotationModeCaptured)
	{
		if (const UCharacterMovementComponent* MoveComp = Self->GetCharacterMovement())
		{
			bCapturedOrientToMovement = MoveComp->bOrientRotationToMovement ? 1 : 0;
			bCapturedDesiredRotation = MoveComp->bUseControllerDesiredRotation ? 1 : 0;
			bCapturedUseControllerYaw = Self->bUseControllerRotationYaw ? 1 : 0;
			bRotationModeCaptured = true;
		}
		else
		{
			return;
		}
	}

	if (!bFaceTargetEnabled || GSFaceTarget <= 0)
	{
		// Switched off mid-session: give the pawn back exactly what it had, once, so the A/B compares
		// against real pre-#133 behaviour rather than against a half-restored hybrid.
		if (bCombatRotationApplied)
		{
			RestoreDefaultRotationMode(Self);
			ClearFocus(EAIFocusPriority::Gameplay);
			FocusedTarget.Reset();
		}
		return;
	}

	// Same key, same read as TickSeparation and every node in the combat tree. Taking it off the
	// blackboard rather than tracking it here is what stops the facing authority ever disagreeing
	// with the thing the tree believes it is fighting.
	AActor* Target = nullptr;
	if (const UBlackboardComponent* BB = GetBlackboardComponent())
	{
		Target = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
	}

	// A corpse is not something to stare at, and neither is a dead agent's own last enemy. Same
	// liveness rule the engagement ledger and the separation scan already apply (#106).
	const AGSCharacterBase* TargetChar = Cast<AGSCharacterBase>(Target);
	const bool bEngaged = IsValid(Target) && Self->IsAlive()
		&& (!TargetChar || TargetChar->IsAlive());

	if (!bEngaged)
	{
		if (bCombatRotationApplied)
		{
			RestoreDefaultRotationMode(Self);
			ClearFocus(EAIFocusPriority::Gameplay);
			FocusedTarget.Reset();
		}
		return;
	}

	// EAIFocusPriority::Gameplay, matching UBTTask_RangedAttack rather than out-ranking it.
	//
	// Move priority would have been the tidier-looking choice, and it is the wrong one: path following
	// sets the MOVE focus to its own goal (AAIController::SetMoveFocus), so a combat focus parked
	// there would be overwritten by every MoveTo - which is the bug being fixed, reintroduced one
	// layer down. Gameplay is above Move, so this survives pathing.
	//
	// The ticket's open question was whether this fights the archer. It does not: BTTask_RangedAttack
	// focuses the same blackboard TargetActor at the same priority, so both are asking for the same
	// actor and whichever writes last writes the same value. The archer additionally wants PITCH
	// tracked for its arc, which SetFocus gives it and this does not take away.
	if (FocusedTarget.Get() != Target)
	{
		SetFocus(Target, EAIFocusPriority::Gameplay);
		FocusedTarget = Target;
	}

	ApplyCombatRotationMode(Self);
}

void AGSAIControllerBase::ApplyCombatRotationMode(AGSCharacterBase* Self)
{
	if (bCombatRotationApplied)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Self ? Self->GetCharacterMovement() : nullptr;
	if (!MoveComp)
	{
		return;
	}

	// bUseControllerRotationYaw OFF and bUseControllerDesiredRotation ON is the whole visual change.
	//
	// Both make the pawn follow the control rotation; the difference is that the first ASSIGNS it in
	// APawn::FaceRotation (an instant snap, no rate, nothing to tune) while the second INTERPOLATES
	// toward it in UCharacterMovementComponent::PhysicsRotation at RotationRate. Since RotationRate is
	// already written from AGSCharacterBase::SetTurnRateRadPerSec, the per-archetype turn dial that
	// #109 and #124 both tried to enforce from inside a BT node starts being enforced by the one
	// system that actually owns yaw.
	//
	// bOrientRotationToMovement is forced off as well. It is already false on both defender CDOs, but
	// it is TRUE on the player and on anything that inherits the C++ default, and it would otherwise
	// fight bUseControllerDesiredRotation for the same yaw - two authorities again.
	MoveComp->bOrientRotationToMovement = false;
	MoveComp->bUseControllerDesiredRotation = true;
	Self->bUseControllerRotationYaw = false;

	bCombatRotationApplied = true;
}

void AGSAIControllerBase::RestoreDefaultRotationMode(AGSCharacterBase* Self)
{
	if (!bCombatRotationApplied || !bRotationModeCaptured)
	{
		return;
	}

	if (UCharacterMovementComponent* MoveComp = Self ? Self->GetCharacterMovement() : nullptr)
	{
		MoveComp->bOrientRotationToMovement = bCapturedOrientToMovement != 0;
		MoveComp->bUseControllerDesiredRotation = bCapturedDesiredRotation != 0;
		Self->bUseControllerRotationYaw = bCapturedUseControllerYaw != 0;
	}

	bCombatRotationApplied = false;
}

void AGSAIControllerBase::TickSeparation()
{
	if (!bSeparationEnabled || GSSeparation <= 0)
	{
		return;
	}

	AGSCharacterBase* Self = Cast<AGSCharacterBase>(GetPawn());
	if (!IsValid(Self) || !Self->IsAlive())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();

	// The agent's own victim is EXEMPT. See the header: distance to your target belongs to the ring
	// and to the personal-space floor, and it is the one distance that has to stay inside AttackRange
	// or nobody swings. Read straight off the blackboard rather than tracked here, so this can never
	// disagree with the key every other node in the combat tree is reading.
	const AActor* Victim = nullptr;
	if (const UBlackboardComponent* BB = GetBlackboardComponent())
	{
		Victim = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
	}

	const FVector SelfLoc = Self->GetActorLocation();

	// ---- the scan, throttled, and through the broadphase -----------------------------------------
	//
	// A SPHERE OVERLAP, NOT TActorIterator, and that is a deliberate answer to a documented objection.
	// AGSHordeAIController's header says outright: "The blackboard refresh is a timer, not Tick. Ten
	// goblins ticking to ask the same subsystem the same question is the cost this design exists to
	// avoid" - and GDD 3.4 wants the horde perception-less, where "the forbidden cost is a per-agent
	// SEARCH" (UBTService_AcquireTarget's note). Enabling Tick on the base controller puts that cost
	// back on every summoned goblin, so it had better not be a search over the world.
	//
	// It is not. TActorIterator walks the entire level actor array; this asks the physics broadphase
	// for pawns inside 260uu, which is spatially indexed and returns a handful. It runs at 4Hz rather
	// than per frame, and the per-frame half below is a loop over at most four cached pointers.
	//
	// Throttling the SEARCH does not make the STEERING coarse: the push uses these neighbours' live
	// positions every frame, and a neighbour cannot cross 260uu of scan margin inside 0.25s.
	if (Now >= NextSeparationScanTime)
	{
		// Jittered, so a warband that spawned on one frame does not rescan on one frame forever.
		NextSeparationScanTime = Now + SeparationScanIntervalSeconds
			+ FMath::FRandRange(0.f, SeparationScanIntervalSeconds * 0.25f);

		SeparationNeighbours.Reset();

		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
		FCollisionQueryParams QueryParams(TEXT("GSSeparationScan"), /*bTraceComplex=*/ false, Self);

		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, SelfLoc, FQuat::Identity, ObjectParams,
			FCollisionShape::MakeSphere(SeparationScanRadius), QueryParams);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AGSCharacterBase* Other = Cast<AGSCharacterBase>(Overlap.GetActor());
			if (!IsValid(Other) || Other == Self || Other == Victim)
			{
				continue;
			}
			// A corpse is not something to walk around: CorpseLifespan is 0 ("never destroy"), so
			// bodies would otherwise accumulate as permanent obstacles pushing the living out of the
			// fight. Same rule the engagement ledger applies everywhere (#106).
			if (!Other->IsAlive())
			{
				continue;
			}
			// AddUnique because an overlap returns one result per COMPONENT - a character answers with
			// its capsule and can answer again with its mesh, and a neighbour counted twice would be
			// pushed against twice as hard as its neighbour standing the same distance away.
			SeparationNeighbours.AddUnique(TWeakObjectPtr<AActor>(Other));
		}
	}

	if (SeparationNeighbours.Num() == 0)
	{
		return;
	}

	// ---- the push, every frame -------------------------------------------------------------------
	// Summed over neighbours rather than taken from the nearest one. An agent wedged between two
	// others has to be pushed out of the gap, not away from whichever of them happens to be a
	// centimetre closer - picking one is how a body oscillates between two neighbours instead of
	// leaving.
	FVector Push = FVector::ZeroVector;
	int32 Considered = 0;

	for (const TWeakObjectPtr<AActor>& Weak : SeparationNeighbours)
	{
		if (Considered >= MaxSeparationNeighbours)
		{
			break;
		}

		const AActor* Other = Weak.Get();
		if (!IsValid(Other))
		{
			continue;
		}

		FVector Away = SelfLoc - Other->GetActorLocation();
		Away.Z = 0.f;
		const float Distance = Away.Size();
		if (Distance < KINDA_SMALL_NUMBER)
		{
			// Exactly co-located. Any direction is as good as any other and none of them is derivable
			// from the positions, so use this agent's own facing - two stacked agents pick different
			// directions and separate, where a fixed world axis would push them both the same way and
			// they would travel as a stack.
			Away = Self->GetActorForwardVector();
			Away.Z = 0.f;
			Push += Away.GetSafeNormal();
			++Considered;
			continue;
		}

		// PER-PAIR, from both capsules. The whole point of #131's GetMinSeparation: a constant would
		// be wrong for every pair except the one it was tuned on, and this project has 52uu goblins
		// standing next to 70uu guards.
		const float Floor = UGSEngagementComponent::GetMinSeparation(Self, Other, SeparationMargin);
		if (Distance >= Floor)
		{
			continue;
		}

		// Linear in depth, normalised by the floor itself so the gain does not depend on how big the
		// two bodies happen to be. Full strength only at total overlap, which is where it belongs -
		// the common case is a shallow intrusion that wants a nudge, not a shove.
		const float Gain = FMath::Clamp((Floor - Distance) / Floor, 0.f, 1.f);
		Push += (Away / Distance) * Gain;
		++Considered;
	}

	if (Push.IsNearlyZero())
	{
		return;
	}

	// Magnitude carries the depth, direction carries where to go, and the scale is clamped so a knot
	// of four overlapping agents cannot ask for more than walk speed and fling itself apart.
	const float Scale = FMath::Min(Push.Size() * SeparationStrength, SeparationStrength);
	Self->AddMovementInput(Push.GetSafeNormal(), Scale);
}
