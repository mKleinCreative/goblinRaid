#include "AI/GSAISteeringComponent.h"

#include "Characters/GSCharacterBase.h"
#include "Combat/GSEngagementComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

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
// this component stops touching focus and rotation mode, and the three BT nodes go back to turning
// the pawn themselves.
static int32 GSFaceTarget = 1;
static FAutoConsoleVariableRef CVarGSFaceTarget(
	TEXT("GS.Combat.FaceTarget"),
	GSFaceTarget,
	TEXT("1 = the AI steering component owns combat facing (SetFocus + interpolated control "
	     "rotation). 0 = pre-#133 behaviour (the BT nodes each set actor rotation)."),
	ECVF_Cheat);

bool UGSAISteeringComponent::IsFacingAuthorityEnabled()
{
	return GSFaceTarget > 0;
}

UGSAISteeringComponent::UGSAISteeringComponent()
{
	// SELF-TICKING, and this is the whole reason the behaviours live in a component. A UActorComponent
	// registers its own tick function and does not consult the owning actor's PrimaryActorTick, so no
	// subclass constructor can silently switch this off - which is precisely what
	// AGSHordeAIController did to both of these features for the entire life of #132 and #133 (see
	// #135, and the header).
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	bCapturedOrientToMovement = 0;
	bCapturedUseControllerYaw = 0;
	bCapturedDesiredRotation = 0;
	bRotationModeCaptured = 0;
	bCombatRotationApplied = 0;
}

AGSCharacterBase* UGSAISteeringComponent::ResolvePawn() const
{
	const AController* OwningController = Cast<AController>(GetOwner());
	return OwningController ? Cast<AGSCharacterBase>(OwningController->GetPawn()) : nullptr;
}

UBlackboardComponent* UGSAISteeringComponent::ResolveBlackboard() const
{
	AAIController* OwningController = Cast<AAIController>(GetOwner());
	if (!OwningController)
	{
		return nullptr;
	}

	// The one AAIController::RunBehaviorTree() set up, first.
	if (UBlackboardComponent* BB = OwningController->GetBlackboardComponent())
	{
		return BB;
	}

	// Fallback for the ACF hierarchy - see the header. AACFAIController owns a second
	// UBlackboardComponent ("BlackBoardComp") that AAIController knows nothing about.
	return OwningController->FindComponentByClass<UBlackboardComponent>();
}

void UGSAISteeringComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TickFacing();
	TickSeparation();
}

void UGSAISteeringComponent::HandleUnPossess()
{
	SeparationNeighbours.Reset();
	NextSeparationScanTime = 0.f;

	if (AGSCharacterBase* Self = ResolvePawn())
	{
		RestoreDefaultRotationMode(Self);
	}

	if (AAIController* OwningController = Cast<AAIController>(GetOwner()))
	{
		OwningController->ClearFocus(EAIFocusPriority::Gameplay);
	}
	FocusedTarget.Reset();
	bRotationModeCaptured = false;
}

// ---------------------------------------------------------------------------------------------
// Facing
// ---------------------------------------------------------------------------------------------

void UGSAISteeringComponent::TickFacing()
{
	AGSCharacterBase* Self = ResolvePawn();
	if (!IsValid(Self))
	{
		return;
	}

	AAIController* OwningController = Cast<AAIController>(GetOwner());
	if (!OwningController)
	{
		return;
	}

	// Capture BEFORE the first modification, and only once. Done on tick rather than on possession
	// because possession runs before a Blueprint-spawned pawn has necessarily finished applying its
	// CDO, and these three values are exactly the ones #133 got wrong by assuming instead of reading.
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
			OwningController->ClearFocus(EAIFocusPriority::Gameplay);
			FocusedTarget.Reset();
		}
		return;
	}

	// Same key, same read as TickSeparation and every node in the combat tree. Taking it off the
	// blackboard rather than tracking it here is what stops the facing authority ever disagreeing
	// with the thing the tree believes it is fighting.
	AActor* Target = nullptr;
	if (const UBlackboardComponent* BB = ResolveBlackboard())
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
			OwningController->ClearFocus(EAIFocusPriority::Gameplay);
			FocusedTarget.Reset();
		}
		return;
	}

	// EAIFocusPriority::Gameplay, matching UBTTask_RangedAttack rather than out-ranking it.
	//
	// Move priority would have been the tidier-looking choice, and it is the wrong one: path following
	// sets the MOVE focus to its own goal (AAIController::SetMoveFocus), so a combat focus parked
	// there would be overwritten by every MoveTo - the bug being fixed, reintroduced one layer down.
	// Gameplay is above Move, so this survives pathing.
	// ---- ASK THE CONTROLLER, DO NOT TRUST THE CACHE (#246) ---------------------------------------
	//
	// This used to read `FocusedTarget.Get() != Target`, i.e. "have I already focused this target?".
	// That is wrong because something else clears the focus behind this component's back:
	// UBTTask_RangedAttack::OnTaskFinished calls ClearFocus(Gameplay) on EVERY exit, deliberately and
	// correctly, but without telling anyone. FocusedTarget still points at the same actor, so the
	// cache says "already focused", and the focus is never re-applied for as long as the archer keeps
	// the same target.
	//
	// The visible result is that after her FIRST shot the archer's control rotation falls back to
	// path following's move focus, so she yaws toward wherever she is being sent instead of at the
	// person she is shooting. Reading the Gameplay slot itself is what makes the two sides agree
	// without either having to know about the other.
	if (OwningController->GetFocusActorForPriority(EAIFocusPriority::Gameplay) != Target)
	{
		OwningController->SetFocus(Target, EAIFocusPriority::Gameplay);
		FocusedTarget = Target;
	}

	ApplyCombatRotationMode(Self);
}

