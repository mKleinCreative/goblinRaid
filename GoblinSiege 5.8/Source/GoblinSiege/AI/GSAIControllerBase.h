// Shared AI controller for every defender archetype and goblin AI companion. Picks its Behavior
// Tree from the possessed pawn's archetype data (defenders) or a fixed companion BT (goblins),
// runs a Blackboard, and hosts AI Perception so archers/scouts/telegraphs all share one sensing
// setup. Per-archetype *behavior differences* (Elf dodge-on-telegraph, Engineer mine-laying,
// firefighting) are Behavior Tree assets + BT task nodes (see AI/Tasks/), not controller subclasses.
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "GSAIControllerBase.generated.h"

class UBehaviorTreeComponent;
class UBlackboardComponent;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;

UCLASS()
class GOBLINSIEGE_API AGSAIControllerBase : public AAIController
{
	GENERATED_BODY()

public:
	/**
	 * Takes an FObjectInitializer so a subclass can decline the perception component entirely:
	 *
	 *     AGSHordeAIController::AGSHordeAIController(const FObjectInitializer& OI)
	 *         : Super(OI.DoNotCreateDefaultSubobject(TEXT("AIPerceptionComponent")))
	 *
	 * Added 2026-08-07 (#069). Until then this constructor built a UAIPerceptionComponent and a
	 * 1200uu sight sense unconditionally, and AGSHordeAIController inherited both - so ten summoned
	 * goblins meant ten independent sight queries a frame, all feeding an empty handler (deleted in
	 * #115). GDD §3.4 requires the exact opposite for the horde: "a crowd of
	 * perception-less agents fed stimuli by a central subsystem, which is what makes ten concurrent
	 * goblins cheap." Defenders keep their senses; the horde reads UGSHordeSubsystem instead.
	 */
	AGSAIControllerBase(const FObjectInitializer& ObjectInitializer);

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;

	/** True when GS.Combat.FaceTarget is on, i.e. when this controller owns yaw for engaged agents.
	 *
	 *  The three BT nodes that used to turn the pawn themselves (UBTTask_MenaceOrbit,
	 *  UBTTask_MeleeAttack, UBTTask_Block) consult this and stand down when it is true, so exactly one
	 *  thing decides facing - the #108 lesson about two position authorities, applied to rotation.
	 *
	 *  They KEEP their old turn code behind the false branch rather than deleting it, deliberately.
	 *  #131 and #132 both shipped their change behind a switch so the crowd could be compared with and
	 *  against it inside one PIE session, and AGENT_STATE records three consecutive fixes that were
	 *  wrong because nobody could do that. Deleting the old path would make GS.Combat.FaceTarget 0 a
	 *  DEADLOCK rather than a comparison: UBTTask_MeleeAttack refuses to swing until it is facing its
	 *  target, and #089 records what happens when a node that can refuse stops making progress toward
	 *  being able to accept - the agent stands at 130uu with zero velocity and never swings, forever. */
	static bool IsFacingAuthorityEnabled();

protected:
	// ---- facing authority (#133) -------------------------------------------------------------------
	//
	// Michael, 2026-08-11: "Goblins are looking off to the side and not paying visual attention to
	// enemies right in front of them."
	//
	// ---- what was actually wrong, which is NOT what the ticket first wrote down ---------------------
	// The ticket (quoting BTTask_Block.h:121) said these pawns run bOrientRotationToMovement with
	// bUseControllerRotationYaw false, so a defender walking somewhere faces its path. Read off the
	// CDOs, both of those are the wrong way round: BP_CastleGuard01 and BP_ErikaArcher ship
	// bOrientRotationToMovement=FALSE and bUseControllerRotationYaw=TRUE.
	//
	// That inverts the mechanism. bUseControllerRotationYaw makes APawn::FaceRotation assign the
	// pawn's yaw from the CONTROL rotation every single frame, so the deliberate SetActorRotation in
	// MenaceOrbit/MeleeAttack/Block was being overwritten in the same frame it ran - and since nothing
	// in this project ever called SetFocus outside UBTTask_RangedAttack, the control rotation pointed
	// wherever path following last left it. A defender therefore looked along its last path while
	// fighting something in front of it, which is exactly the complaint.
	//
	// It is also very likely the real cause behind #109 ("MenaceOrbit sets facing instantly, up to 167
	// degrees in one frame") and #124 ("the melee facing snap turns 120 degrees in one frame"). Both
	// were fixed by rate-limiting the deliberate turn, which cannot help when the thing undoing that
	// turn is FaceRotation slamming the yaw to an un-interpolated control rotation. This is offered as
	// the likely explanation, not a proven one - it is falsifiable by watching a fight with the switch
	// on and off, which is what the switch is for.
	//
	// ---- the fix -----------------------------------------------------------------------------------
	// Give the control rotation something correct to point at (SetFocus on the blackboard target) and
	// stop it being applied as an instant snap (bUseControllerDesiredRotation, which INTERPOLATES at
	// CharacterMovementComponent::RotationRate instead of assigning). RotationRate is already fed from
	// AGSCharacterBase::SetTurnRateRadPerSec, so a goblin still turns at a goblin's rate and the
	// per-archetype dial keeps working - nothing new invents a turn speed.
	//
	// ---- why the controller, and why not a BT service ----------------------------------------------
	// Same argument as the separation steer above: it is the one place that ticks for every AI
	// combatant whatever branch the tree is in, it covers the horde, and it needs no BT asset edited.
	// Facing is also genuinely controller business - SetFocus and the control rotation live here.

