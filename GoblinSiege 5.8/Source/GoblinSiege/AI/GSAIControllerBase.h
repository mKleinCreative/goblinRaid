// Shared AI controller for every defender archetype and goblin AI companion. Picks its Behavior
// Tree from the possessed pawn's archetype data (defenders) or a fixed companion BT (goblins),
// runs a Blackboard, and hosts AI Perception so archers/scouts/telegraphs all share one sensing
// setup. Per-archetype *behavior differences* (Elf dodge-on-telegraph, Engineer mine-laying,
// firefighting) are Behavior Tree assets + BT task nodes (see AI/Tasks/), not controller subclasses.
#pragma once

#include "CoreMinimal.h"
// ACF migration Phase 1 (2026-08-20, #214). Was "AIController.h" / AAIController.
//
// AACFAIController brings its own BehaviorTree, Blackboard, Commands, Targeting, CombatBehaviour and
// ThreatManager components, and - easy to miss - replaces the path-following component with a
// UCrowdFollowingComponent in its constructor.
//
// CORRECTED 2026-08-27 (#339). This comment used to end: "Its OnPossess early-returns on a
// non-AACFCharacter pawn, which ours are until Phase 2, so ACF's blackboard init and StartTree do
// not run yet. OUR OnPossess overrides call RunBehaviorTree() themselves after Super, which is the
// only reason the trees still start - that is load-bearing, not redundant."
//
// ALL OF THAT IS FALSE SINCE PHASE 2A and it cost a session (#330). Our pawns derive from
// AACFCharacter now, so the cast SUCCEEDS and ACF's OnPossess runs its blackboard init and
// StartTree in full. A RunBehaviorTree() call after Super is therefore NOT load-bearing - it is
// actively harmful: ACF never assigns BrainComponent (ACFAIController.cpp:53), so the engine
// allocates a SECOND UBehaviorTreeComponent and re-initialises the blackboard, invalidating every
// key ID ACF cached. Two trees on two blackboards drove one movement component.
//
// The archetype RunBehaviorTree below survives only because DA_Race_Human's Militia archetype has
// its BehaviorTree CLEARED, so the branch never fires for guards. Assigning a tree there again
// brings the two-tree bug straight back. See AGENT_STATE.md BUILT 2026-08-21.
#include "ACFAIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "GSAIControllerBase.generated.h"

class UBehaviorTreeComponent;
class UBlackboardComponent;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UGSAISteeringComponent;

UCLASS()
class GOBLINSIEGE_API AGSAIControllerBase : public AACFAIController
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
	 *
	 * KNOWN NOT TO WORK IN 5.8, observed 2026-08-12: the engine logs "Ignored
	 * DoNotCreateDefaultSubobject for AIPerceptionComponent as it's marked as required. Creating
	 * AIPerceptionComponent." eleven times in a single PIE session, so the horde is carrying the
	 * sight sense this opt-out exists to refuse. Left in place and recorded rather than removed -
	 * the intent is still correct and the fix belongs with whoever measures the cost.
	 */
	AGSAIControllerBase(const FObjectInitializer& ObjectInitializer);

	// ---- IACFEntityInterface, the half ACF 4.4.2 does not ship -----------------------------
	//
	// AACFBaseAIController declares `public IACFEntityInterface` and defines only two of its four
	// methods (GetEntityCombatTeam, AssignTeamToEntity). These two are never overridden there, and
	// AscentCoreInterfaces' ACFEntityInterface.cpp is empty apart from a "add default functionality
	// here" comment - so nothing in the plugin defines them. ACF links anyway; WE cannot, because
	// deriving from that class makes UHT emit our own interface thunks into our module, which then
	// have nothing to bind to (LNK2001 on both symbols, 2026-08-20, #215).
	//
	// Implemented properly rather than stubbed: ACF's targeting and motion warp both read the
	// extent radius, and aliveness decides whether this entity is a legal target at all.
	virtual bool IsEntityAlive_Implementation() const override;
	virtual float GetEntityExtentRadius_Implementation() const override;

	/**
	 * Has this AI had time to REACT to Target yet? (#238)
	 *
	 * Michael, watching a fight: "there's no buffer with the AI decisions, they twitch, because they
	 * do decisions right away instead of having to take some time. It doesn't look natural."
	 *
	 * He was right and it was structural: target SWITCHING was already damped
	 * (TargetSwitchHysteresis, ReacquireIntervalSeconds) and turning already interpolates, but
	 * nothing anywhere put a delay between a decision becoming true and the AI acting on it. A
	 * goblin that noticed you swung in the same frame.
	 *
	 * Lazy on purpose - the first call for a given target stamps the clock, later calls report
	 * whether the beat has elapsed. No tick, no bookkeeping in the behaviour tree, and an AI that
	 * never looks at a target never pays for one.
	 *
	 * Gates SWINGING, not moving or turning: the beat should read as "it sees you and gathers
	 * itself", not as a freeze.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|AI")
	bool HasReactedTo(AActor* Target);

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/** True when GS.Combat.FaceTarget is on, i.e. when the steering component owns yaw for engaged
	 *  agents.
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
	 *  being able to accept - the agent stands at 130uu with zero velocity and never swings, forever.
	 *
	 *  KEPT ON THIS CLASS AS A FORWARDER (#143). The implementation moved to UGSAISteeringComponent,
	 *  but three BT nodes already call it here, and one symbol forwarding to one source of truth beats
	 *  editing three call sites to say the same thing. */
	static bool IsFacingAuthorityEnabled();

protected:
	// ---- the two steering behaviours moved out (#143) ------------------------------------------
	//
	// The facing authority (#133) and the separation steer (#132) BOTH LIVE ON
	// UGSAISteeringComponent now. Everything that was declared here - bFaceTargetEnabled, TickFacing,
	// ApplyCombatRotationMode / RestoreDefaultRotationMode, the five captured-rotation bitfields,
	// FocusedTarget, the six separation dials, SeparationNeighbours, NextSeparationScanTime and
	// TickSeparation - moved there wholesale with its reasoning intact. Read that header; it is not
	// summarised here, because a summary is how two descriptions of one behaviour drift apart.
	//
	// WHY: ACF migration Phase 1a. Phase 1 reparents this class onto AACFAIController, and these are
	// signed-off, watched behaviours (Michael, 2026-08-11: "do not re-open") that have no business
	// being inside a class whose base is changing. Moving them first makes the reparent unable to
	// touch them.
	//
	// The component also ticks ITSELF, which closes the #135 hole permanently: both behaviours used to
	// ride AGSAIControllerBase::Tick, and AGSHordeAIController's constructor set
	// PrimaryActorTick.bCanEverTick = false, so neither ever ran on a summoned goblin for the entire
	// life of #132 and #133. A UActorComponent registers its own tick function and does not consult
	// the owning actor's flag.

	/** Created in the constructor for every AI controller, defender and horde alike. */
	/** The target HasReactedTo is currently timing, and when it was first seen. Transient: a
	 *  reaction is a moment, not a saved property. */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> ReactionTarget;

	float ReactionStartedTime = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|AI")
	TObjectPtr<UGSAISteeringComponent> SteeringComponent;

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
