// Copyright Goblin Siege.

#pragma once

#include "CoreMinimal.h"
#include "Components/ACFCharacterMovementComponent.h"
#include "GSCharacterMovementComponent.generated.h"

/**
 * ACF's movement component with its locomotion state machine switched off.
 *
 * WHY THIS EXISTS (2026-08-21, #223, Phase 2a). AACFCharacter's constructor swaps in a
 * UACFCharacterMovementComponent, and that component owns MaxWalkSpeed: BeginPlay applies
 * DefaultState (EJog) and UpdateLocomotion re-derives the band from velocity on every transition,
 * writing MaxWalkSpeed from its LocomotionStates table - which ships populated at
 * Idle 0 / Walk 250 / Jog 500 / Sprint 650.
 *
 * That makes SPRINT STRUCTURALLY IMPOSSIBLE for this project: entering the Sprint band needs
 * velocity above 505 while MaxWalkSpeed is pinned at 500. ACF expects sprint to be a STATE change
 * (SetLocomotionState(ESprint)); ours is a speed write, as are the block slow, the carry slow and
 * the per-swing MoveSpeedScale.
 *
 * THE OBVIOUS FIX DOES NOT WORK, AND FAILS QUIETLY. Passing
 * SetDefaultSubobjectClass<UCharacterMovementComponent> in our constructor compiles, links, and is
 * then REFUSED at runtime - "Class Engine.CharacterMovementComponent is not a legal override for
 * component CharMoveComp because it does not derive from ACFCharacterMovementComponent. Will use
 * ACFCharacterMovementComponent" - one error per character, and ACF's component is constructed
 * anyway. An override must DERIVE from what the parent chose. Hence this subclass.
 *
 * A DEFERRAL, NOT A REJECTION: ruling 27 says move speed becomes ACF locomotion states, and Phase 2b
 * does that with the ARS attribute migration so slow-composition is decided once. Deleting this
 * class is part of that work.
 */
UCLASS()
class GOBLINSIEGE_API UGSCharacterMovementComponent : public UACFCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
};
