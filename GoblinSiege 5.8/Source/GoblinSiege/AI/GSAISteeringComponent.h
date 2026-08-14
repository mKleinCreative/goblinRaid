// The facing authority (#133) and the separation steer (#132), lifted off AGSAIControllerBase.
// Written 2026-08-13, ticket #143, as ACF migration Phase 1a.
//
// ---- why this moved, and why now ---------------------------------------------------------------
// Phase 1 of the ACF migration reparents AGSAIControllerBase onto AACFAIController. Both of these
// behaviours are signed-off, watched work - Michael signed off the facing on 2026-08-11 with "do not
// re-open" - and both lived as private methods on the class whose base is about to change under them.
// Moving them into a component first is the "make the change easy, then make the easy change" split:
// after this, the reparent cannot touch them.
//
// ---- WHY THE COMPONENT TICKS ITSELF ------------------------------------------------------------
// This is the important part, and it is a direct answer to #135.
//
// #132 put the separation steer on AGSAIControllerBase::Tick and #133 put the facing authority there
// too. AGSHordeAIController's constructor had `PrimaryActorTick.bCanEverTick = false`, so BOTH
// features silently never executed on a single summoned goblin - for two whole tickets, each of which
// closed claiming the horde was covered. Nothing failed loudly, because defenders possess the base
// directly and ticked fine.
//
// A UActorComponent registers its own tick function. It does not read the owning actor's
// bCanEverTick, so no subclass constructor can turn this off by accident ever again. That property
// is the main reason this is a component rather than two protected methods on a shared base.
//
// ---- what did NOT change -----------------------------------------------------------------------
// The behaviour, the tuning values, the cvars, and every comment explaining why each number is what
// it is. This is a move, not a rewrite. `GS.Combat.Separation` and `GS.Combat.FaceTarget` still
// exist, still mean the same thing, and AGSAIControllerBase::IsFacingAuthorityEnabled() still answers
// for the BT nodes that consult it - it forwards here so there is exactly one source of truth.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSAISteeringComponent.generated.h"

class AGSCharacterBase;
class UBlackboardComponent;

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSAISteeringComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSAISteeringComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** True when GS.Combat.FaceTarget is on, i.e. when this component owns yaw for engaged agents.
	 *
	 *  The three BT nodes that would otherwise turn the pawn themselves (UBTTask_MenaceOrbit,
	 *  UBTTask_MeleeAttack, UBTTask_Block) consult this and stand down when it is true, so exactly one
	 *  thing decides facing - the #108 lesson about two position authorities, applied to rotation.
	 *  They reach it through AGSAIControllerBase::IsFacingAuthorityEnabled(), which forwards here. */
	static bool IsFacingAuthorityEnabled();

	/** Called from the owning controller's OnUnPossess, BEFORE Super. Hands the pawn back in the
	 *  rotation mode it arrived in and drops the cached neighbours.
	 *
	 *  A pawn returned to the horde pool still carrying bUseControllerDesiredRotation would come back
	 *  out turning like an engaged combatant while idle, and nothing downstream would explain why.
	 *  Keeping the neighbour list would have the next possession steer away from whoever the LAST
	 *  pawn stood next to, for up to one scan interval. */
	void HandleUnPossess();

