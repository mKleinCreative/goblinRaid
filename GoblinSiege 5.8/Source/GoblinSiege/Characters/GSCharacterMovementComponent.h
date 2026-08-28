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
 *
 * THE DISARM IS NOT GLOBAL ANY MORE (2026-08-27, #334). The reason above is a PLAYER problem - the
 * player sprints by writing MaxWalkSpeed, so ACF's bands have to stop fighting that write. AI never
 * sprints, and emptying its bands cost us a feature ACF ships: AACFAIController maps AI state to
 * locomotion state (LocomotionStateByAIState) and SetCurrentAIState calls UpdateLocomotionState on
 * every transition, so a guard can WALK on AIState.Patrol and RUN on AIState.Combat for free. With
 * the bands emptied that lookup resolves to nothing and MaxWalkSpeed stays pinned at whatever the
 * Blueprint authored - which is why every defender jogged everywhere at a flat 450uu/s.
 *
 * So the disarm is now opt-out per Blueprint. Leave bDisarmLocomotionStates TRUE for the player;
 * clear it on AI whose bands you have authored. Measured live in PIE on BP_CastleGuard01_C_1:
 * with bands authored, SetLocomotionState(EWalk) -> MaxWalkSpeed 280.1, (EJog) -> 606.3, which are
 * the ground speeds of the retargeted PowerfulSword walk and run clips.
 */
UCLASS()
class GOBLINSIEGE_API UGSCharacterMovementComponent : public UACFCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	/**
	 * Empty ACF's LocomotionStates in BeginPlay, disarming its state machine so nothing rewrites
	 * MaxWalkSpeed behind a direct speed write.
	 *
	 * TRUE (default) is the player's case - see the class comment. Set FALSE on AI that should use
	 * ACF's locomotion states instead; those Blueprints must author their own LocomotionStates
	 * bands, because an empty band table makes GetCharacterMaxSpeedByState find nothing.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin Siege|Locomotion")
	bool bDisarmLocomotionStates = true;
};
