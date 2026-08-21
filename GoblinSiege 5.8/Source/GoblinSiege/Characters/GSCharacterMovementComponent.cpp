// Copyright Goblin Siege.

#include "Characters/GSCharacterMovementComponent.h"

void UGSCharacterMovementComponent::BeginPlay()
{
	// Hold the speed the archetype actually asked for. ACF is about to overwrite it.
	const float AuthoredWalkSpeed = MaxWalkSpeed;

	// EMPTY THE BANDS. This is what disarms the state machine, and it is done by emptying rather
	// than by overriding UpdateLocomotion because that function is NOT virtual (nor are
	// HandleStateChanged or Internal_ApplyLocomotionState) - only BeginPlay and TickComponent are.
	//
	// With no states, UpdateLocomotion's `for (i = 0; i < LocomotionStates.Num() - 1; i++)` never
	// runs, so no transition ever fires and nothing rewrites MaxWalkSpeed behind our back.
	LocomotionStates.Empty();

	// Super still applies DefaultState, and with the bands gone GetCharacterMaxSpeedByState finds
	// nothing and returns 0.0f - so this call sets MaxWalkSpeed to ZERO. That is why the restore
	// below is not optional: without it every character in the game stands still.
	Super::BeginPlay();

	MaxWalkSpeed = AuthoredWalkSpeed;
}