	/** Master switch. Mirrors GS.Combat.Separation and GS.Combat.PersonalSpace so all three of this
	 *  crowd's dials A/B the same way inside one PIE session. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Facing")
	bool bFaceTargetEnabled = true;

private:
	/** Points the control rotation at the blackboard target and lets the movement component
	 *  interpolate the body round to it; releases both when there is no target. */
	void TickFacing();

	/** Applied while engaged, restored on disengage and on unpossess. */
	void ApplyCombatRotationMode(class AGSCharacterBase* Self);
	void RestoreDefaultRotationMode(class AGSCharacterBase* Self);

	/** Captured from the pawn on possession rather than assumed, because these values come from the
	 *  Blueprint CDO and differ from the C++ constructor defaults - the exact mistake the ticket's own
	 *  diagnosis made. Restoring a guessed value would leave a disengaged defender in a rotation mode
	 *  it never shipped with. */
	uint8 bCapturedOrientToMovement : 1;
	uint8 bCapturedUseControllerYaw : 1;
	uint8 bCapturedDesiredRotation : 1;
	uint8 bRotationModeCaptured : 1;
	uint8 bCombatRotationApplied : 1;

	/** The actor this controller currently holds a focus on, so the focus is only re-set when it
	 *  actually changes rather than every frame. */
	TWeakObjectPtr<AActor> FocusedTarget;

protected:
	// ---- separation steering (#132) --------------------------------------------------------------
	//
	// The gap #107 named in its own Evaluate and left open: "at peak density (17 live agents) the
	// worst pairwise clearance across ALL agents is still ~0.2uu. That number counts bystanders
	// engaged with different victims passing each other, which this ticket's ring geometry does not
	// govern." Bodies inside each other, by the measurement of the ticket that fixed the ring.
	//
	// ---- why it could not stay in the behaviour tree ----------------------------------------------
	// #131 put its personal-space floor in UBTTask_MenaceOrbit, which is the node an agent runs while
	// it is NOT allowed to attack. That covered most of a crowd when the token budget was 2 of 3
	// assigned. #132 raises it to 4 of 6, so two thirds of every gang now holds a token, runs the
	// attack branch, and never executes a line of that floor. Left alone, the change Michael asked
	// for to fix the standstill would have made the crowding he asked about in the same breath
	// strictly worse.
	//
	// ---- why the controller ----------------------------------------------------------------------
	// It is the one place that ticks for every AI combatant whatever branch the tree is in, and it
	// covers the horde too (AGSHordeAIController derives from this). The alternative - a BT service
	// on the combat branch - would need every BT asset edited to add it, and would still be per-tree
	// rather than per-agent. Steering on the controller and decisions in the tree is also the
	// conventional split.
	//
	// ---- why this is not the second position authority #108 killed --------------------------------
	// It never sets a goal or a destination. It contributes an acceleration through AddMovementInput,
	// which composes additively with path following in CalcVelocity rather than competing with it -
	// the agent still walks to the station its tree chose, just not through its neighbour on the way.
	// The jitter in #108 was two nodes writing DIFFERENT destinations for the same agent; nothing
	// here writes one at all.
	//
	// ---- and why it deliberately ignores the agent's own victim -----------------------------------
	// Distance to your target is owned by the ring and by the personal-space floor, and it is the one
	// distance that has to stay inside AttackRange or nobody swings. A separation term pushing on it
	// too would be a third opinion on the only number in this system that is already crowded with
	// them. This handles allies, bystanders and ring-mates; the victim is somebody else's job.

