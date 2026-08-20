// The order wheel: latch a target, drag to pick a verb, release to commit. Written 2026-08-12,
// ticket #141. The on-screen half is UGSHordeOrderWheelWidget; the consequences are
// UGSHordeSubsystem::IssueOrder.
//
// THIS IS A DELIBERATE CLONE of UGSWeaponComponent's wheel block (GSWeaponComponent.cpp:271-347),
// down to the dead-zone fallback and the screen-space Y negation. The alternative was hoisting a
// shared FGSRadialWheel out of both, and that was rejected: the weapon wheel is three 120-degree
// sectors over an enum the component also OWNS and validates, this is four 90-degree sectors over an
// enum it merely reports, and the two differ again on what "release with no drag" means. A shared
// base would be two behaviours in a trench coat, and it would put a refactor of shipped, watched
// input code inside a ticket about a new feature.
//
// WHY THIS LIVES ON THE PAWN and not on UGSHordeSubsystem, which owns everything else about the
// horde. Three reasons, heaviest first:
//   1. A UWorldSubsystem has no NetRole and cannot host a UFUNCTION(Server, ...). The committed order
//      has to reach the server from a client-owned actor, and the pawn is that actor.
//   2. The drag accumulator and the latch are per-PLAYER input state; the subsystem is per-WORLD.
//      Putting them there breaks the moment there are two players - which is exactly the retrofit
//      UGSHordeSubsystem::ActiveGoblins' controller keying exists to avoid (GSHordeSubsystem.h:213).
//   3. It makes UGSHordeOrderWheelWidget a FindComponentByClass copy of UGSWeaponWheelWidget rather
//      than a second, different way for a widget to find its state.
//
// NOTHING HERE REPLICATES except the one RPC. Open/drag/highlight/latch are local, exactly like
// UGSWeaponComponent's wheel state. Only the committed result crosses the wire.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Horde/GSHordeOrderTypes.h"
#include "GSHordeCommandComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnOrderWheelOpenChanged, bool, bOpen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnOrderWheelHighlightChanged, EGSHordeOrder, Highlighted);

/** The crosshair moved on or off something orderable. Drives the reticle's colour (#149). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSOnCrosshairTargetChanged, bool, bHasTarget, AActor*, Target);

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSHordeCommandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSHordeCommandComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// ---------------------------------------------------------------------------- the wheel

	/**
	 * Latch the camera trace, THEN open.
	 *
	 * Returns false and does NOT open when the latch found nowhere to send anybody - pointing at the
	 * sky over a void. A wheel that cannot commit is worse than no wheel: it eats the mouse, hides the
	 * camera, and then does nothing on release, which reads as the whole feature being broken.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Horde|Wheel")
	bool OpenOrderWheel();

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Horde|Wheel")
	void AddWheelInput(FVector2D Delta);

	/** bCommit false aborts the gesture outright - for the caller that knows it was interrupted
	 *  (pawn died, wheel force-closed) rather than dragged back to centre. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Horde|Wheel")
	void CloseOrderWheel(bool bCommit = true);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde|Wheel")
	bool IsWheelOpen() const { return bWheelOpen; }

	/** Played on the owning character the moment an order actually commits - the goblin barks the
	 *  command with a gesture instead of the horde silently changing behaviour. Cosmetic only:
	 *  nothing about the order depends on it, and a null montage just means no gesture.
	 *
	 *  Deliberately fired in CloseOrderWheel, not in ServerIssueOrder: the gesture belongs to the
	 *  player who made it and should be seen locally the instant they release, not after a round
	 *  trip. It is skipped when the release lands in the dead zone, so an aborted wheel gesture
	 *  does not animate. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Wheel")
	TObjectPtr<class UAnimMontage> OrderIssuedMontage;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Wheel")
	float OrderIssuedMontagePlayRate = 1.f;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde|Wheel")
	EGSHordeOrder GetWheelHighlight() const { return WheelHighlight; }

	/** Raw accumulated drag, for a widget that wants to draw the stick. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde|Wheel")
	FVector2D GetWheelVector() const { return WheelAccum; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde|Wheel")
	bool IsWheelCommitted() const { return WheelAccum.Size() >= WheelDeadZone; }

	/**
	 * Direction to verb. Four 90-degree sectors: Attack up, Hold right, Follow down, Loot left.
	 *
	 * BlueprintPure and public so the widget draws EXACTLY what the input will do. Follow sits 180
	 * degrees from Attack on purpose - a mis-drag can turn a charge into a recall or the reverse, and
	 * those are the two orders you least want to confuse, so they are put as far apart as the geometry
	 * allows.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde|Wheel")
	EGSHordeOrder OrderForDirection(FVector2D Direction, EGSHordeOrder Fallback) const;

	// ---------------------------------------------------------------------------- the latch

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde|Wheel")
	FVector GetLatchedLocation() const { return LatchedLocation; }

	/** Non-const return on a UFUNCTION for the reason GSHordeSubsystem.h:139-142 sets out: UHT
	 *  rejects a `const T*` parameter or return outright, and finding that out costs a full build. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde|Wheel")
	AActor* GetLatchedSubject() const { return LatchedSubject.Get(); }

	/** "CastleGuard01", "LootSack", or "the ground". Drawn on the wheel - without it the latch is
	 *  invisible, and the player has no way to tell what he is about to order an attack on. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde|Wheel")
	FText GetLatchedSubjectLabel() const;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Horde|Wheel")
	FGSOnOrderWheelOpenChanged OnOrderWheelOpenChanged;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Horde|Wheel")
	FGSOnOrderWheelHighlightChanged OnOrderWheelHighlightChanged;

	// ---------------------------------------------------------------- the live crosshair (#149)
	//
	// Michael: "make the reticle turn gold on a valid target", after "it's hard to attempt to aim a
	// command at something."
	//
	// The latch already answers "is this orderable" - it just only ran on the frame the wheel opened,
	// which is exactly one frame too late to help anybody aim. This runs the same resolution
	// continuously at a low rate and publishes the answer, so the HUD can colour the reticle before
	// the player commits to anything.

	/** True while the crosshair is over something an order could be given about. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde|Wheel")
	bool HasCrosshairTarget() const { return bHasCrosshairTarget; }

	/** Non-const return, for the UHT reason GSHordeSubsystem.h:139-142 sets out. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde|Wheel")
	AActor* GetCrosshairTarget() const { return CrosshairTarget.Get(); }

	/** Fires only on CHANGE, not per scan - a reticle that re-tints itself ten times a second is
	 *  work for nothing, and a delegate that fires constantly is one nobody can debug. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Horde|Wheel")
	FGSOnCrosshairTargetChanged OnCrosshairTargetChanged;

	/**
	 * Is this hit actor worth naming as the subject of an order, or is it scenery?
	 *
	 * Public and static because the server re-runs it on the client's nomination (see
	 * ServerIssueOrder) and because GS.Horde.Order needs the same answer without a wheel. Returns null
	 * for anything unorderable, which downgrades the order to location-only rather than rejecting it -
	 * "Hold, over there" is a perfectly good thing to want from a patch of grass.
	 */
	static AActor* ResolveOrderSubject(AActor* HitActor, const AActor* Asker);

	/**
	 * The shared "what is the player pointing at" trace, used by the wheel AND by GS.Horde.Order.
	 *
	 * A SWEEP, not a line, and that is the fix for the first thing Michael said after watching it:
	 * "there's no way to give it anything to attack - it says attack the bare ground". A line down
	 * the exact centre of the camera has to touch a capsule dead-on; at 20m, with a goblin-sized
	 * target and no reticle to aim by, it misses almost every time and quietly reports ground.
	 * Sweeping a sphere and preferring an orderable actor among ALL the hits means clipping a guard's
	 * shoulder counts as pointing at him.
	 *
	 * OutLocation is always filled on success. OutSubject may be null - that is a valid location-only
	 * result, which is what Hold wants.
	 */
	static bool TraceForOrder(UWorld* World, APawn* Asker, float Distance, float Radius,
		FVector& OutLocation, AActor*& OutSubject);

