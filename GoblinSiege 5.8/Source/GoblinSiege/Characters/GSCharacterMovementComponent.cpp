// Copyright Goblin Siege.

#include "Characters/GSCharacterMovementComponent.h"

UGSCharacterMovementComponent::UGSCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// PATH FOLLOWING MUST DRIVE MOVEMENT THROUGH ACCELERATION, NOT RAW VELOCITY (#415, 2026-09-10).
	//
	// `bUseAccelerationForPaths` defaults to FALSE in the engine (`NavigationTypes.h:430-432`), and
	// that default decides which of two entirely different code paths a path-following AI takes:
	//
	//   false -> UPathFollowingComponent::FollowPathSegment calls RequestDirectMove
	//            (`PathFollowingComponent.cpp:1159`), which sets only `RequestedVelocity` /
	//            `bHasRequestedVelocity` (`CharacterMovementComponent.cpp:4038`). The requested
	//            acceleration inside CalcVelocity is a LOCAL (`:3877`) added straight onto Velocity
	//            (`:3946-3951`) and never stored, so the `Acceleration` MEMBER stays zero forever.
	//   true  -> RequestPathMove (`:1149`) routes through `AddMovementInput`
	//            (`PawnMovementComponent.cpp:87-93`), and `ControlledCharacterMove` assigns the
	//            member at `CharacterMovementComponent.cpp:6451`.
	//
	// With it false, `GetCurrentAcceleration()` is identically 0 for every AI in the game while they
	// walk at full speed. That is invisible to a speed-driven blendspace like `ABP_Human`, and fatal
	// to ACF: `UACFAnimInstance::UpdateAcceleration` does `bIsAccelerating = Acceleration > 0`
	// (`ACFAnimInstance.cpp:383`), and `ACF_BaseMoveset` gates locomotion on `bIsAccelerating`,
	// `LocalAccel2D`, `AccelerationDirection` and `PivotStartingAcceleration`. So an ACF character
	// idles and turns in place correctly (yaw-driven) and NEVER STARTS WALKING, at any speed.
	//
	// ACF's own sample ticks this box on `ACF_Enemy_BP`'s component instance rather than in
	// `UACFCharacterMovementComponent`'s constructor, so swapping in this subclass
	// (`GSCharacterBase.cpp:54`) silently dropped it and we inherited the engine default instead.
	//
	// Measured in PIE with a control, 2026-09-10: two `BP_CastleGuard01_C` instances, same class,
	// same 280.1 uu/s, five consecutive samples - flag ON reports |accel| 4131.5, flag OFF reports
	// 0.0. Note the engine reads this ONLY on the path-following route, so a player-controlled pawn
	// is unaffected.
	//
	// KNOWN CONSEQUENCE, deliberate: `ShouldStopMovementOnPathFinished` returns false in
	// acceleration mode (`PathFollowingComponent.cpp:623-626`), so agents brake over
	// `CachedBrakingDistance` instead of stopping dead on arrival. If a behaviour-tree task turns
	// out to assume an instantaneous stop at its acceptance radius, tune it with
	// `bUseFixedBrakingDistanceForPaths` + `FixedPathBrakingDistance` rather than reverting this.
	NavMovementProperties.bUseAccelerationForPaths = true;
}

void UGSCharacterMovementComponent::BeginPlay()
{
	if (!bDisarmLocomotionStates)
	{
		// ACF's locomotion state machine stays ARMED for this character. The LocomotionStates bands
		// authored on this Blueprint are the speed table, and AACFAIController::UpdateLocomotionState
		// - called from SetCurrentAIState on every transition - picks the band from the AI state, so
		// AIState.Patrol can walk while AIState.Combat runs. Nothing here may touch MaxWalkSpeed:
		// owning it is the whole point of leaving the machine on.
		//
		// A Blueprint that clears this flag MUST author its bands. UpdateMaxSpeed's miss branch is
		// log-only (see below), so an empty table leaves MaxWalkSpeed wherever it happened to be and
		// the state changes silently do nothing.
		Super::BeginPlay();
		return;
	}

	// Hold the speed the archetype actually asked for. ACF is about to overwrite it.
	const float AuthoredWalkSpeed = MaxWalkSpeed;

	// EMPTY THE BANDS. This is what disarms the state machine, and it is done by emptying rather
	// than by overriding UpdateLocomotion because that function is NOT virtual (nor are
	// HandleStateChanged or Internal_ApplyLocomotionState) - only BeginPlay and TickComponent are.
	//
	// With no states, UpdateLocomotion's `for (i = 0; i < LocomotionStates.Num() - 1; i++)` never
	// runs, so no transition ever fires and nothing rewrites MaxWalkSpeed behind our back.
	LocomotionStates.Empty();

	Super::BeginPlay();

	// Belt and braces. This restore was documented as load-bearing - "with the bands gone
	// Super::BeginPlay sets MaxWalkSpeed to ZERO" - and that was WRONG; the claim was corrected in
	// AGENT_STATE.md on 2026-08-27 and the comment asserting it is deleted here. Re-verified against
	// ACFCharacterMovementComponent.cpp:684-691 for #334: when GetCharacterMaxSpeedByState misses,
	// the else branch only logs "Locomotion State inexistent" and writes nothing at all. So this
	// line is a no-op today. It stays because it is free and it pins the invariant that leaving
	// BeginPlay with anything other than the authored speed is a bug.
	MaxWalkSpeed = AuthoredWalkSpeed;
}