	/** Master switch, so the crowd can be compared with and without it inside one PIE session rather
	 *  than against a memory of yesterday's fight - the failure mode AGENT_STATE records three times
	 *  in a row. Mirrors GS.Combat.PersonalSpace, which does the same for the #131 floor. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Separation")
	bool bSeparationEnabled = true;

	/** Surface daylight this agent tries to keep from any OTHER agent, on top of both capsule radii.
	 *
	 *  Smaller than UBTTask_MenaceOrbit's PersonalSpaceMargin (50) on purpose. That one is a resting
	 *  stand-off from the man you are fighting and wants to read as wariness; this is shoulder room
	 *  in a moving crowd, and a warband that will not pass within 50uu of itself cannot get through a
	 *  doorway. 25uu is about a hand's width of daylight between two capsules. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Separation", meta = (ClampMin = "0.0"))
	float SeparationMargin = 25.f;

	/** Strength at full overlap, as a fraction of walk speed. Below the back-off's 0.8: this fires
	 *  constantly in a crowd and at full authority it would be a shoving match, where the back-off is
	 *  an occasional deliberate step. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Separation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SeparationStrength = 0.45f;

	/** Neighbours further than this are not considered. Sized past the worst pair's floor
	 *  (138.8 + 25 = 163.8uu) with room to spare, so nothing that could matter is culled. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Separation", meta = (ClampMin = "0.0"))
	float SeparationScanRadius = 260.f;

	/** Seconds between rebuilds of the neighbour list. The PUSH is recomputed every frame from those
	 *  neighbours' live positions - only the O(actors) scan is throttled, so the steering stays smooth
	 *  while the search cost stays off the frame. Jittered per agent at first use. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Separation", meta = (ClampMin = "0.05"))
	float SeparationScanIntervalSeconds = 0.25f;

	/** Most neighbours one agent will push against. Four is more than can physically surround a
	 *  capsule at contact; a cap past that only spends time on agents whose contribution is already
	 *  summed into the same direction. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Separation", meta = (ClampMin = "1"))
	int32 MaxSeparationNeighbours = 4;

private:
	/** Rebuilt every SeparationScanIntervalSeconds; read every frame. Weak, so a neighbour dying
	 *  between scans drops out rather than being followed to its corpse's last known position. */
	TArray<TWeakObjectPtr<AActor>> SeparationNeighbours;

	float NextSeparationScanTime = 0.f;

	/** Adds an outward acceleration when this agent's capsule is inside a neighbour's floor.
	 *
	 *  Takes no delta: the steer is an ACCELERATION handed to the movement component, which does its
	 *  own integration. Scaling it by DeltaSeconds here would make the push frame-rate dependent in
	 *  the one direction that matters - weaker the slower the frame, i.e. weakest in exactly the
	 *  17-agent pile-up it exists for. */
	void TickSeparation();

protected:
	// The perception component below has no C++ consumer: the empty HandlePerceptionUpdated handler
	// that used to listen to it was deleted in #115, since it only ever documented that target
	// selection happens elsewhere. The component itself stays until BT_Militia has been checked
	// in-editor for a stock node or EQS query that reads it (see #115's Refine).
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|AI")
	TObjectPtr<UAIPerceptionComponent> AIPerceptionComponent;

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|AI")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	/** Blackboard keys every archetype's BT relies on, so BT assets can be swapped per-archetype
	 *  without redefining these each time: "TargetActor" (Object), "IsFleeing" (Bool, Landlord
	 *  mission), "IsFirefighting" (Bool), "HomeBuilding" (Object, for guard/return-to-post AI). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|AI")
	float SightRadius = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|AI")
	float LoseSightRadius = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|AI")
	float PeripheralVisionAngleDegrees = 90.f;
};
