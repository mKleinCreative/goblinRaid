#include "World/GSWaterVolume.h"

AGSWaterVolume::AGSWaterVolume()
{
	// The whole mechanism. UCharacterMovementComponent::PhysicsVolumeChanged reads this and switches
	// the character to MOVE_Swimming; nothing else is required for swimming to work.
	//
	// Note the character ALSO needs NavAgentProps.bCanSwim - without it the movement component
	// refuses the mode change and the pawn falls through the volume as if it were empty air, which
	// looks exactly like this flag not being set. Both halves are needed; see
	// AGSPlayerCharacter's constructor.
	bWaterVolume = true;

	// Terminal velocity in water. Left at the engine default rather than tuned, because it only
	// governs how fast you sink when not swimming, and nothing has been felt yet.
	FluidFriction = 0.3f;
}