protected:
	/**
	 * The one network hop.
	 *
	 * Reliable, not unreliable: an order is a discrete intent rather than a stream, and a dropped one
	 * is a player-visible failure with no retry. Validation lives in the body rather than in a
	 * _Validate - a stale trace from a laggy client is an ordinary race, and disconnecting him for it
	 * (which is what returning false from _Validate does) is wildly the wrong response.
	 */
	UFUNCTION(Server, Reliable)
	void ServerIssueOrder(EGSHordeOrder InVerb, AActor* InSubject, FVector InLocation);

	/** Camera trace at the instant of the press. Fills LatchedLocation / LatchedSubject / bHasLatch. */
	bool LatchTargetUnderCamera();

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Wheel", meta = (ClampMin = "1.0"))
	float WheelDeadZone = 40.f;

	/** How far the latch trace reaches. Generous: you point at a guard across the hamlet. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Order", meta = (ClampMin = "100.0"))
	float OrderTraceDistance = 8000.f;

	/** Sweep radius for the latch. Wide enough that pointing NEAR a guard counts as pointing AT him -
	 *  there is no reticle on screen, so the player is aiming by feel. Too wide and you snag the
	 *  fence you meant to stand behind; 60 is about a goblin's shoulder width. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Order", meta = (ClampMin = "0.0"))
	float OrderTraceRadius = 60.f;

	/** Server-side sanity bound on a client-supplied location. Slightly above the trace distance so
	 *  an honest order at maximum range is never rejected by rounding. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Order", meta = (ClampMin = "100.0"))
	float MaxOrderRange = 10000.f;

	/**
	 * Seconds between crosshair scans. NOT per frame.
	 *
	 * 0.1 is about three frames at 30fps and is imperceptible on a reticle tint, while costing one
	 * sphere sweep ten times a second on ONE pawn - the locally controlled player. That is a rounding
	 * error next to the 4Hz broadphase every AI already runs in UGSAISteeringComponent, and it is the
	 * reason this is a throttled tick rather than a per-frame one.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Order", meta = (ClampMin = "0.0"))
	float CrosshairScanIntervalSeconds = 0.1f;

private:
	bool bWheelOpen = false;
	FVector2D WheelAccum = FVector2D::ZeroVector;
	EGSHordeOrder WheelHighlight = EGSHordeOrder::None;

	FVector LatchedLocation = FVector::ZeroVector;
	TWeakObjectPtr<AActor> LatchedSubject;
	bool bHasLatch = false;

	/** Live crosshair state (#149). Purely cosmetic and purely local - it never crosses the wire and
	 *  never touches an order. */
	bool bHasCrosshairTarget = false;
	TWeakObjectPtr<AActor> CrosshairTarget;
	float NextCrosshairScanTime = 0.f;

	void TickCrosshair();
};
