// Copyright Goblin Siege.

#include "Characters/GSCharacterMovementComponent.h"

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