void UGSAISteeringComponent::ApplyCombatRotationMode(AGSCharacterBase* Self)
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
	// Both make the pawn follow the control rotation; the difference is that the first ASSIGNS it in
	// APawn::FaceRotation (an instant snap, no rate) while the second INTERPOLATES toward it in
	// UCharacterMovementComponent::PhysicsRotation at RotationRate - which is already written from
	// AGSCharacterBase::SetTurnRateRadPerSec, so the per-archetype turn dial starts being enforced by
	// the one system that actually owns yaw.
	//
	// bOrientRotationToMovement is forced off as well. It is already false on both defender CDOs, but
	// it is TRUE on the player and on anything inheriting the C++ default, and it would otherwise
	// fight bUseControllerDesiredRotation for the same yaw - two authorities again.
	MoveComp->bOrientRotationToMovement = false;
	MoveComp->bUseControllerDesiredRotation = true;
	Self->bUseControllerRotationYaw = false;

	bCombatRotationApplied = true;
}

void UGSAISteeringComponent::RestoreDefaultRotationMode(AGSCharacterBase* Self)
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

// ---------------------------------------------------------------------------------------------
// Separation
// ---------------------------------------------------------------------------------------------

void UGSAISteeringComponent::TickSeparation()
{
	if (!bSeparationEnabled || GSSeparation <= 0)
	{
		return;
	}

	AGSCharacterBase* Self = ResolvePawn();
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

	// The agent's own victim is EXEMPT. Distance to your target belongs to the ring and to the
	// personal-space floor, and it is the one distance that has to stay inside AttackRange or nobody
	// swings. Read straight off the blackboard so this can never disagree with the key every other
	// node in the combat tree is reading.
	const AActor* Victim = nullptr;
	if (const UBlackboardComponent* BB = ResolveBlackboard())
	{
		Victim = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
	}

	const FVector SelfLoc = Self->GetActorLocation();

	// ---- the scan, throttled, and through the broadphase -------------------------------------
	//
	// A SPHERE OVERLAP, NOT TActorIterator, and that is a deliberate answer to a documented objection.
	// GDD 3.4 wants the horde perception-less, where "the forbidden cost is a per-agent SEARCH".
	// TActorIterator walks the entire level actor array; this asks the physics broadphase for pawns
	// inside 260uu, which is spatially indexed and returns a handful. It runs at 4Hz, and the
	// per-frame half below is a loop over at most four cached pointers.
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
			// pushed against twice as hard as one standing the same distance away.
			SeparationNeighbours.AddUnique(TWeakObjectPtr<AActor>(Other));
		}
	}

	if (SeparationNeighbours.Num() == 0)
	{
		return;
	}

	// ---- the push, every frame ---------------------------------------------------------------
	// Summed over neighbours rather than taken from the nearest one. An agent wedged between two
	// others has to be pushed out of the gap, not away from whichever happens to be a centimetre
	// closer - picking one is how a body oscillates between two neighbours instead of leaving.
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
			// Exactly co-located. Any direction is as good as any other and none is derivable from the
			// positions, so use this agent's own facing - two stacked agents pick different directions
			// and separate, where a fixed world axis would push them both the same way and they would
			// travel as a stack.
			Away = Self->GetActorForwardVector();
			Away.Z = 0.f;
			Push += Away.GetSafeNormal();
			++Considered;
			continue;
		}

		// PER-PAIR, from both capsules. The whole point of #131's GetMinSeparation: a constant would be
		// wrong for every pair except the one it was tuned on, and this project has 52uu goblins
		// standing next to 70uu guards.
		const float Floor = UGSEngagementComponent::GetMinSeparation(Self, Other, SeparationMargin);
		if (Distance >= Floor)
		{
			continue;
		}

		// Linear in depth, normalised by the floor itself so the gain does not depend on how big the
		// two bodies happen to be. Full strength only at total overlap, which is where it belongs.
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
