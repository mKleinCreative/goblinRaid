// The camera shake for a landed melee hit (#355) - defined in C++ so it needs no asset.
//
// A UCameraShakeBase with a UPerlinNoiseCameraShakePattern as its root pattern. Sub-classing
// rather than authoring a Blueprint means the swing can reference it by class with no
// ConstructorHelpers path into Content, and the whole shake is three numbers a designer can still
// override in a Blueprint child if the feel needs a hand-tuned curve later.
//
// Two scales exist, LIGHT and HEAVY, chosen by the swing from the same per-stage weight that drives
// hitstop (#353). They are separate classes rather than one class with a magnitude parameter because
// UCameraShakeBase plays by CLASS and stacks by class: a heavy landing during a light's shake must
// read as a bigger event, and two instances of one class merely overlap.
//
// CONSTRUCTION follows the engine's own ULegacyCameraShake exactly: the base class already creates a
// default subobject named "RootShakePattern", so a subclass swaps its CLASS through the
// FObjectInitializer rather than creating a second pattern. UCameraShakeBase has no default
// constructor - the first version of this file assumed one and did not compile.
#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "GSHitCameraShake.generated.h"

/** A light tap: short, small, mostly pitch. Sells "the blade connected" without moving the frame. */
UCLASS()
class GOBLINSIEGE_API UGSHitCameraShake_Light : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	UGSHitCameraShake_Light(const FObjectInitializer& ObjectInitializer);
};

/** A heavy: longer and larger, with roll. This one is allowed to move the frame. */
UCLASS()
class GOBLINSIEGE_API UGSHitCameraShake_Heavy : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	UGSHitCameraShake_Heavy(const FObjectInitializer& ObjectInitializer);
};