protected:
	// ---- facing authority (#133) ---------------------------------------------------------------
	//
	// Michael, 2026-08-11: "Goblins are looking off to the side and not paying visual attention to
	// enemies right in front of them."
	//
	// What was actually wrong was NOT what the ticket first wrote down. Read off the CDOs,
	// BP_CastleGuard01 and BP_ErikaArcher ship bOrientRotationToMovement=FALSE and
	// bUseControllerRotationYaw=TRUE - the opposite of what BTTask_Block.h claimed. That inverts the
	// mechanism: bUseControllerRotationYaw makes APawn::FaceRotation assign the pawn's yaw from the
	// CONTROL rotation every frame, so the deliberate SetActorRotation in MenaceOrbit/MeleeAttack/Block
	// was overwritten in the same frame it ran - and since nothing outside UBTTask_RangedAttack ever
	// called SetFocus, the control rotation pointed wherever path following last left it.
	//
	// The fix: give the control rotation something correct to point at (SetFocus on the blackboard
	// target) and stop it being applied as an instant snap (bUseControllerDesiredRotation, which
	// INTERPOLATES at CharacterMovementComponent::RotationRate). RotationRate is already fed from
	// AGSCharacterBase::SetTurnRateRadPerSec, so a goblin still turns at a goblin's rate.

	/** Master switch. Mirrors GS.Combat.Separation and GS.Combat.PersonalSpace so all three of this
	 *  crowd's dials A/B the same way inside one PIE session. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Facing")
	bool bFaceTargetEnabled = true;

	// ---- separation steering (#132) ------------------------------------------------------------
	//
	// The gap #107 named in its own Evaluate: "at peak density (17 live agents) the worst pairwise
	// clearance across ALL agents is still ~0.2uu". Bodies inside each other, by the measurement of
	// the ticket that fixed the ring.
	//
	// It could not stay in the behaviour tree: #131 put its personal-space floor in
	// UBTTask_MenaceOrbit, the node an agent runs while it is NOT allowed to attack, and #132 raised
	// the token budget to 4 of 6 so two thirds of every gang now runs the attack branch instead.
	//
	// It is not the second position authority #108 killed. It never sets a goal or a destination - it
	// contributes an acceleration through AddMovementInput, which composes additively with path
	// following in CalcVelocity rather than competing with it.

	/** Master switch, so the crowd can be compared with and without it inside one PIE session rather
	 *  than against a memory of yesterday's fight. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Separation")
	bool bSeparationEnabled = true;

	/** Surface daylight this agent keeps from any OTHER agent, on top of both capsule radii.
	 *
	 *  Smaller than UBTTask_MenaceOrbit's PersonalSpaceMargin (50) on purpose: that is a resting
	 *  stand-off from the man you are fighting and wants to read as wariness; this is shoulder room in
	 *  a moving crowd, and a warband that will not pass within 50uu of itself cannot get through a
	 *  doorway. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Separation", meta = (ClampMin = "0.0"))
	float SeparationMargin = 25.f;

	/** Strength at full overlap, as a fraction of walk speed. Below the back-off's 0.8: this fires
	 *  constantly in a crowd and at full authority it would be a shoving match. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Separation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SeparationStrength = 0.45f;

	/** Neighbours further than this are not considered. Sized past the worst pair's floor
	 *  (138.8 + 25 = 163.8uu) with room to spare. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Separation", meta = (ClampMin = "0.0"))
	float SeparationScanRadius = 260.f;

	/** Seconds between rebuilds of the neighbour list. The PUSH is recomputed every frame from those
	 *  neighbours' live positions - only the O(actors) scan is throttled. Jittered per agent. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Separation", meta = (ClampMin = "0.05"))
	float SeparationScanIntervalSeconds = 0.25f;

	/** Most neighbours one agent will push against. Four is more than can physically surround a
	 *  capsule at contact. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Separation", meta = (ClampMin = "1"))
	int32 MaxSeparationNeighbours = 4;

private:
	void TickFacing();
	void TickSeparation();

	void ApplyCombatRotationMode(AGSCharacterBase* Self);
	void RestoreDefaultRotationMode(AGSCharacterBase* Self);

	/** The pawn this component's owning controller is driving, or null. */
	AGSCharacterBase* ResolvePawn() const;

	/**
	 * The blackboard both behaviours read TargetActor from.
	 *
	 * Resolved defensively rather than through AAIController::GetBlackboardComponent() alone, and
	 * that is deliberate ACF-migration groundwork: AACFAIController creates its OWN
	 * UBlackboardComponent named "BlackBoardComp" in its constructor, separate from the one
	 * AAIController::RunBehaviorTree() sets up. After Phase 1 there may be two, and a facing authority
	 * reading the empty one would look exactly like the bug this component exists to fix.
	 */
	UBlackboardComponent* ResolveBlackboard() const;

	/** Captured from the pawn on first tick rather than assumed, because these values come from the
	 *  Blueprint CDO and differ from the C++ constructor defaults - the exact mistake #133's own
	 *  diagnosis made. Restoring a guessed value would leave a disengaged defender in a rotation mode
	 *  it never shipped with. */
	uint8 bCapturedOrientToMovement : 1;
	uint8 bCapturedUseControllerYaw : 1;
	uint8 bCapturedDesiredRotation : 1;
	uint8 bRotationModeCaptured : 1;
	uint8 bCombatRotationApplied : 1;

	/** The actor currently focused, so the focus is only re-set when it actually changes. */
	TWeakObjectPtr<AActor> FocusedTarget;

	/** Rebuilt every SeparationScanIntervalSeconds; read every frame. Weak, so a neighbour dying
	 *  between scans drops out rather than being followed to its corpse's last known position. */
	TArray<TWeakObjectPtr<AActor>> SeparationNeighbours;

	float NextSeparationScanTime = 0.f;
};
